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

    enum MixFadeType : int32_t {
        MFT_LINEAR = 1,
        MFT_SQRT,
        MFT_TRIG,
        MFT_EXP,
    };

    template<typename Params>
    struct Movement {
        Params params;
        MixFadeType interpolation_type;
        float threshold_percent;
        const uint8_t* cue_pos;
    };

    template<typename Params>
    struct MixerControl {        
        std::vector<Movement<Params> > movements;

        Movement<Params>& Add(Params&& params, uint8_t const * const position);
        float Apply(uint8_t const * const position, const float sample) const;
    };

    struct WavePeaks {
        struct WavePeak {
            float min;
            float max;
        };
        std::atomic_bool dirty;
        std::vector<WavePeak> peaks;

        WavePeaks() {
            dirty = false;
        }
    };

    struct AmplitudeAutomation {
        std::atomic_bool dirty;
        std::vector<float> values;

        AmplitudeAutomation() {
            dirty = false;
        }
    };
    struct WaveAudioSource {
        WaveAudioFormat format;
        std::string file_name;
        std::unique_ptr<WaveAudioBuffer> buffer;        
        uint8_t* const audio_start;
        uint8_t* const audio_end;
        std::vector<const uint8_t*> cue_starts;
        MixerControl<GainParams> gain_control;
        uint32_t mix_duration;
        int selected_marker;

        const float kSampleRatio = 1.f / (float)((uint32_t)1 << (uint32_t)31);
        const uint8_t* read_pos;
        std::atomic_int32_t last_read_pos;
        uint8_t* write_pos;
        float Read();
        float Read(const uint8_t** read_pos_) const;
        void Write(const float value);
        bool Cue(uint8_t const * const position, uint32_t& cue_id) const;
        const uint8_t * SelectedMarkerPos() const;
        void TryWrap();
        void AddMarker();
        void DeleteMarker();
        void MoveSelectedMarker(const int32_t num_samples);

        ~WaveAudioSource();

        WaveAudioSource(const char* file_path, const WaveAudioFormat& format_, WaveAudioBuffer* buffer_,
            uint8_t* const audio_start_pos_, uint8_t* const audio_end_pos_, const std::vector<uint32_t>& cue_offsets);
    };

    const uint8_t* ComputeWavePeaks(const WaveAudioSource& source, const uint32_t pixel_width, WavePeaks& peaks,
        const int zoom_factor);
    void ComputeParamAutomation(const WaveAudioSource& source, const uint32_t pixel_width, AmplitudeAutomation& automation,
        const int zoom_factor);

    std::unique_ptr<WaveAudioSource> LoadWaveFile(const char* file_path);
    bool WriteWaveFile(const char* file_path, const std::unique_ptr<WaveAudioSource>& source);

    void ResetToCue(std::unique_ptr<WaveAudioSource>& source, const uint32_t cue_id);
    void ResetToPos(WaveAudioSource& source, uint8_t const * const position);
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

        WaveAudioSource& Selected();
        const WaveAudioSource& Selected() const;

        void ResetToCue(const uint32_t cue_id);

        const WaveAudioSource* Playing() const { return playing.get(); }
        const WaveAudioSource* Incoming() const { return incoming.get(); }        

        int MarkerLeft() const;
        int MarkerRight() const;

        void SetSelectedMarker(int cue_id);
        void UpdateGainValue(const float gain, const float interpolation_percent);
        float GainValue(float& interpolation_percent) const;
        void SetMixSync();
        void AddMarker();
        void DeleteMarker();
    private:

        std::unique_ptr<WaveAudioSource> playing;
        std::unique_ptr<WaveAudioSource> incoming;
    };

    template void Mixer::Mix<FloatOutputWriter>(FloatOutputWriter& output_writer, int samples_to_read);
    template void Mixer::Mix<PCMOutputWriter>(PCMOutputWriter& output_writer, int samples_to_read);

    struct TrackVisualCache {
        struct Bounds {
            int32_t x, y, w, h;
        };
        std::atomic_int32_t zoom_factor;
        const uint8_t* scroll_offset;
        Bounds draw_region;
        WavePeaks peaks;
        AmplitudeAutomation gain_automation;

        TrackVisualCache() : zoom_factor(0), scroll_offset(nullptr) {}

        uint32_t SamplesPerPixel(const WaveAudioSource& source) const;

        void ChangeZoom(const int delta) {
            if (delta < 0 && zoom_factor <= 0) {
                return;
            }
            else if (delta > 0 && zoom_factor >= 20) {
                return;
            }
            zoom_factor += delta;
            peaks.dirty = true;
        }
    };
}
