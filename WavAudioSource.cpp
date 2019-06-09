// WaveAudioSource - reads wav file into memory
// Author - Nic Taylor

#include "WavAudioSource.h"
#include "WavAudioBuffer.h"
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

namespace nMath {
    template <typename T>
    inline T Max(T lhs, T rhs)
    {
        return (lhs > rhs) ? lhs : rhs;
    }
}

namespace MixScript
{
    const uint32_t kMaxAudioEsimatedDuration = 10 * 60; // minutes
    const uint32_t kMaxAudioBufferSize = kMaxAudioEsimatedDuration * 48000 * 2 * 2 + 1024; // sample rate * byte_rate * channels

    uint32_t ByteRate(const WaveAudioFormat& format) {
        return format.bit_rate / 8;
    }

    float WaveAudioSource::Read() {
        if (read_pos >= audio_end) {
            return 0.f;
        }
        const int32_t next = (int32_t)((*(const uint32_t*)read_pos) << 16);
        read_pos += 2;

        return next * kSampleRatio;
    }

    float WaveAudioSource::Read(const uint8_t** read_pos_) const {
        const int32_t next = (int32_t)((*(const uint32_t*)(*read_pos_)) << 16);
        (*read_pos_) += 2;

        return next * kSampleRatio;
    }

    void WaveAudioSource::Write(const float value) {
        const float sample_max = (float)((1 << (format.bit_rate - 1)) - 1);        
        const int32_t next = (uint32_t)roundf(value * sample_max);
        memcpy(write_pos, (uint8_t*)&next, format.bit_rate);
        write_pos += 2; // TODO: Byte rate
    }

    int32_t WaveAudioSource::FindMarkerPivot(const int32_t marker_id) const {
        const int32_t marker_index = marker_id - 1;
        if (marker_index < 0 ||
            (cue_starts[marker_index].type != CT_IMPLIED &&
             cue_starts[marker_index].type != CT_DEFAULT)) {
            return marker_id;
        }        
        for (int32_t index = marker_index - 1; index >= 0; --index) {
            if (cue_starts[index].type == CT_LEFT_RIGHT ||
                cue_starts[index].type == CT_RIGHT) {
                return index + 1; // to id
            }
        }
        for (int32_t index = marker_index + 1; index < cue_starts.size(); ++index) {
            if (cue_starts[index].type == CT_LEFT_RIGHT ||
                cue_starts[index].type == CT_LEFT) {
                return index + 1; // to id
            }
        }
        return marker_id;
    }

    void WaveAudioSource::CorrectImpliedMarkers() {
        const int pivot_id = FindMarkerPivot(selected_marker);
        if (pivot_id == 0 || pivot_id >= (int)cue_starts.size() || selected_marker == pivot_id) {
            return;
        }
        const MixScript::Cue& pivot_cue = cue_starts[pivot_id - 1];
        uint8_t const * const start = pivot_cue.start;
        const int32_t delta = cue_starts[selected_marker - 1].start - start;
        const float new_delta = fabsf(static_cast<float>(delta) / (selected_marker - pivot_id));
        const int pivot_index = pivot_id - 1;        
        auto update_marker = [&, this](MixScript::Cue& cue, const int32_t index) -> bool {
            if (FindMarkerPivot(index + 1) == pivot_id) {
                if (int samples = static_cast<int>((index - pivot_index) * new_delta)) {
                    samples &= ~(0x04 - 1);
                    cue.start = start + samples;
                    return true;
                }
            }
            return false;
        };
        if (pivot_cue.type == CT_LEFT || pivot_cue.type == CT_LEFT_RIGHT) {
            for (int32_t index = pivot_index - 1; index >= 0; --index) {
                if (!update_marker(cue_starts[index], index)) {
                    break;
                }
            }
        }
        if (pivot_cue.type == CT_RIGHT || pivot_cue.type == CT_LEFT_RIGHT) {
            for (int32_t index = pivot_index + 1; index < cue_starts.size(); ++index) {
                if (!update_marker(cue_starts[index], index)) {
                    break;
                }
            }
        }
        const float samples_per_beat = new_delta / (4 * format.channels * ByteRate(format));
        bpm = 60.f * format.sample_rate / samples_per_beat;
    }

