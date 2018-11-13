// WaveAudioSource - reads wav file into memory
// Author - Nic Taylor

#pragma once
#include <vector>
#include <atomic>
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
        std::string file_name;
        std::unique_ptr<WaveAudioBuffer> buffer;        
        uint8_t* const audio_start;
        uint8_t* const audio_end;
        std::vector<uint8_t*> cue_starts;
        uint32_t mix_duration;

        const float kSampleRatio = 1.f / (float)((uint32_t)1 << (uint32_t)31);
        const uint8_t* read_pos;
        std::atomic_int32_t last_read_pos;
        uint8_t* write_pos;
        float Read();
        void Write(const float value);
        bool Cue(uint8_t const * const position, uint32_t& cue_id) const;
        void TryWrap();

        ~WaveAudioSource();

        WaveAudioSource(const char* file_path, const WaveAudioFormat& format_, WaveAudioBuffer* buffer_,
            uint8_t* const audio_start_pos_, uint8_t* const audio_end_pos_, const std::vector<uint32_t>& cue_offsets);
    };

    std::unique_ptr<WaveAudioSource> LoadWaveFile(const char* file_path);
    bool WriteWaveFile(const char* file_path, const std::unique_ptr<WaveAudioSource>& source);

    void ResetToCue(std::unique_ptr<WaveAudioSource>& source, const uint32_t cue_id);
    void ReadSamples(std::unique_ptr<WaveAudioSource>& source, float* left, float* right, int samples_to_read);

    struct FloatOutputWriter {
        float *left;
        float *right;

        void WriteLeft(const float left_) {
            *left = left_;
            ++left;
        }
        void WriteRight(const float right_) {
            *right = right_;
            ++right;
        }
    };

    struct PCMOutputWriter {
        WaveAudioSource* source;

        void WriteLeft(const float left_);
        void WriteRight(const float right_);
    };

    class Mixer {
    public:
        Mixer();

        template<class T>
        void Mix(T& output_writer, int samples_to_read);
        WaveAudioSource* Render();
        void Save(const char* file_path);
        void Load(const char* file_path);
        void LoadPlayingFromFile(const char* file_path);
        void LoadIncomingFromFile(const char* file_path);
        std::atomic_bool modifier_mono;

        void ResetToCue(const uint32_t cue_id);

        const WaveAudioSource* Playing() const { return playing.get(); }
    private:
        std::unique_ptr<WaveAudioSource> playing;
        std::unique_ptr<WaveAudioSource> incoming;
    };

    template void Mixer::Mix<FloatOutputWriter>(FloatOutputWriter& output_writer, int samples_to_read);
    template void Mixer::Mix<PCMOutputWriter>(PCMOutputWriter& output_writer, int samples_to_read);
}
