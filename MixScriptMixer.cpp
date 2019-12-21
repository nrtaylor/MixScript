// WaveAudioSource - reads wav file into memory
// Author - Nic Taylor

#include "MixScriptMixer.h"
#include "WavAudioBuffer.h"
#include "nMath.h"
#undef UNICODE // using single byte file loading routines
#include <windows.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <algorithm>
#include <stdint.h>
#include <memory>
#include <assert.h>

#include <iostream>
#include <fstream>
#include <string>

namespace MixScript
{
    inline float GainToDb(const float gain)
    {
        return 20.f * log10f(gain);
    }

    inline float DbToGain(const float db)
    {
        // return powf(10.f, db / 20.f);        
        constexpr float ln10_20 = 2.30258512f / 20.f;
        // 10^x = exp(ln(10)*x)
        return expf(db * ln10_20);
    }

    Mixer::Mixer() : playing(nullptr), incoming(nullptr), selected_track(0), update_param_on_selected_marker(false),
        selected_action(MixScript::SA_MULTIPLY_FADER_GAIN) {
        modifier_mono = false;
    }

    void Mixer::LoadPlaceholders() {
        playing = std::unique_ptr<MixScript::WaveAudioSource>(new MixScript::WaveAudioSource());
        incoming = std::unique_ptr<MixScript::WaveAudioSource>(new MixScript::WaveAudioSource());
    }

    void Mixer::LoadPlayingFromFile(const char* file_path) {
        playing = std::unique_ptr<MixScript::WaveAudioSource>(std::move(MixScript::LoadWaveFile(file_path)));
        playing->fader_control.Add(GainControl{ 1.f }, playing->audio_start);
        playing->gain_control.Add(GainControl{ 1.f }, playing->audio_start);
        if (incoming != nullptr) {
            MixScript::ResetToCue(incoming, 0);
        }
    }

    void Mixer::LoadIncomingFromFile(const char* file_path) {
        incoming = std::unique_ptr<MixScript::WaveAudioSource>(std::move(MixScript::LoadWaveFile(file_path)));
        incoming->fader_control.Add(GainControl{ 0.f }, incoming->audio_start);
        incoming->gain_control.Add(GainControl{ 1.f }, playing->audio_start);
        if (playing != nullptr) {
            MixScript::ResetToCue(playing, 0);
        }
    }

    float Mixer::FaderGainValue(float& interpolation_percent) const {
        const WaveAudioSource& source = Selected();
        interpolation_percent = 0.0f;
        uint8_t const * const cue_pos = source.SelectedMarkerPos();
        const float gain = source.fader_control.ValueAt(cue_pos);
        const auto& movements = source.fader_control.movements;
        auto it = std::find_if(movements.begin(), movements.end(),
            [&](const Movement& a) { return a.cue_pos == cue_pos; });
        if (it != movements.end()) {
            interpolation_percent = (*it).threshold_percent;
        }

        return gain;
    }

    Mixer::Region Mixer::CurrentRegion() const {
        const WaveAudioSource& source = Selected();
        Region region{ source.audio_start, source.audio_end };
        uint8_t const * const read_pos = source.audio_start + source.last_read_pos;
        for (const MixScript::Cue& cue : source.cue_starts) {
            if (cue.type == MixScript::CT_IMPLIED || cue.type == MixScript::CT_DEFAULT) {
                continue;
            }
            if (cue.start < read_pos) {
                region.start = cue.start;
            }
            if (cue.start >= read_pos) {
                region.end = cue.start;
                break;
            }
        }
        return region;
    }
    
    void Mixer::HandleAction(const SourceActionInfo& action_info) {
        actions.WriteAction(action_info);
    }

    void Mixer::ProcessActions() {
        actions.BeginRead();
        SourceActionInfo action_info(SA_NULL_ACTION);
        while (actions.ReadAction(action_info)) {
            DoAction(action_info);
        }
    }

