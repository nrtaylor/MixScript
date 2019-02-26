// WaveAudioSource - reads wav file into memory
// Author - Nic Taylor

#include "WavAudioSource.h"
#undef UNICODE // using single byte file loading routines
#include <windows.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <algorithm>
#include <stdint.h>
#include <memory>
#include <vector>
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

    struct WaveAudioBuffer {
        uint8_t* const samples;
        const uint32_t file_size;
        WaveAudioBuffer(uint8_t* const samples_, const uint32_t file_size_) :
            file_size(file_size_),
            samples(samples_) {
        }
    };

    uint32_t ByteRate(const WaveAudioFormat& format) {
        return format.bit_rate / 8;
    }

    float WaveAudioSource::Read() {
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

    void WaveAudioSource::MoveSelectedMarker(const int32_t num_samples) {
        if (selected_marker <= 0) {
            return;
        }
        const int32_t alignment = format.channels * ByteRate(format);
        const int32_t num_bytes = num_samples * alignment;
        const int selected_marker_index = selected_marker - 1;        
        cue_starts[selected_marker_index] += num_bytes;
        if (selected_marker_index > 0 &&
            cue_starts[selected_marker_index - 1] > cue_starts[selected_marker_index]) {
            std::swap(cue_starts[selected_marker_index - 1], cue_starts[selected_marker_index]);
            --selected_marker;
        }
        else if (selected_marker_index < cue_starts.size() - 1 &&
            cue_starts[selected_marker_index + 1] < cue_starts[selected_marker_index]) {
            std::swap(cue_starts[selected_marker_index + 1], cue_starts[selected_marker_index]);
            ++selected_marker;
        }        
    }

    void WaveAudioSource::AddMarker() {
        assert((read_pos - audio_start) % 4 == 0);
        auto it = cue_starts.begin();
        for (; it != cue_starts.end(); ++it) {
            if (*it > read_pos) {
                break;
            }
        }        
        it = cue_starts.insert(it, read_pos);
        selected_marker = 1 + (it - cue_starts.begin());
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
        uint8_t* const audio_start_pos_, uint8_t* const audio_end_pos_, const std::vector<uint32_t>& cue_offsets):
        file_name(file_path),
        format(format_),
        buffer(buffer_),
        audio_start(audio_start_pos_),
        audio_end(audio_end_pos_),
        selected_marker(-1) {
        read_pos = audio_start_pos_;
        last_read_pos = 0;
        write_pos = audio_start_pos_;
        for (const uint32_t cue_offset : cue_offsets) {
            cue_starts.push_back(audio_start + 4*cue_offset);
        }
        if (cue_starts.size() > 1) {
            mix_duration = static_cast<decltype(mix_duration)>(cue_starts.back() - cue_starts.front());
            mix_duration /= format.channels;
        }
        else {
            mix_duration = 0;
        }
    }

    // Returns true if on the cue
    bool WaveAudioSource::Cue(uint8_t const * const position, uint32_t& cue_id) const {
        cue_id = 0;
        for (uint8_t const * const cue_pos : cue_starts) {
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
            source.read_pos = source.cue_starts[cue_id - 1];
        }
        source.last_read_pos = static_cast<int32_t>(source.read_pos - source.audio_start);
        assert(source.last_read_pos.load() % 4 == 0);
    }

    void ResetToPos(WaveAudioSource& source, uint8_t const * const position) {
        source.read_pos = position;
        source.last_read_pos = static_cast<int32_t>(position - source.audio_start);
        assert(source.last_read_pos.load() % 4 == 0);
    }

    bool TrySelectMarker(WaveAudioSource& source, uint8_t const * const position, const int tolerance) {
        int index = 0;
        for (uint8_t const * const cue : source.cue_starts) {            
            if (abs(static_cast<int>(cue - position)) <= tolerance) {
                source.selected_marker = index + 1;
                return true;
            }
            ++index;
        }
        return false;
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

    Mixer::Mixer() : playing(nullptr), incoming(nullptr), selected_track(0) {
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
        return cue_starts[selected_marker - 1];
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

    void Mixer::UpdateGainValue(const float gain, const float interpolation_percent) {
        WaveAudioSource& source = Selected();
        const int cue_id = source.selected_marker;
        if (cue_id > 0) {
            uint8_t const * const marker_pos = source.cue_starts[cue_id - 1];
            int gain_cue_id = 0;
            bool found = false;
            for (Movement<GainParams>& movement : source.gain_control.movements) {
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
        if (movements.empty()) {
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
    }

    template<class T>
    void Mixer::Mix(T& output_writer, int samples_to_read) {

        WaveAudioSource& playing_ = *playing.get();
        WaveAudioSource& incoming_ = *incoming.get();

        const uint8_t* front = playing_.cue_starts.size() ? playing_.cue_starts[mix_sync.playing_cue_id - 1] : playing_.audio_start;
        const float inv_duration = 1.f / playing_.mix_duration;
        float left = 0;
        float right = 0;
        const bool make_mono = modifier_mono;
        for (int32_t i = 0; i < samples_to_read; ++i) {
            playing_.TryWrap();
            incoming_.TryWrap();
            uint32_t cue_id = 0;
            uint8_t const * const playing_read_pos = playing_.read_pos;
            bool on_cue = playing_.Cue(playing_read_pos, cue_id) && cue_id >= mix_sync.playing_cue_id;
            left = playing_.gain_control.Apply(playing_read_pos, playing_.Read());
            right = playing_.gain_control.Apply(playing_read_pos, playing_.Read());
            if (playing_.read_pos >= front) {
                uint8_t const * const incoming_read_pos = incoming_.read_pos;
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
                left = 0.5f * (left + right);
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
        for (uint8_t const * const cue_pos : source->cue_starts) {
            fs << "cues {\n";
            fs << "  pos: " << static_cast<int32_t>(cue_pos - source->audio_start) << "\n";
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
        std::vector<const uint8_t*> cue_starts;
        const uint32_t indent = 2;

        while (ParseStartBlock("cues", line)) {
            std::getline(fs, line);
            while (!ParseEndBlock(line)) {
                ParseParam("pos", line, cue_pos, indent);
                cue_starts.push_back(source.audio_start + std::stoi(cue_pos));
                std::getline(fs, line);
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
        const uint32_t playing_offset = playing->cue_starts.front() - playing->audio_start;

        const uint32_t incoming_size = incoming->audio_end - incoming->cue_starts.front();

        const uint32_t render_size = nMath::Max(playing_size, incoming_size + playing_offset);

        const uint32_t padding = 4;
        WaveAudioBuffer* wav_buffer = new WaveAudioBuffer(new uint8_t[render_size + padding], static_cast<uint32_t>(render_size));
        uint8_t* const audio_start = &wav_buffer->samples[0];
        WaveAudioSource* output_source = new WaveAudioSource("", playing->format, wav_buffer, audio_start,
            audio_start + render_size, {});

        MixScript::ResetToCue(playing, 0);
        MixScript::ResetToCue(incoming, 0);

        PCMOutputWriter output_writer = { output_source };        
        Mix(output_writer, render_size / (playing->format.channels * playing->format.bit_rate / 8));

        return output_source;
    }

    WaveAudioSource* ParseWaveFile(const char* file_path, WaveAudioBuffer* buffer) {
        WaveAudioFormat format;
        uint8_t* read_pos = buffer->samples;
        uint8_t* eof_pos = buffer->samples + buffer->file_size;

        uint8_t* audio_pos = nullptr;        
        uint32_t data_chunk_size = 0;
        assert(*(uint32_t*)read_pos == (uint32_t)('R' | ('I' << 8) | ('F' << 16) | ('F' << 24)));
        read_pos += 4 + 4 + 4; // skip past RIFF chunk

        std::vector<uint32_t> cues;
        while (read_pos < eof_pos - 8) {
            const uint32_t chunk_id = *(uint32_t*)read_pos;
            read_pos += 4;
            const uint32_t chunk_size = *(uint32_t*)read_pos;
            read_pos += 4;
            switch (chunk_id)
            {
            case (uint32_t)('f' | ('m' << 8) | ('t' << 16) | (' ' << 24)) :
            {
                uint8_t* format_pos = read_pos;
                const uint16_t format_tag = *(decltype(format_tag)*)format_pos;
                assert(format_tag == 0x0001);
                format_pos += sizeof(format_tag);
                const uint16_t channels = *(decltype(channels)*)format_pos;
                assert(channels == 1 || channels == 2);
                format.channels = static_cast<decltype(format.channels)>(channels);
                format_pos += sizeof(channels);
                const uint32_t sample_rate = *(decltype(sample_rate)*)format_pos;
                assert(sample_rate <= 96000);
                format.sample_rate = static_cast<decltype(format.sample_rate)>(sample_rate);
                format_pos += sizeof(sample_rate);

                format_pos += 6; // skip other fields

                const uint16_t bit_rate = *(decltype(bit_rate)*)format_pos;
                //assert(bit_rate == 16 || bit_rate == 24);
                assert(bit_rate == 16); // TODO: 24 bit
                format.bit_rate = static_cast<decltype(format.bit_rate)>(bit_rate);
                break;
            }                
            case (uint32_t)('d' | ('a' << 8) | ('t' << 16) | ('a' << 24)) :
                audio_pos = read_pos;
                data_chunk_size = chunk_size;                
                break;
            case (uint32_t)('c' | ('u' << 8) | ('e' << 16) | (' ' << 24)) :
            {
                // TODO
                uint8_t* cue_pos = read_pos;
                const uint32_t num_cues = *(decltype(num_cues)*)cue_pos;
                cue_pos += sizeof(num_cues);
                
                for (uint32_t i = 0; i < num_cues; ++i) {
                    cue_pos += 8; // skip ID and position
                    assert(*(uint32_t*)cue_pos == (uint32_t)('d' | ('a' << 8) | ('t' << 16) | ('a' << 24)));
                    cue_pos += 4; // skip chunk ID
                    assert(*(uint32_t*)cue_pos == 0);
                    cue_pos += 4; // skip chunk byte pos
                    assert(*(uint32_t*)cue_pos == 0);
                    cue_pos += 4; // skip block start
                    const uint32_t cue_sample = *(decltype(cue_sample)*)cue_pos;
                    assert(cues.size() == 0 || cues.back() < cue_sample);
                    cues.push_back(cue_sample);
                    cue_pos += sizeof(cue_sample);
                }
                break;
            }
            default:                
                break;
            }

            read_pos += chunk_size;
        }

        return new WaveAudioSource(file_path, format, buffer, audio_pos, audio_pos + data_chunk_size, cues);
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
            const float marker_ratio = marker_delta / (float)delta;
            const float cursor_pixel = pixel_width * marker_ratio;
            const float bytes_per_pixel = zoom_amount * delta / (float)pixel_width;
            const uint32_t cursor_screen_pos = cursor_pixel * bytes_per_pixel;
            const uint32_t cursor_alignment = cursor_screen_pos % (ByteRate(source.format) * source.format.channels);
            const uint32_t offset = marker_delta - (cursor_screen_pos - cursor_alignment);
            read_pos = read_pos + offset;
        }
        return read_pos;
    }

    uint32_t TrackVisualCache::SamplesPerPixel(const WaveAudioSource& source) const {
        const int32_t pixel_width = static_cast<int32_t>(peaks.peaks.size());
        const float zoom_amount = zoom_factor > 0 ? powf(2, -zoom_factor) : 1.f;
        const uint32_t delta = (source.audio_end - source.audio_start)/(ByteRate(source.format) * source.format.channels);
        const uint32_t samples_per_pixel = (uint32_t)(zoom_amount * delta / (float)pixel_width);

        return samples_per_pixel;
    }

    void ComputeParamAutomation(const WaveAudioSource& source, const uint32_t pixel_width, AmplitudeAutomation& automation,
        const int zoom_factor) {
        const float zoom_amount = zoom_factor > 0 ? powf(2, -zoom_factor) : 1.f;
        const uint32_t delta = source.audio_end - source.audio_start;
        const uint32_t bytes_per_pixel = (uint32_t)(zoom_amount * delta / (float)pixel_width);

        automation.values.resize(pixel_width);
        const uint8_t* read_pos = ZoomScrollOffsetPos(source, source.SelectedMarkerPos(), pixel_width, zoom_amount);
        for (float& value : automation.values) {
            value = source.gain_control.Apply(read_pos, 1.f);
            read_pos += bytes_per_pixel;
        }
    }

    const uint8_t* ComputeWavePeaks(const WaveAudioSource& source, const uint32_t pixel_width, WavePeaks& peaks,
        const int zoom_factor) {
        const float zoom_amount = zoom_factor > 0 ? powf(2, -zoom_factor) : 1.f;
        const uint32_t delta = source.audio_end - source.audio_start;
        const uint32_t sample_count = zoom_amount * delta / ByteRate(source.format);
        const float samples_per_pixel = sample_count / (float) pixel_width;

        peaks.peaks.resize(pixel_width);
        uint8_t const * const scroll_offset = ZoomScrollOffsetPos(source, source.SelectedMarkerPos(), pixel_width, zoom_amount);
        const uint8_t* read_pos = scroll_offset;
        float remainder = 0.f;
        const float remainder_amount = samples_per_pixel - floorf(samples_per_pixel);
        for (WavePeaks::WavePeak& peak : peaks.peaks) {
            peak.max = -FLT_MAX;
            peak.min = FLT_MAX;
            // TODO: Handle float to int
            int i = 0;
            if (remainder >= 1.f) {
                remainder -= 1.f;
                ++i;
                if (samples_per_pixel < 1.f) {
                    peak.max = peak.min = 0.f;
                }
            }
            for (; i < samples_per_pixel; ++i) {
                const float sample = source.Read(&read_pos);
                if (sample > peak.max) {
                    peak.max = sample;
                }
                if (sample < peak.min) {
                    peak.min = sample;
                }
            }
            remainder += remainder_amount;
        }
        return scroll_offset;
    }
}