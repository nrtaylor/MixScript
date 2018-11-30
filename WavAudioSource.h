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

    struct GainParams {
        float gain;

        float Apply(const float sample) const {
            return sample * gain;
        }
    };

    template<typename Params>
    struct Movement {
        Params params;
        uint8_t* cue_pos;
    };

    template<typename Params>
    struct MixerControl {
        Params starting_state;
        std::vector<Movement<Params> > movements;

        float Apply(uint8_t const * const position, const float sample);
    };

    struct WavePeaks {
        struct WavePeak {
            float min;
            float max;
        };
        std::vector<WavePeak> peaks;
    };

    struct WaveAudioSource {
        WaveAudioFormat format;
        std::string file_name;
        std::unique_ptr<WaveAudioBuffer> buffer;        
        uint8_t* const audio_start;
        uint8_t* const audio_end;
        std::vector<uint8_t*> cue_starts;
        MixerControl<GainParams> gain_control;
        uint32_t mix_duration;

        const float kSampleRatio = 1.f / (float)((uint32_t)1 << (uint32_t)31);
        const uint8_t* read_pos;
        std::atomic_int32_t last_read_pos;
        uint8_t* write_pos;
        float Read();
        float Read(const uint8_t** read_pos_) const;
        void Write(const float value);
        bool Cue(uint8_t const * const position, uint32_t& cue_id) const;
        void TryWrap();

        ~WaveAudioSource();

        WaveAudioSource(const char* file_path, const WaveAudioFormat& format_, WaveAudioBuffer* buffer_,
            uint8_t* const audio_start_pos_, uint8_t* const audio_end_pos_, const std::vector<uint32_t>& cue_offsets);
    };

    void ComputeWavePeaks(const WaveAudioSource& source, const uint32_t pixel_width, WavePeaks& peaks);

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

    struct MixSync {
        int playing_cue_id = 1;
        int incoming_cue_id = 1;

        int Delta() const {
            return incoming_cue_id - playing_cue_id;
        }
        int Reverse() const {
            return playing_cue_id - incoming_cue_id;
        }
    };

    //Select Control[Gain | Low | Mid | High...]
    //Select Cue[1..9]
    //Select Track[O | I]
    //Set Focus on Slider
    //Optional : Set interpolation mode
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
        MixSync mix_sync;
        int selected_track;

        void ResetToCue(const uint32_t cue_id);

        const WaveAudioSource* Playing() const { return playing.get(); }
        const WaveAudioSource* Incoming() const { return incoming.get(); }

        void SetMixSync(int cue_id);
    private:
        std::unique_ptr<WaveAudioSource> playing;
        std::unique_ptr<WaveAudioSource> incoming;
    };

    template void Mixer::Mix<FloatOutputWriter>(FloatOutputWriter& output_writer, int samples_to_read);
    template void Mixer::Mix<PCMOutputWriter>(PCMOutputWriter& output_writer, int samples_to_read);
}