    void Mixer::DoAction(const SourceActionInfo& action_info) {
        WaveAudioSource& target = action_info.explicit_target >= 0 ?
            (action_info.explicit_target ? *incoming.get() : *playing.get()) : Selected();
        auto& control = target.GetControl(selected_action);
        switch (action_info.action)
        {
        case MixScript::SA_UPDATE_GAIN:
            UpdateGainValue(target, action_info.r_value, 1.f);
            break;
        case MixScript::SA_MULTIPLY_FADER_GAIN:
        {
            const float minimuum_adjustment = DbToGain(0.5f) - 1.f;
            const float current_gain = target.fader_control.ValueAt(target.audio_start + target.last_read_pos);
            const float next_gain = current_gain + minimuum_adjustment * action_info.r_value;
            UpdateGainValue(target, nMath::Clamp(next_gain, 0.f, 1.f), 1.f);
        }
        break;
        case MixScript::SA_MULTIPLY_TRACK_GAIN:
        {
            const float current_gain = target.gain_control.ValueAt(target.audio_start + target.last_read_pos);
            float db = (current_gain > 0.f ? GainToDb(current_gain) : -96.f) + action_info.r_value;
            db = nMath::Clamp(db, -96.f, 12.f);
            //char debug_msg[256]; sprintf(&debug_msg[0], "Next db %.3f\n", db);
            //OutputDebugString(debug_msg);
            UpdateMovement(target, GainControl{ DbToGain(db) }, target.gain_control, 1.f,
                update_param_on_selected_marker, -1);
        }
        break;
        case MixScript::SA_MULTIPLY_LP_SHELF_GAIN:
        {
            const float current_gain = target.lp_shelf_control.ValueAt(target.audio_start + target.last_read_pos);
            float db = (current_gain > 0.f ? GainToDb(current_gain) : -96.f) + action_info.r_value;
            db = nMath::Clamp(db, -24.f, 6.f);
            // TODO: Replace index with uid
            target.lp_shelf_precomute.cache.emplace_back(fabsf(db) > 0.01 ?
                nMath::TwoPoleButterworthLowShelfConfig(FrequencyToPercent(target.format, 200.f), db) :
                nMath::TwoPoleNullConfig());            
            UpdateMovement(target, GainControl{ DbToGain(db) }, target.lp_shelf_control, 1.f,
                update_param_on_selected_marker, target.lp_shelf_precomute.cache.size() - 1);
        }
        break;
        case MixScript::SA_MULTIPLY_HP_SHELF_GAIN:
        {
            const float current_gain = target.hp_shelf_control.ValueAt(target.audio_start + target.last_read_pos);
            float db = (current_gain > 0.f ? GainToDb(current_gain) : -96.f) + action_info.r_value;
            db = nMath::Clamp(db, -24.f, 6.f);
            // TODO: Replace index with uid
            target.hp_shelf_precomute.cache.emplace_back(fabsf(db) > 0.01 ?
                nMath::TwoPoleButterworthHighShelfConfig(FrequencyToPercent(target.format, 3000.f), db) :
                nMath::TwoPoleNullConfig());
            UpdateMovement(target, GainControl{ DbToGain(db) }, target.hp_shelf_control, 1.f,
                update_param_on_selected_marker, target.hp_shelf_precomute.cache.size() - 1);
        }
        break;
        case MixScript::SA_BYPASS_GAIN:
            if (control.bypass != (action_info.i_value != 0)) {
                control.bypass = action_info.i_value != 0;
                return;
            }
            break;
        case MixScript::SA_SET_RECORD:
            update_param_on_selected_marker = !(action_info.i_value != 0);
            break;
        case MixScript::SA_RESET_AUTOMATION:
            if (control.movements.size() > 1) {
                auto& movements = control.movements;
                movements.erase(movements.begin() + 1, movements.end());                
            }
            break;
        case MixScript::SA_RESET_AUTOMATION_IN_REGION:
        {
            const Region region = CurrentRegion();
            control.ClearMovements(region.start, region.end);
        }
        break;
        case MixScript::SA_CUE_POSITION:
            MixScript::ResetToPos(target, action_info.position);
            break;
        case MixScript::SA_BYPASS:
            target.playback_bypass_all = !target.playback_bypass_all;
            break;
        case MixScript::SA_SOLO:
            target.playback_solo = !target.playback_solo;
            break;
        default:
            break;
        }
    }

