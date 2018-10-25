// WaveAudioSource - reads wav file into memory
// Author - Nic Taylor

#pragma once
#include <vector>
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
        WaveAudioFormat format;
        std::unique_ptr<WaveAudioBuffer> buffer;        
        uint8_t* const audio_start;
        uint8_t* const audio_end;
        std::vector<uint8_t*> cue_starts;

        const float kSampleRatio = 1.f / (float)((uint32_t)1 << (uint32_t)31);
        const uint8_t* read_pos;
        float Read();
        int32_t Cue(uint8_t const * const position) const;
        void TryWrap();

        ~WaveAudioSource();

        WaveAudioSource(const WaveAudioFormat& format_, WaveAudioBuffer* buffer_, uint8_t* const audio_start_pos_,
            uint8_t* const audio_end_pos_, const std::vector<uint32_t>& cue_offsets);
    };

    std::unique_ptr<WaveAudioSource> LoadWaveFile(const char* file_path);
    bool WriteWaveFile(const char* file_path, const std::unique_ptr<WaveAudioSource>& source);

    void ResetToCue(std::unique_ptr<WaveAudioSource>& source, const uint32_t cue_id);
    void ReadSamples(std::unique_ptr<WaveAudioSource>& source, float* left, float* right, int samples_to_read);
    void Mix(std::unique_ptr<WaveAudioSource>& playing, std::unique_ptr<WaveAudioSource>& incoming,
        float* left, float* right, int samples_to_read);
}