    void WaveAudioSource::MoveSelectedMarker(const int32_t num_samples) {
        if (selected_marker <= 0) {
            return;
        }
        const int32_t alignment = format.channels * ByteRate(format);
        const int32_t num_bytes = num_samples * alignment;
        const int selected_marker_index = selected_marker - 1;        
        cue_starts[selected_marker_index].start += num_bytes;
        if (cue_starts[selected_marker_index].type == CT_IMPLIED) {
            CorrectImpliedMarkers();
        }
        if (selected_marker_index > 0 &&
            cue_starts[selected_marker_index - 1].start > cue_starts[selected_marker_index].start) {
            std::swap(cue_starts[selected_marker_index - 1], cue_starts[selected_marker_index]);
            --selected_marker;
        }
        else if (selected_marker_index < cue_starts.size() - 1 &&
            cue_starts[selected_marker_index + 1].start < cue_starts[selected_marker_index].start) {
            std::swap(cue_starts[selected_marker_index + 1], cue_starts[selected_marker_index]);
            ++selected_marker;
        }        
    }

    void WaveAudioSource::AddMarker(const CueType type /*= CT_DEFAULT*/) {
        assert((read_pos - audio_start) % 4 == 0);
        auto it = cue_starts.begin();
        for (; it != cue_starts.end(); ++it) {
            if ((*it).start > read_pos) {
                break;
            }
        }
        const bool change_default = type == CT_DEFAULT && cue_starts.size() == 0;
        it = cue_starts.insert(it, { read_pos, change_default ? CT_LEFT_RIGHT : type });
        selected_marker = 1 + (it - cue_starts.begin());
    }

    void WaveAudioSource::UpdateMarker(const CueType type) {
        if (selected_marker <= 0) {
            return;
        }
        cue_starts[selected_marker - 1].type = type;
    }

    void WaveAudioSource::DeleteMarker() {
        if (selected_marker <= 0) {
            return;
        }
        const int selected_marker_index = selected_marker - 1;
        cue_starts.erase(cue_starts.begin() + selected_marker_index);
        if (selected_marker_index >= cue_starts.size()) {
            --selected_marker;
        }
    }

    void WaveAudioSource::TryWrap() {
        if (read_pos >= audio_end) {
            read_pos = audio_start;
        }
    }

    WaveAudioSource::~WaveAudioSource() {
        OutputDebugString("Destroying Source");
    }

    WaveAudioSource::WaveAudioSource(const char* file_path, const WaveAudioFormat& format_, WaveAudioBuffer* buffer_,
        const AudioRegion& region_, const std::vector<uint32_t>& cue_offsets):
        file_name(file_path),
        format(format_),
        buffer(buffer_),
        audio_start(region_.start),
        audio_end(region_.end),
        selected_marker(-1),
        bpm(-1.f) {
        read_pos = region_.start;
        last_read_pos = 0;
        write_pos = region_.start;
        for (const uint32_t cue_offset : cue_offsets) {
            cue_starts.push_back({ audio_start + 4 * cue_offset, CT_DEFAULT });
        }
        if (cue_starts.size()) {
            cue_starts.front().type = CT_LEFT_RIGHT;
        }
    }

    // Returns true if on the cue
    bool WaveAudioSource::Cue(uint8_t const * const position, uint32_t& cue_id) const {
        cue_id = 0;
        for (const MixScript::Cue& cue : cue_starts) {
            uint8_t const * const cue_pos = cue.start;
            ++cue_id;
            if (cue_pos == position) {
                return true;
            }
            else if (cue_pos > position) {
                --cue_id;
                break;
            }
        }
        return false;
    }

    void ResetToCue(std::unique_ptr<WaveAudioSource>& source_, const uint32_t cue_id) {
        WaveAudioSource& source = *source_.get();

        if (cue_id == 0) {
            source.read_pos = source.audio_start;
        }
        else if (cue_id <= source.cue_starts.size()) {
            source.read_pos = source.cue_starts[cue_id - 1].start;
        }
        source.last_read_pos = static_cast<int32_t>(source.read_pos - source.audio_start);
        assert(source.last_read_pos.load() % 4 == 0);
    }