    // TODO: Deprecate
    void Mixer::UpdateGainValue(WaveAudioSource& source, const float gain, const float interpolation_percent) {
        UpdateMovement(source, GainControl{ gain }, source.fader_control, interpolation_percent,
            update_param_on_selected_marker, -1);
    }

    WaveAudioSource& Mixer::Selected() {
        if (selected_track == 1) {
            return *incoming.get();
        }
        return *playing.get();
    }

    const WaveAudioSource& Mixer::Selected() const {
        if (selected_track == 1) {
            return *incoming.get();
        }
        return *playing.get();
    }

    int Mixer::MarkerLeft() const {
        const WaveAudioSource& source = Selected();
        int cue_id = source.selected_marker;
        if (--cue_id <= 0) {
            cue_id = static_cast<int>(source.cue_starts.size());
        }
        return cue_id;
    }

    int Mixer::MarkerRight() const {
        const WaveAudioSource& source = Selected();
        int cue_id = source.selected_marker;
        const int num_markers = static_cast<int>(source.cue_starts.size());
        if (++cue_id > num_markers) {
            cue_id = 1;
        }
        return cue_id;
    }

    void Mixer::SetSelectedMarker(int cue_id) {
        Selected().selected_marker = cue_id;
    }

    void Mixer::AddMarker() {
        playing->AddMarker();
        incoming->AddMarker();
    }

    void Mixer::DeleteMarker() {
        playing->DeleteMarker();
        incoming->DeleteMarker();
    }

    void Mixer::ClearImpliedMarkers() {
        WaveAudioSource& source = Selected();
        const int selected_marker = source.selected_marker;
        uint8_t const * const selected_marker_start = selected_marker > 0 ? source.cue_starts[source.selected_marker - 1].start : nullptr;
        int selected_marker_offset = 0;
        auto& cues = source.cue_starts;
        cues.erase(std::remove_if(cues.begin(), cues.end(), [selected_marker_start, &selected_marker_offset](const MixScript::Cue& cue) {
            if (cue.start < selected_marker_start && cue.type == CT_IMPLIED) {
                ++selected_marker_offset;
            }
            return cue.type == CT_IMPLIED;
        }), cues.end());
        if (selected_marker_offset > 0) {
            source.selected_marker -= selected_marker_offset;
            assert(source.selected_marker > 0);
        }
    }

    void Mixer::GenerateImpliedMarkers() {        
        WaveAudioSource& source = Selected();
        if (source.selected_marker <= 0) {
            return;
        }
        ClearImpliedMarkers();
        int cue_start = source.selected_marker;
        if (cue_start <= 0) {
            return;
        }
        int cue_end = MarkerRight();
        if (cue_end == cue_start) {
            return;
        }
        if (cue_end < cue_start) {
            std::swap(cue_end, cue_start);
        }
        auto& cues = source.cue_starts;
        uint8_t const * const original_pos = source.read_pos;
        source.read_pos = cues[cue_start - 1].start;
        uint8_t const * const read_end_cue = cues[cue_end - 1].start;
        const uint32_t delta = static_cast<uint32_t>(read_end_cue - cues[cue_start - 1].start);
        while (source.read_pos - source.audio_start > delta) {
            source.read_pos -= delta;
            source.AddMarker(CT_IMPLIED); // Will invalidate cue_start and cue_end
        }
        source.read_pos = read_end_cue;
        while (source.audio_end - source.read_pos > delta) {
            source.read_pos += delta;
            source.AddMarker(CT_IMPLIED);
        }
        source.read_pos = original_pos;
    }

    void Mixer::SeekSync() {
        switch (selected_track)
        {
        case 0:
            ResetToCue(mix_sync.playing_cue_id);
            break;
        case 1:
            ResetToCue(mix_sync.incoming_cue_id);
            break;
        default:
            break;
        }

    }

