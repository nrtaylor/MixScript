// WaveAudioSource - reads wav file into memory
// Author - Nic Taylor

#include "WavAudioSource.h"
#undef UNICODE // using single byte file loading routines
#include <windows.h>
#define _USE_MATH_DEFINES
#include <math.h>
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
    const uint32_t kMaxAudioBufferSize = kMaxAudioEsimatedDuration * 48000 * 2 + 1024; // sample rate * channels

    struct WaveAudioBuffer {
        uint8_t* const samples;
        const uint32_t file_size;
        WaveAudioBuffer(uint8_t* const samples_, const uint32_t file_size_) :
            file_size(file_size_),
            samples(samples_) {
        }
    };

    float WaveAudioSource::Read() {
        const int32_t next = (int32_t)((*(const uint32_t*)read_pos) << 16);
        read_pos += 2;

        return next * kSampleRatio;
    }

    void WaveAudioSource::Write(const float value) {
        const float sample_max = (float)((1 << (format.bit_rate - 1)) - 1);        
        const int32_t next = (uint32_t)roundf(value * sample_max);
        memcpy(write_pos, (uint8_t*)&next, format.bit_rate);
        write_pos += 2; // TODO: Byte rate
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
        audio_end(audio_end_pos_) {
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

    enum MixFadeType : int32_t {
        MFT_LINEAR = 1,
        MFT_SQRT,
        MFT_TRIG,
        MFT_EXP,
    };

    Mixer::Mixer() : playing(nullptr), incoming(nullptr) {
        modifier_mono = false;
    }

    void Mixer::LoadPlayingFromFile(const char* file_path) {
        playing = std::unique_ptr<MixScript::WaveAudioSource>(std::move(MixScript::LoadWaveFile(file_path)));
        playing->gain_control.starting_state.gain = 1.f;
        if (incoming != nullptr) {
            MixScript::ResetToCue(incoming, 0);
        }
    }

    void Mixer::LoadIncomingFromFile(const char* file_path) {
        incoming = std::unique_ptr<MixScript::WaveAudioSource>(std::move(MixScript::LoadWaveFile(file_path)));
        incoming->gain_control.starting_state.gain = 0.f;
        if (playing != nullptr) {
            MixScript::ResetToCue(playing, 0);
        }
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
    }

    template<typename Params>
    float MixerControl<Params>::Apply(uint8_t const * const position, const float sample) {
        if (movements.empty()) {
            return starting_state.Apply(sample);
        }
        int interval = 0;
        for (const Movement<Params>& movement : movements) {
            if (position < movement.cue_pos) {                
                break;
            }
            ++interval;
        }
        if (interval > 0) {
            return movements[interval - 1].params.Apply(sample);
        }
        return starting_state.Apply(sample);

        // TODO: Interpolate
        //if (interval >= movements.size()) {
        //    return movements.back().params.Apply(sample);
        //}
        //if (interval == 0) {
        //    const float base_val = movements[i].params.Apply(sample) - starting_state.Apply(sample);
        //    // TODO: Starting state needs pos
        //    // TODO: Refactor flow
        //    const uint32_t delta = static_cast<decltype(delta)>(playing_.read_pos - front) / playing_.format.channels;
        //    const float inv_duration = 
        //    return base_val + InterpolateMix(base_val, , MFT_LINEAR);
        //}
    }

    template<class T>
    void Mixer::Mix(T& output_writer, int samples_to_read) {

        WaveAudioSource& playing_ = *playing.get();
        WaveAudioSource& incoming_ = *incoming.get();

        const uint8_t* front = playing_.cue_starts.front();
        const float inv_duration = 1.f / playing_.mix_duration;
        const float inv_cue_size = 1.f / playing_.cue_starts.size();
        float left = 0;
        float right = 0;
        const bool make_mono = modifier_mono;
        for (int32_t i = 0; i < samples_to_read; ++i) {
            playing_.TryWrap();
            incoming_.TryWrap();
            uint32_t cue_id = 0;
            bool on_cue = playing_.Cue(playing_.read_pos, cue_id);
            if (playing_.read_pos < front) {
                left = playing_.Read();
                right = playing_.Read();
            }
            else {
                const uint32_t delta = static_cast<decltype(delta)>(playing_.read_pos - front)/playing_.format.channels;
                // Continuous Mode
                //const float gain = InterpolateMix(delta, inv_duration, MFT_EXP);
                //incoming_.gain_control.Apply(incoming_.read_pos, incoming_.Read());
                const float gain = InterpolateMix(cue_id, inv_cue_size, MFT_SQRT);
                left = playing_.Read() + incoming_.Read() * gain;
                right = playing_.Read() + incoming_.Read() * gain;
            }            
            if (on_cue) {
                MixScript::ResetToCue(incoming, cue_id);
            }
            else if (incoming_.Cue(incoming_.read_pos, cue_id)) {
                MixScript::ResetToCue(playing, cue_id);
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
    }

    void SaveAudioSource(const WaveAudioSource* source, std::ofstream& fs) {
        for (uint8_t const * const cue_pos : source->cue_starts) {
            fs << "cues {\n";
            fs << "  pos: " << static_cast<int32_t>(cue_pos - source->audio_start) << "\n";
            fs << "}\n";
        }
    }

    void Mixer::Save(const char* file_path) {
        // TOOD: Switch to protobuf?
        std::ofstream fs(file_path);
        fs << "Playing: " << playing->file_name.c_str() << "\n";
        fs << "Incoming: " << incoming->file_name.c_str() << "\n";
        SaveAudioSource(playing.get(), fs);
        SaveAudioSource(incoming.get(), fs);
        fs.close();
    }

    bool ParseParam(const char* param_name, const std::string& line, std::string& value, const uint32_t indent = 0) {
        const auto param_name_len = strlen(param_name);
        if (line.length() <= param_name_len + 3 + indent) {
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
        std::string cue_pos;
        std::vector<uint8_t*> cue_starts;
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
    }

    void Mixer::Load(const char* file_path) {
        std::ifstream fs(file_path);
        std::string file_playing;
        std::string line;
        std::getline(fs, line);
        ParseParam("Playing", line, file_playing);
        LoadPlayingFromFile(file_playing.c_str());
        std::string file_incoming;
        std::getline(fs, line);
        ParseParam("Incoming", line, file_incoming);
        LoadIncomingFromFile(file_incoming.c_str());

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
}