    void ResetToPos(WaveAudioSource& source, uint8_t const * const position) {
        source.read_pos = position;
        source.last_read_pos = static_cast<int32_t>(position - source.audio_start);
        assert(source.last_read_pos.load() % 4 == 0);
    }

    MixScript::Cue* TrySelectMarker(WaveAudioSource& source, uint8_t const * const position, const int tolerance) {
        int index = 0;
        for (MixScript::Cue& cue : source.cue_starts) {                        
            if (abs(static_cast<int>(cue.start - position)) <= tolerance) {
                source.selected_marker = index + 1;
                return &cue;
            }
            ++index;
        }
        return nullptr;
    }

    void ReadSamples(std::unique_ptr<WaveAudioSource>& source_, float* left, float* right, int samples_to_read) {
        WaveAudioSource& source = *source_.get();

        for (int32_t i = 0; i < samples_to_read; ++i) {
            source.TryWrap();
            *left = source.Read();
            *right = source.Read();
            ++left;
            ++right;
        }
    }

    float DecibelToGain(const float db) {
        static const float log_10 = 2.302585f;
        return expf(db * log_10);
    }

    Mixer::Mixer() : playing(nullptr), incoming(nullptr), selected_track(0), use_marker(true) {
        modifier_mono = false;        
    }

    void Mixer::LoadPlayingFromFile(const char* file_path) {
        playing = std::unique_ptr<MixScript::WaveAudioSource>(std::move(MixScript::LoadWaveFile(file_path)));
        playing->gain_control.Add(GainParams{ 1.f }, playing->audio_start);
        if (incoming != nullptr) {
            MixScript::ResetToCue(incoming, 0);
        }
    }

    void Mixer::LoadIncomingFromFile(const char* file_path) {
        incoming = std::unique_ptr<MixScript::WaveAudioSource>(std::move(MixScript::LoadWaveFile(file_path)));
        incoming->gain_control.Add(GainParams{ 0.f }, incoming->audio_start);
        if (playing != nullptr) {
            MixScript::ResetToCue(playing, 0);
        }
    }

    const uint8_t * WaveAudioSource::SelectedMarkerPos() const {
        if (selected_marker <= 0) {
            return audio_start;
        }
        return cue_starts[selected_marker - 1].start;
    }