    void Mixer::AlignPlayingSyncToIncomingStart() {
        if (incoming->selected_marker < mix_sync.incoming_cue_id) {
            OutputDebugString("Selected incoming marker is invalid or less than the incoming sync point.");
            return;
        }
        if (incoming->cue_starts.size() < 2 || playing->cue_starts.size() < 2) {
            return;
        }
        const uint64_t delta = incoming->cue_starts[incoming->selected_marker - 1].start -
            incoming->cue_starts[mix_sync.incoming_cue_id - 1].start;
        const auto& playing_cues = playing->cue_starts;
        uint8_t const * const selected_pos = playing_cues[playing->selected_marker - 1].start;
        if ((uint64_t)(selected_pos - playing->audio_start) < delta) {
            mix_sync.playing_cue_id = 1;
            return;
        }
        uint8_t const * const sync_pos = selected_pos - delta;
        uint64_t best_delta = (uint64_t)(playing->audio_end - playing->audio_start);
        int playing_cue_id = 0;
        for (const MixScript::Cue& cue : playing_cues) {
            const uint64_t distance = sync_pos > cue.start ? sync_pos - cue.start : cue.start - sync_pos;
            if (distance <= best_delta) {
                best_delta = distance;
            }
            else {
                break;
            }
            ++playing_cue_id;
        }
        mix_sync.playing_cue_id = playing_cue_id;
    }

    void Mixer::SetMixSync() {
        switch (selected_track)
        {
        case 0:
            mix_sync.playing_cue_id = playing->selected_marker;
            break;
        case 1:
            mix_sync.incoming_cue_id = incoming->selected_marker;
            break;
        default:
            break;
        }
        ResetToCue(0);
    }
    
    template<class T>
    void Mixer::Mix(T& output_writer, int samples_to_read) {

        WaveAudioSource& playing_ = *playing.get();
        WaveAudioSource& incoming_ = *incoming.get();

        if (playing_.Empty() && incoming_.Empty()) {
            return;
        }

        float left = 0;
        float right = 0;
        const bool make_mono = modifier_mono;

        // TODO: Figure out template unresolved external bug preventing refactor.
        if (incoming_.playback_solo && (!playing_.playback_solo || playing_.Empty())) {
            for (int32_t i = 0; i < samples_to_read; ++i) {
                left = incoming_.ReadAndProcess(0);
                // Second read should use first read pos for automation calculation
                right = incoming_.ReadAndProcess(1);
                if (make_mono) {
                    left = 0.707f * (left + right);
                    right = left;
                }
                output_writer.WriteLeft(left);
                output_writer.WriteRight(right);
            }
            incoming_.last_read_pos = static_cast<int32_t>(incoming_.read_pos - incoming_.audio_start);
            return;
        }
        else if (playing_.playback_solo && (!incoming_.playback_solo || incoming_.Empty())) {
            for (int32_t i = 0; i < samples_to_read; ++i) {
                left = playing_.ReadAndProcess(0);
                // Second read should use first read pos for automation calculation
                right = playing_.ReadAndProcess(1);
                if (make_mono) {
                    left = 0.707f * (left + right);
                    right = left;
                }
                output_writer.WriteLeft(left);
                output_writer.WriteRight(right);
            }
            playing_.last_read_pos = static_cast<int32_t>(playing_.read_pos - playing_.audio_start);
            return;
        }
        else {
            const uint8_t* front = playing_.cue_starts.size() ? playing_.cue_starts[mix_sync.playing_cue_id - 1].start :
                playing_.audio_start;
            for (int32_t i = 0; i < samples_to_read; ++i) {
                uint32_t cue_id = 0;
                // TODO: look up next cue instead of per sample.
                bool on_cue = playing_.Cue(playing_.read_pos, cue_id) && (int)cue_id >= mix_sync.playing_cue_id;
                left = playing_.ReadAndProcess(0);
                // Second read should use first read pos for automation calculation
                right = playing_.ReadAndProcess(1);
                if (playing_.read_pos >= front) {
                    left += incoming_.ReadAndProcess(0);
                    right += incoming_.ReadAndProcess(1);
                }
                if (on_cue) {
                    MixScript::ResetToCue(incoming, (uint32_t)((int32_t)cue_id + mix_sync.Delta()));
                }
                else if (incoming_.Cue(incoming_.read_pos, cue_id)) {
                    MixScript::ResetToCue(playing, (uint32_t)((int32_t)cue_id + mix_sync.Reverse()));
                }
                if (make_mono) {
                    left = 0.707f * (left + right);
                    right = left;
                }
                output_writer.WriteLeft(left);
                output_writer.WriteRight(right);
            }
            playing_.last_read_pos = static_cast<int32_t>(playing_.read_pos - playing_.audio_start);
            incoming_.last_read_pos = static_cast<int32_t>(incoming_.read_pos - incoming_.audio_start);
        }
    }

