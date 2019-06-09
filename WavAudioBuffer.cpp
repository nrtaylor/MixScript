// WaveAudioBuffer - reads wav file into memory
// Author - Nic Taylor

#include "WavAudioBuffer.h"
#include <assert.h>

namespace MixScript {
    void ParseWaveFile(WaveAudioFormat *format, WaveAudioBuffer* buffer,
        std::vector<uint32_t>* cues, AudioRegion* region) {

        *region = { nullptr, nullptr };

        uint8_t* read_pos = buffer->samples;
        uint8_t* eof_pos = buffer->samples + buffer->file_size;

        uint8_t* audio_pos = nullptr;
        uint32_t data_chunk_size = 0;
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
                format->channels = static_cast<decltype(format->channels)>(channels);
                format_pos += sizeof(channels);
                const uint32_t sample_rate = *(decltype(sample_rate)*)format_pos;
                assert(sample_rate <= 96000);
                format->sample_rate = static_cast<decltype(format->sample_rate)>(sample_rate);
                format_pos += sizeof(sample_rate);

                format_pos += 6; // skip other fields

                const uint16_t bit_rate = *(decltype(bit_rate)*)format_pos;
                //assert(bit_rate == 16 || bit_rate == 24);
                assert(bit_rate == 16); // TODO: 24 bit
                format->bit_rate = static_cast<decltype(format->bit_rate)>(bit_rate);
                break;
            }
            case (uint32_t)('d' | ('a' << 8) | ('t' << 16) | ('a' << 24)) :
                audio_pos = read_pos;
                data_chunk_size = chunk_size;
                break;
            case (uint32_t)('c' | ('u' << 8) | ('e' << 16) | (' ' << 24)) :
            {
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
                    assert(cues->size() == 0 || cues->back() < cue_sample);
                    cues->push_back(cue_sample);
                    cue_pos += sizeof(cue_sample);
                }
                break;
            }
            default:
                break;
            }

            read_pos += chunk_size;
        }

        region->start = audio_pos;
        region->end = audio_pos + data_chunk_size;
    }
}