    float Mixer::GainValue(float& interpolation_percent) const {
        const WaveAudioSource& source = Selected();
        interpolation_percent = 0.0f;
        uint8_t const * const cue_pos = source.SelectedMarkerPos();
        const float gain = source.gain_control.Apply(cue_pos, 1.f);
        const auto& movements = source.gain_control.movements;
        auto it = std::find_if(movements.begin(), movements.end(),
            [&](const Movement<GainParams>& a) { return a.cue_pos == cue_pos; });
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

    template <typename Params>
    void MixerControl<Params>::ClearMovements(uint8_t const * const start, uint8_t const * const end) {
        if (!movements.empty()) {
            movements.erase(
                std::remove_if(movements.begin(), movements.end(), [&](const MixerControl::movement_type& movement) {
                return movement.cue_pos > start && movement.cue_pos <= end;
            }), movements.end());
        }
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
        switch (action_info.action)
        {
        case MixScript::SA_UPDATE_GAIN:
            UpdateGainValue(action_info.r_value, 1.f);
            break;
        case MixScript::SA_BYPASS_GAIN:
            if (Selected().gain_control.bypass != (bool)action_info.i_value) {
                Selected().gain_control.bypass = (bool)action_info.i_value;
                return;
            }
            break;
        case MixScript::SA_SET_RECORD:
            use_marker = !(bool)action_info.i_value;
            break;
        case MixScript::SA_RESET_AUTOMATION:            
            if (Selected().gain_control.movements.size() > 1) {
                auto& movements = Selected().gain_control.movements;
                movements.erase(movements.begin() + 1, movements.end());
            }
            break;
        case MixScript::SA_RESET_AUTOMATION_IN_REGION:
        {
            const Region region = CurrentRegion();            
            Selected().gain_control.ClearMovements(region.start, region.end);
        }
        break;
        default:
            break;
        }
    }

    void Mixer::UpdateGainValue(const float gain, const float interpolation_percent) {
        WaveAudioSource& source = Selected();    
        const int cue_id = source.selected_marker;
        if (cue_id > 0 || !use_marker) {
            uint8_t const * const marker_pos = use_marker ? source.cue_starts[cue_id - 1].start :
                (source.audio_start + source.last_read_pos);
            int gain_cue_id = 0;
            bool found = false;
            // TODO: Binary Search
            for (Movement<GainParams>& movement : source.gain_control.movements) {
                // TODO: Decide if it is easier to separate automation points from cues, or if
                // automation and cues should stay in sync.
                if (movement.cue_pos == marker_pos) {
                    movement.params.gain = gain;
                    movement.threshold_percent = interpolation_percent;
                    found = true;
                    break;
                }
                else if (movement.cue_pos > marker_pos) {
                    break;
                }
                ++gain_cue_id;
            }
            if (!found) {
                if (gain_cue_id >= source.gain_control.movements.size()) {
                    Movement<GainParams>& movement = source.gain_control.Add(GainParams{ gain }, marker_pos);
                    movement.threshold_percent = interpolation_percent;
                }
                else {
                    source.gain_control.movements.insert(source.gain_control.movements.begin() + gain_cue_id,
                        Movement<GainParams>{ GainParams{ gain }, MFT_LINEAR, interpolation_percent, marker_pos } );
                }
            }
        }
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
        auto& cues = source.cue_starts;
        cues.erase(std::remove_if(cues.begin(), cues.end(), [](const MixScript::Cue& cue) {
            return cue.type == CT_IMPLIED;
        }), cues.end());
    }

    void Mixer::GenerateImpliedMarkers() {
        ClearImpliedMarkers();
        WaveAudioSource& source = Selected();
        const int cue_start = source.selected_marker;
        if (cue_start <= 0) {
            return;
        }
        const int cue_end = MarkerRight();
        if (cue_end <= cue_start) {
            return;
        }
        auto& cues = source.cue_starts;
        uint8_t const * const original_pos = source.read_pos;
        source.read_pos = cues[cue_start - 1].start;
        uint8_t const * const read_end_cue = cues[cue_end - 1].start;
        const uint32_t delta = read_end_cue - cues[cue_start - 1].start;
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

    float InterpolateMix(const float param, const float inv_duration, const MixFadeType fade_type) {
        switch (fade_type) {
        case MFT_LINEAR:
            return param * inv_duration;
        case MFT_SQRT:
            return sqrtf(param * inv_duration);
        case MFT_TRIG:
            return sinf(param * inv_duration * M_PI_2);
        case MFT_EXP:
            return expf(param * inv_duration)/M_E;
        }

        return param;
    }

    template<typename Params>
    Movement<Params>& MixerControl<Params>::Add(Params&& params, uint8_t const * const position) {
        movements.emplace_back(Movement<Params>{ params, MFT_LINEAR, 0.f, position });
        return movements.back();
    }

    template<typename Params>
    float MixerControl<Params>::Apply(uint8_t const * const position, const float sample) const {
        if (movements.empty() ||
            bypass) {
            return sample;
        }
        int interval = movements.size();        
        for (auto it = movements.rbegin(); it != movements.rend(); ++it) {
            const Movement<Params>& movement = *it;
            if (position >= movement.cue_pos) {                
                break;
            }
            --interval;
        }
        if (interval == 0) {
            movements.front().params.Apply(sample);
        }
        else if (interval >= movements.size()) {
            return movements.back().params.Apply(sample);
        }
        else {
            const Movement<Params>& start_state = movements[interval - 1];
            const Movement<Params>& end_state = movements[interval];

            const float t = (float)(position - start_state.cue_pos);
            const float duration = (float)(end_state.cue_pos - start_state.cue_pos);

            const float start_value = start_state.params.Apply(sample);
            float ratio = t / duration;
            if (ratio < end_state.threshold_percent) {
                return start_value;
            }
            const float end_value = end_state.params.Apply(sample);

            // TODO: Clean up
            if (end_state.threshold_percent > 0.f) {
                uint8_t const * const start_pos = start_state.cue_pos +
                    static_cast<int32_t>(duration * end_state.threshold_percent);
                ratio = (float)(position - start_pos) / (float)(end_state.cue_pos - start_pos);
            }

            return InterpolateMix(end_value - start_value, ratio, end_state.interpolation_type) + start_value;
        }
        return sample;
    }

    template<class T>
    void Mixer::Mix(T& output_writer, int samples_to_read) {

        WaveAudioSource& playing_ = *playing.get();
        WaveAudioSource& incoming_ = *incoming.get();

        const uint8_t* front = playing_.cue_starts.size() ? playing_.cue_starts[mix_sync.playing_cue_id - 1].start : playing_.audio_start;        
        float left = 0;
        float right = 0;
        const bool make_mono = modifier_mono;
        for (int32_t i = 0; i < samples_to_read; ++i) {
            uint32_t cue_id = 0;
            uint8_t const * const playing_read_pos = playing_.read_pos;
            bool on_cue = playing_.Cue(playing_read_pos, cue_id) && cue_id >= mix_sync.playing_cue_id;
            left = playing_.gain_control.Apply(playing_read_pos, playing_.Read());
            right = playing_.gain_control.Apply(playing_read_pos, playing_.Read());
            if (playing_.read_pos >= front) {
                left += incoming_.gain_control.Apply(incoming_.read_pos, incoming_.Read());
                right += incoming_.gain_control.Apply(incoming_.read_pos, incoming_.Read());
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
                } else {
                    cue_type = CT_DEFAULT;
                }
                cue_starts.push_back({ source.audio_start + std::stoi(cue_pos), cue_type});                
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
        const uint32_t playing_size = playing->audio_end - playing->audio_start;
        const uint32_t playing_offset = playing->cue_starts.front().start - playing->audio_start;

        const uint32_t incoming_size = incoming->audio_end - incoming->cue_starts.front().start;

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

    WaveAudioSource* ParseWaveFile(const char* file_path, WaveAudioBuffer* buffer) {
        WaveAudioFormat format;
        std::vector<uint32_t> cues;
        AudioRegion region;

        ParseWaveFile(&format, buffer, &cues, &region);
        return new WaveAudioSource(file_path, format, buffer, region, cues);
    }

    std::unique_ptr<WaveAudioSource> LoadWaveFile(const char* file_path) {

        uint8_t* large_file_buffer = new uint8_t[kMaxAudioBufferSize];

        HANDLE file = CreateFile(file_path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
            nullptr);
        DWORD bytes_read = 0;
        if (file != INVALID_HANDLE_VALUE)
        {                        
            const bool read_entire_file = ReadFile(file, (LPVOID)large_file_buffer, kMaxAudioBufferSize - 1, &bytes_read, 
                nullptr) != 0;            
            CloseHandle(file);
        }

        WaveAudioBuffer* wav_buffer = new WaveAudioBuffer(large_file_buffer, static_cast<uint32_t>(bytes_read));
        return std::unique_ptr<WaveAudioSource>(ParseWaveFile(file_path, wav_buffer));
    }

    std::vector<uint8_t> ToByteBuffer(const std::unique_ptr<WaveAudioSource>& source) {
        
        const uint32_t data_size = static_cast<uint32_t>(source->audio_end - source->audio_start);
        const uint32_t file_size_minus_8 = 36 + data_size;

        std::vector<uint8_t> buffer; buffer.resize(file_size_minus_8 + 8);
        uint8_t* write_pos = &buffer[0];

        *(uint32_t*)write_pos = (uint32_t)('R' | ('I' << 8) | ('F' << 16) | ('F' << 24));
        write_pos += 4;

        *(uint32_t*)write_pos = file_size_minus_8;
        write_pos += 4;

        *(uint32_t*)write_pos = (uint32_t)('W' | ('A' << 8) | ('V' << 16) | ('E' << 24));
        write_pos += 4;

        *(uint32_t*)write_pos = (uint32_t)('f' | ('m' << 8) | ('t' << 16) | (' ' << 24));
        write_pos += 4;

        const uint32_t format_size = 16;
        *(uint32_t*)write_pos = format_size;
        write_pos += 4;

        const uint16_t format_tag = 1;
        *(uint16_t*)write_pos = format_tag;
        write_pos += 2;

        const uint16_t channels = static_cast<uint16_t>(source->format.channels);
        *(uint16_t*)write_pos = channels;
        write_pos += 2;

        const uint32_t sample_rate = source->format.sample_rate;
        *(uint32_t*)write_pos = sample_rate;
        write_pos += 4;

        const uint16_t bit_rate = static_cast<uint16_t>(source->format.bit_rate);
        const uint32_t byte_rate = sample_rate * channels * bit_rate / 8;
        *(uint32_t*)write_pos = byte_rate;
        write_pos += 4;
        
        const uint16_t block_align = static_cast<uint16_t>(channels * bit_rate / 8);
        *(uint16_t*)write_pos = block_align;
        write_pos += 2;
        
        *(uint16_t*)write_pos = bit_rate;
        write_pos += 2;

        *(uint32_t*)write_pos = (uint32_t)('d' | ('a' << 8) | ('t' << 16) | ('a' << 24));
        write_pos += 4;
        
        *(uint32_t*)write_pos = data_size;
        write_pos += 4;

        memcpy((void*)write_pos, (void*)source->audio_start, data_size);
        // pad 1 if odd num bytes
        return buffer;
    }

    bool WriteWaveFile(const char* file_path, const std::unique_ptr<WaveAudioSource>& source) {
        HANDLE file = CreateFile(file_path, GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (file != INVALID_HANDLE_VALUE) {            
            DWORD bytes_written = 0;
            std::vector<uint8_t> file_buffer(std::move(ToByteBuffer(source)));
            const DWORD byte_count = (DWORD)file_buffer.size();
            BOOL bWriteComplete = WriteFile(file, &file_buffer[0], byte_count, &bytes_written, nullptr);
            assert(bytes_written == byte_count);
            CloseHandle(file);
            return bWriteComplete;
        }

        return false;
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

    float TrackVisualCache::SamplesPerPixel(const WaveAudioSource& source) const {
        const int32_t pixel_width = static_cast<int32_t>(peaks.peaks.size());
        const float zoom_amount = zoom_factor > 0 ? powf(2, -zoom_factor) : 1.f;
        const uint32_t delta = (source.audio_end - source.audio_start)/(ByteRate(source.format) * source.format.channels);
        const float samples_per_pixel = zoom_amount * delta / (float)pixel_width;

        return samples_per_pixel;
    }

    void ComputeParamAutomation(const WaveAudioSource& source, const uint32_t pixel_width, AmplitudeAutomation& automation,
        const int zoom_factor, uint8_t const * const _scroll_offset) {
        const float zoom_amount = zoom_factor > 0 ? powf(2, -zoom_factor) : 1.f;
        const uint32_t delta = source.audio_end - source.audio_start;
        const float bytes_per_pixel = zoom_amount * delta / (float)pixel_width;

        automation.values.resize(pixel_width);
        uint8_t const * const scroll_offset = _scroll_offset != nullptr ? _scroll_offset :
            ZoomScrollOffsetPos(source, source.audio_start + source.last_read_pos, pixel_width, zoom_amount);
        const uint8_t* read_pos = scroll_offset;
        int i = 0;
        for (float& value : automation.values) {
            value = source.gain_control.Apply(read_pos, 1.f);
            ++i;
            read_pos = scroll_offset + (uint32_t)(i * bytes_per_pixel);
        }
    }

    const uint8_t* ComputeWavePeaks(const WaveAudioSource& source, const uint32_t pixel_width, WavePeaks& peaks,
        const int zoom_factor) {
        const float zoom_amount = zoom_factor > 0 ? powf(2, -zoom_factor) : 1.f;
        const uint32_t delta = source.audio_end - source.audio_start;
        const uint32_t sample_count = zoom_amount * delta / ByteRate(source.format);
        const float samples_per_pixel = sample_count / (float) (pixel_width * source.format.channels);

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
                    DerivativeFilter& active_filter = peaks.filters[channel % source.format.channels];
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