    void Mixer::ResetToCue(const uint32_t cue_id) {
        MixScript::ResetToCue(playing, cue_id);
        if (cue_id == 0) { // TODO: This will lead to marker bugs.
            MixScript::ResetToCue(incoming, cue_id);
        }
    }

    void SaveAudioSource(const WaveAudioSource* source, std::ofstream& fs) {
        fs << "audio_source {\n";
        for (const MixScript::Cue& cue : source->cue_starts) {
            uint8_t const * const cue_pos = cue.start;
            fs << "cues {\n";
            fs << "  pos: " << static_cast<int32_t>(cue_pos - source->audio_start) << "\n";
            fs << "  type: " << static_cast<int32_t>(cue.type) << "\n";
            fs << "}\n";
        }
        fs << "}\n";
    }

    void Mixer::Save(const char* file_path) {
        // TOOD: Switch to protobuf?
        std::ofstream fs(file_path);
        fs << "Playing: " << playing->file_name.c_str() << "\n";
        fs << "Incoming: " << incoming->file_name.c_str() << "\n";
        fs << "mix_sync {\n";
        fs << "  playing_cue_id: " << mix_sync.playing_cue_id << "\n";
        fs << "  incoming_cue_id: " << mix_sync.incoming_cue_id << "\n";
        fs << "}\n";
        SaveAudioSource(playing.get(), fs);
        SaveAudioSource(incoming.get(), fs);
        fs.close();
    }

    bool ParseParam(const char* param_name, const std::string& line, std::string& value, const uint32_t indent = 0) {
        const auto param_name_len = strlen(param_name);
        if (line.length() < param_name_len + 3 + indent) {
            return false;
        }
        if (line.compare(indent, param_name_len, param_name) == 0 &&
            line[param_name_len + indent] == ':') {
            value = line.substr(param_name_len + indent + 2);
            return true;
        }
        return false;
    }

    bool ParseStartBlock(const char* param_name, const std::string& line) {
        const auto param_name_len = strlen(param_name);
        if (line.length() < param_name_len + 2) {
            return false;
        }
        return line.compare(0, param_name_len, param_name) == 0 &&
            line[param_name_len + 1] == '{';
    }

    bool ParseEndBlock(const std::string& line) {
        return line[0] == '}';
    }

    void LoadAudioSource(std::ifstream& fs, WaveAudioSource& source) {
        std::string line;
        std::getline(fs, line);
        ParseStartBlock("audio_source", line);
        std::getline(fs, line);
        std::string cue_pos;
        std::string cue_type_param;
        CueType cue_type;
        std::vector<Cue> cue_starts;
        const uint32_t indent = 2;

        while (ParseStartBlock("cues", line)) {
            std::getline(fs, line);
            while (!ParseEndBlock(line)) {
                ParseParam("pos", line, cue_pos, indent);
                std::getline(fs, line);
                if (ParseParam("type", line, cue_type_param, indent)) {
                    cue_type = static_cast<CueType>(std::stoi(cue_type_param));
                    std::getline(fs, line);
                }
                else {
                    cue_type = CT_DEFAULT;
                }
                cue_starts.push_back({ source.audio_start + std::stoi(cue_pos), cue_type });
            }
            std::getline(fs, line);
        }
        source.cue_starts = std::move(cue_starts);
        ParseEndBlock(line);
    }

