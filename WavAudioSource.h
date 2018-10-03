// WaveAudioSource - reads wav file into memory
// Author - Nic Taylor

#pragma once
#include <memory>

namespace MixScript
{
    struct WaveAudioBuffer;

    struct WaveAudioFormat {
        uint32_t channels;
        uint32_t sample_rate;
        uint32_t bit_rate;
    };

    struct WaveAudioSource {
        std::unique_ptr<WaveAudioBuffer> buffer;
        WaveAudioFormat format;
        uint8_t* const audio_start;
        uint8_t* const audio_end;

        const float kSampleRatio = 1.f / (float)((uint32_t)1 << (uint32_t)31);
        const uint8_t* read_pos;
        float Read();
        void TryWrap();

        ~WaveAudioSource();

        WaveAudioSource(WaveAudioBuffer* buffer_, uint8_t* const audio_start_pos_, uint8_t* const audio_end_pos_);
    };

    std::unique_ptr<WaveAudioSource> LoadWaveFile(const char* file_path);

    void ReadSamples(std::unique_ptr<WaveAudioSource>& source, float* left, float* right, int samples_to_read);
}
