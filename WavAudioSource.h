// WaveAudioSource - reads wav file into memory
// Author - Nic Taylor

#pragma once
#include <array>
#include <vector>
#include <atomic>
#include <memory>

#include "MixScriptAction.h"
#include "MixScriptShared.h"

namespace MixScript
{
    struct WaveAudioBuffer;

    struct DerivativeFilter {
        DerivativeFilter() : y(0.f), bypass(false) {}
        float y;
        bool bypass;
        float Compute(const float x) {
            const float sample = x - y;
            y = x;
            return !bypass ? sample : x;
        }
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
        int64_t transition_samples;
        const uint8_t* cue_pos;
    };

    template<typename Params>
    struct MixerControl {        
        typedef Movement<Params> movement_type;
        std::vector<movement_type> movements;
        bool bypass;

        WaveAudioFormat format;

        MixerControl() : bypass(false) { movements.reserve(256); }

        movement_type& Add(Params&& params, uint8_t const * const position);
        float Apply(uint8_t const * const position, const float sample) const;
        void ClearMovements(uint8_t const * const start, uint8_t const * const end);
    };

    struct WavePeaks {
        struct WavePeak {
            float min;
            float max;
            const uint8_t * start;
            const uint8_t * end;
        };
        std::atomic_bool dirty;
        std::vector<WavePeak> peaks;

        static constexpr int kMaxChannels = 8;
        std::array<DerivativeFilter, kMaxChannels> filters;

        WavePeaks() {            
            SetFilterBypass(true);
            dirty = false; // Set last
        }

        void SetFilterBypass(const bool bypass) {
            for (DerivativeFilter& filter : filters) {
                filter.bypass = bypass;
            }
            dirty = true;
        }
    };

    struct AmplitudeAutomation {
        std::atomic_bool dirty;
        std::vector<float> values;

        AmplitudeAutomation() {
            dirty = false;
        }
    };
    enum CueType : int {
        CT_DEFAULT,
        CT_LEFT_RIGHT,
        CT_LEFT,
        CT_RIGHT,
        CT_IMPLIED
    };
    struct Cue {
        const uint8_t* start;
        CueType type;
    };
    struct WaveAudioSource {
        WaveAudioFormat format;
        std::string file_name;
        std::unique_ptr<WaveAudioBuffer> buffer;        
        uint8_t const * const audio_start;
        uint8_t const * const audio_end;
        std::vector<Cue> cue_starts;
        MixerControl<GainParams> gain_control;
        float bpm;
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
        void AddMarker(const CueType type = CT_DEFAULT);
        void UpdateMarker(const CueType type);
        void DeleteMarker();
        void MoveSelectedMarker(const int32_t num_samples);

        ~WaveAudioSource();

        WaveAudioSource(const char* file_path, const WaveAudioFormat& format_, WaveAudioBuffer* buffer_,
            const AudioRegion& region_, const std::vector<uint32_t>& cue_offsets);

    private:
        int32_t FindMarkerPivot(const int32_t marker_id) const;
        void CorrectImpliedMarkers();
    };

    const uint8_t* ComputeWavePeaks(const WaveAudioSource& source, const uint32_t pixel_width, WavePeaks& peaks,
        const int zoom_factor);
    void ComputeParamAutomation(const WaveAudioSource& source, const uint32_t pixel_width, AmplitudeAutomation& automation,
        const int zoom_factor, uint8_t const * const _scroll_offset);

    std::unique_ptr<WaveAudioSource> LoadWaveFile(const char* file_path);
    bool WriteWaveFile(const char* file_path, const std::unique_ptr<WaveAudioSource>& source);

    void ResetToCue(std::unique_ptr<WaveAudioSource>& source, const uint32_t cue_id);
    void ResetToPos(WaveAudioSource& source, uint8_t const * const position);
    MixScript::Cue* TrySelectMarker(WaveAudioSource& source, uint8_t const * const position, const int tolerance);
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
        void UpdateGainValue(WaveAudioSource& source, const float gain, const float interpolation_percent);
        void HandleAction(const SourceActionInfo& action_info);
        void ProcessActions();
        float GainValue(float& interpolation_percent) const;
        void SetMixSync();
        void AlignPlayingSyncToIncomingStart();
        void AddMarker();
        void DeleteMarker();
        void ClearImpliedMarkers();
        void GenerateImpliedMarkers();
    private:
        struct Region {
            const uint8_t* start;
            const uint8_t* end;
        };

        Region CurrentRegion() const;
        bool use_marker;

        std::unique_ptr<WaveAudioSource> playing;
        std::unique_ptr<WaveAudioSource> incoming;

        ActionQueue actions;
        void DoAction(const SourceActionInfo& action_info);
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

        float SamplesPerPixel(const WaveAudioSource& source) const;

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