    void Mixer::Load(const char* file_path) {
        std::ifstream fs(file_path);
        std::string file_playing;
        std::string line;
        std::string param;

        std::getline(fs, line);
        ParseParam("Playing", line, file_playing);
        LoadPlayingFromFile(file_playing.c_str());
        std::string file_incoming;
        std::getline(fs, line);
        ParseParam("Incoming", line, file_incoming);
        LoadIncomingFromFile(file_incoming.c_str());

        std::getline(fs, line);
        ParseStartBlock("mix_sync", line);
        std::getline(fs, line);
        ParseParam("playing_cue_id", line, param, 2);
        mix_sync.playing_cue_id = std::stoi(param);
        std::getline(fs, line);
        ParseParam("incoming_cue_id", line, param, 2);
        mix_sync.incoming_cue_id = std::stoi(param);
        std::getline(fs, line);
        ParseEndBlock(line);

        // TODO: Audio Source needs to be scoped. Reading all the cues into playing
        LoadAudioSource(fs, *playing.get());
        LoadAudioSource(fs, *incoming.get());
    }

    void PCMOutputWriter::WriteLeft(const float left_) {
        source->Write(left_);
    }
    void PCMOutputWriter::WriteRight(const float right_) {
        source->Write(right_);
    }

    WaveAudioSource* Mixer::Render() {
        const uint32_t playing_size = static_cast<uint32_t>(playing->audio_end - playing->audio_start);
        const uint32_t playing_offset = static_cast<uint32_t>(playing->cue_starts.front().start - playing->audio_start);
        const uint32_t incoming_size = static_cast<uint32_t>(incoming->audio_end - incoming->cue_starts.front().start);
        const uint32_t render_size = nMath::Max(playing_size, incoming_size + playing_offset);

        const uint32_t padding = 4;
        WaveAudioBuffer* wav_buffer = new WaveAudioBuffer(new uint8_t[render_size + padding], static_cast<uint32_t>(render_size));
        uint8_t* const audio_start = &wav_buffer->samples[0];
        WaveAudioSource* output_source = new WaveAudioSource("", playing->format, wav_buffer, AudioRegion{ audio_start,
            audio_start + render_size }, {});

        MixScript::ResetToCue(playing, 0);
        MixScript::ResetToCue(incoming, 0);

        PCMOutputWriter output_writer = { output_source };
        Mix(output_writer, render_size / (playing->format.channels * playing->format.bit_rate / 8));

        return output_source;
    }

    float TrackVisualCache::SamplesPerPixel(const WaveAudioSource& source) const {
        const int32_t pixel_width = static_cast<int32_t>(peaks.peaks.size());
        const float zoom_amount = zoom_factor > 0 ? powf(2, -zoom_factor) : 1.f;
        const uint32_t delta = (source.audio_end - source.audio_start) / (ByteRate(source.format) * source.format.channels);
        const float samples_per_pixel = zoom_amount * delta / (float)pixel_width;

        return samples_per_pixel;
    }

    const uint8_t* ZoomScrollOffsetPos(const WaveAudioSource& source, uint8_t const * const cursor_pos,
        const uint32_t pixel_width, const float zoom_amount) {
        const uint8_t* read_pos = source.audio_start;
        if (cursor_pos != read_pos && zoom_amount < 1.f) {
            // TODO: Clean-up
            const uint32_t delta = source.audio_end - source.audio_start;
            const uint32_t marker_delta = cursor_pos - source.audio_start;
            const float center_pixel = pixel_width * 0.5f;
            const float bytes_per_pixel = zoom_amount * delta / (float)pixel_width;
            uint32_t cursor_screen_pos = center_pixel * bytes_per_pixel;
            const uint32_t cursor_alignment = cursor_screen_pos % (ByteRate(source.format) * source.format.channels);
            cursor_screen_pos -= cursor_alignment;
            if (cursor_screen_pos < marker_delta) {
                if (delta > marker_delta + cursor_screen_pos) {
                    const uint32_t offset = marker_delta - cursor_screen_pos;
                    read_pos = read_pos + offset;
                }
                else {
                    const uint32_t bytes_pixel_width = 2 * cursor_screen_pos;
                    const uint32_t offset = delta - bytes_pixel_width;
                    read_pos = read_pos + offset;
                }
            }
        }
        return read_pos;
    }

