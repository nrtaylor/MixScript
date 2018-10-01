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

    struct WaveAudioFormat {        
        uint32_t channels;
        uint32_t sample_rate;
        uint32_t bit_rate;
    };

    struct WaveAudioSource {
        std::unique_ptr<WaveAudioBuffer> buffer;
        WaveAudioFormat format;
        uint8_t* const audio;

        WaveAudioSource(WaveAudioBuffer* buffer_, uint8_t* const audio_start_pos_):
            buffer(buffer_),
            audio(audio_start_pos_){
        }
    };

    WaveAudioSource* ParseWaveFile(WaveAudioBuffer* buffer) {
        WaveAudioFormat format;
        uint8_t* read_pos = buffer->samples;
        uint8_t* eof_pos = buffer->samples + buffer->file_size;

        uint8_t* audio_pos = nullptr;        
        assert(*(uint32_t*)read_pos == (uint32_t)('R' | ('I' << 8) | ('F' << 16) | ('F' << 24)));
        read_pos += 4 + 4 + 4; // skip past RIFF chunk

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
                // store this chunk size somewhere
                break;
            case (uint32_t)('c' | ('u' << 8) | ('e' << 16) | (' ' << 24)) :
            {
                // TODO
                uint8_t* cue_pos = read_pos;
                const uint32_t num_cues = *(decltype(num_cues)*)cue_pos;
                cue_pos += sizeof(num_cues);

                std::vector<uint32_t> cues;
                for (uint32_t i = 0; i < num_cues; ++i) {
                    cue_pos += 8; // skip ID and position
                    assert(*(uint32_t*)cue_pos == (uint32_t)('d' | ('a' << 8) | ('t' << 16) | ('a' << 24)));
                    cue_pos += 4; // skip chunk ID
                    assert(*(uint32_t*)cue_pos == 0);
                    cue_pos += 4; // skip chunk byte pos
                    assert(*(uint32_t*)cue_pos == 0);
                    cue_pos += 4; // skip block start
                    const uint32_t cue_sample = *(decltype(cue_sample)*)cue_pos;
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

        return new WaveAudioSource(buffer, audio_pos);
    }

    WaveAudioSource* LoadWaveFile(const char* file_path) {

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
        return ParseWaveFile(wav_buffer);
    }

}