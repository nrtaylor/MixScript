// WaveAudioSource - reads wav file into memory
// Author - Nic Taylor

#pragma once
#include <array>
#include <vector>
#include <atomic>
#include <memory>

#include "MixScriptAction.h"
#include "MixScriptShared.h"
#include "nFilters.h"

namespace MixScript
{
    struct WaveAudioBuffer;
    
    struct GainControl {
        float gain;
        float Value() const {
            return gain;
        }
    };

    struct BiquadFilterInterpolatedState {
        nMath::TwoPoleFilter left; // region
        nMath::TwoPoleFilter right;
    };

    enum MixFadeType : int32_t {
        MFT_LINEAR = 1,
        MFT_SQRT,
        MFT_TRIG,
        MFT_EXP,
    };

    struct MovementPrecomputeCache {
        virtual void Remove(const int index) = 0;
    };

    struct MovementPrecomputCacheTwoPoleFilter : public MovementPrecomputeCache {
        std::vector<nMath::TwoPoleFilterParams> cache;
        void Remove(const int index) {
            cache.erase(cache.begin() + index);
        }
    };

    struct Movement {
        GainControl control;
        MixFadeType interpolation_type;
        float threshold_percent;
        int64_t transition_samples;
        const uint8_t* cue_pos;
        int precompute_index;
    };
    
    struct MixerControl {
        typedef Movement movement_type;
        std::vector<movement_type> movements;
        MovementPrecomputeCache* cache;
        bool bypass;
        
        MixerControl() : bypass(false), cache(nullptr) { movements.reserve(256); }

        movement_type& Add(const GainControl& control, uint8_t const * const position);
        struct MixerInterpolation {
            const movement_type* start;
            const movement_type* end;
            float ratio;
        };
        MixerInterpolation GetInterpolation(uint8_t const * const position) const;
        float ValueAt(uint8_t const * const position) const;
        void ClearMovements(uint8_t const * const start, uint8_t const * const end);
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
        MixerControl gain_control;
        MixerControl fader_control;
        MixerControl lp_shelf_control;
        MovementPrecomputCacheTwoPoleFilter lp_shelf_precomute;
        std::array<BiquadFilterInterpolatedState, 2> lp_shelf_filters;
        float bpm;
        int selected_marker;

        const float kSampleRatio = 1.f / (float)((uint32_t)1 << (uint32_t)31);
        const uint8_t* read_pos;
        std::atomic_int32_t last_read_pos;
        uint8_t* write_pos;
        float Read();
        float ReadAndProcess(const int channel);
        float Read(const uint8_t** read_pos_) const;
        void Write(const float value);
        bool Cue(uint8_t const * const position, uint32_t& cue_id) const;
        const uint8_t * SelectedMarkerPos() const;
        void TryWrap();
        void AddMarker(const CueType type = CT_DEFAULT);
        void UpdateMarker(const CueType type);
        void DeleteMarker();
        void MoveSelectedMarker(const int32_t num_samples);

        const MixerControl& GetControl(const MixScript::SourceAction action) const;
        MixerControl& GetControl(const MixScript::SourceAction action);

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

    void UpdateMovement(const WaveAudioSource& source, const GainControl& control, MixerControl& mixer_control,
        const float interpolation_percent, const bool update_param_on_selected_marker, const int precompute_index);
}