    void ComputeParamAutomation(const WaveAudioSource& source, const uint32_t pixel_width, AmplitudeAutomation& automation,
        const int zoom_factor, uint8_t const * const _scroll_offset, const MixScript::SourceAction selected_action) {
        const float zoom_amount = zoom_factor > 0 ? powf(2, -zoom_factor) : 1.f;
        const uint32_t delta = source.audio_end - source.audio_start;
        const float bytes_per_pixel = zoom_amount * delta / (float)pixel_width;

        automation.values.resize(pixel_width);
        uint8_t const * const scroll_offset = _scroll_offset != nullptr ? _scroll_offset :
            ZoomScrollOffsetPos(source, source.audio_start + source.last_read_pos, pixel_width, zoom_amount);
        const MixerControl* control = &source.GetControl(selected_action);
        const uint8_t* read_pos = scroll_offset;
        int i = 0;
        for (float& value : automation.values) {
            value = control->ValueAt(read_pos);
            // Center around 1.f
            if (selected_action == MixScript::SA_MULTIPLY_TRACK_GAIN) {
                if (value > 1.f) {
                    // Max value is +12 db or 4.f gain.
                    value = (value - 1.f) * 0.3333f + 1.f;
                }
                value *= 0.5f;
            }
            else if (selected_action == MixScript::SA_MULTIPLY_LP_SHELF_GAIN ||
                selected_action == MixScript::SA_MULTIPLY_HP_SHELF_GAIN) {
                value *= 0.5f;
            }
            ++i;
            read_pos = scroll_offset + (uint32_t)(i * bytes_per_pixel);
        }
    }

    const uint8_t* ComputeWavePeaks(const WaveAudioSource& source, const uint32_t pixel_width, WavePeaks& peaks,
        const int zoom_factor) {
        const float zoom_amount = zoom_factor > 0 ? powf(2, -zoom_factor) : 1.f;
        const uint32_t delta = source.audio_end - source.audio_start;
        const uint32_t sample_count = static_cast<uint32_t>(zoom_amount * delta / ByteRate(source.format));
        const float samples_per_pixel = sample_count / (float)(pixel_width * source.format.channels);

        peaks.peaks.resize(pixel_width);
        uint8_t const * const scroll_offset = ZoomScrollOffsetPos(source, source.audio_start + source.last_read_pos,
            pixel_width, zoom_amount);
        const uint8_t* read_pos = scroll_offset;
        float remainder = 0.f;
        const float remainder_amount = samples_per_pixel - floorf(samples_per_pixel);
        int channel = 0;
        const int spp = static_cast<int>(samples_per_pixel);

        for (WavePeaks::WavePeak& peak : peaks.peaks) {
            peak.max = -FLT_MAX;
            peak.min = FLT_MAX;
            peak.start = read_pos;
            int i = 0;
            if (remainder >= 1.f) {
                remainder -= 1.f;
                --i;
                if (samples_per_pixel < 1.f) {
                    peak.max = peak.min = 0.f;
                }
            }
            for (; i < spp; ++i, ++channel) {
                for (uint32_t c = 0; c < source.format.channels; ++c) {
                    nMath::DerivativeFilter& active_filter = peaks.filters[channel % source.format.channels];
                    const float sample = active_filter.Compute(source.Read(&read_pos));
                    if (sample > peak.max) {
                        peak.max = sample;
                    }
                    if (sample < peak.min) {
                        peak.min = sample;
                    }
                }
            }
            remainder += remainder_amount;
            peak.end = read_pos;
        }
        return scroll_offset;
    }
}