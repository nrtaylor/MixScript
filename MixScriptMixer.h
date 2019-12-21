// MixScriptMixer - mixes two tracks
// Author - Nic Taylor

#pragma once
#include <array>
#include <vector>
#include <atomic>
#include <memory>

#include "MixScriptAction.h"
#include "MixScriptShared.h"
#include "WavAudioSource.h"
#include "nFilters.h"

namespace MixScript
{
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
        void LoadPlaceholders();
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
        MixScript::SourceAction SelectedAction() const { return selected_action.load(); }
        void SetSelectedAction(const MixScript::SourceAction action) { selected_action.store(action); }

        void SetSelectedMarker(int cue_id);
        void UpdateGainValue(WaveAudioSource& source, const float gain, const float interpolation_percent);
        void HandleAction(const SourceActionInfo& action_info);
        void ProcessActions();
        float FaderGainValue(float& interpolation_percent) const;
        void SetMixSync();
        void SeekSync();
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
        // When true, only adjust params on the selected marker.
        bool update_param_on_selected_marker;

        std::unique_ptr<WaveAudioSource> playing;
        std::unique_ptr<WaveAudioSource> incoming;

        ActionQueue actions;
        std::atomic<MixScript::SourceAction> selected_action;
        void DoAction(const SourceActionInfo& action_info);
    };

    template void Mixer::Mix<FloatOutputWriter>(FloatOutputWriter& output_writer, int samples_to_read);
    template void Mixer::Mix<PCMOutputWriter>(PCMOutputWriter& output_writer, int samples_to_read);

    struct AmplitudeAutomation {
        std::atomic_bool dirty;
        std::vector<float> values;

        AmplitudeAutomation() {
            dirty = false;
        }
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
        std::array<nMath::DerivativeFilter, kMaxChannels> filters;

        WavePeaks() {
            SetFilterBypass(true);
            dirty = false; // Set last
        }

        void SetFilterBypass(const bool bypass) {
            for (nMath::DerivativeFilter& filter : filters) {
                filter.bypass = bypass;
            }
            dirty = true;
        }
    };

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

    const uint8_t* ComputeWavePeaks(const WaveAudioSource& source, const uint32_t pixel_width, WavePeaks& peaks,
        const int zoom_factor);
    void ComputeParamAutomation(const WaveAudioSource& source, const uint32_t pixel_width, AmplitudeAutomation& automation,
        const int zoom_factor, uint8_t const * const _scroll_offset, const MixScript::SourceAction selected_action);
}
