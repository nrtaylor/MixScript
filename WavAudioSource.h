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

        movement_type& Add(const Params& params, uint8_t const * const position);
        float Apply(uint8_t const * const position, const float sample) const;
        void ClearMovements(uint8_t const * const start, uint8_t const * const end);
    };

    template struct MixerControl<GainParams>; // TODO: Clean-up explicit definition warnings.
   
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
        MixerControl<GainParams> fader_control;
        MixerControl<GainParams> gain_control;
        float bpm;
        int selected_marker;

        const float kSampleRatio = 1.f / (float)((uint32_t)1 << (uint32_t)31);
        const uint8_t* read_pos;
        std::atomic_int32_t last_read_pos;
        uint8_t* write_pos;
        float Read();
        float ReadAndProcess();
        float Read(const uint8_t** read_pos_) const;
        void Write(const float value);
        bool Cue(uint8_t const * const position, uint32_t& cue_id) const;
        const uint8_t * SelectedMarkerPos() const;
        void TryWrap();
        void AddMarker(const CueType type = CT_DEFAULT);
        void UpdateMarker(const CueType type);
        void DeleteMarker();
        void MoveSelectedMarker(const int32_t num_samples);

        const MixerControl<GainParams>& GetControl(const MixScript::SourceAction action) const;
        MixerControl<GainParams>& GetControl(const MixScript::SourceAction action);

        ~WaveAudioSource();

        WaveAudioSource(const char* file_path, const WaveAudioFormat& format_, WaveAudioBuffer* buffer_,
            const AudioRegion& region_, const std::vector<uint32_t>& cue_offsets);

    private:
        int32_t FindMarkerPivot(const int32_t marker_id) const;
        void CorrectImpliedMarkers();
    };

    std::unique_ptr<WaveAudioSource> LoadWaveFile(const char* file_path);
    bool WriteWaveFile(const char* file_path, const std::unique_ptr<WaveAudioSource>& source);

    void ResetToCue(std::unique_ptr<WaveAudioSource>& source, const uint32_t cue_id);
    void ResetToPos(WaveAudioSource& source, uint8_t const * const position);
    MixScript::Cue* TrySelectMarker(WaveAudioSource& source, uint8_t const * const position, const int tolerance);
    void ReadSamples(std::unique_ptr<WaveAudioSource>& source, float* left, float* right, int samples_to_read);

    void UpdateGainValue(const WaveAudioSource& source, const GainParams& params, MixerControl<GainParams>& control,
        const float interpolation_percent, const bool update_param_on_selected_marker);
}
