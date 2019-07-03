// WaveAudioSource - reads wav file into memory
// Author - Nic Taylor

#include "WavAudioSource.h"
#include "WavAudioBuffer.h"
#include "nMath.h"
#undef UNICODE // using single byte file loading routines
#include <windows.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <algorithm>
#include <stdint.h>
#include <memory>
#include <assert.h>

#include <iostream>
#include <fstream>
#include <string>

namespace MixScript
{
    const uint32_t kMaxAudioEsimatedDuration = 10 * 60; // minutes
    const uint32_t kMaxAudioBufferSize = kMaxAudioEsimatedDuration * 48000 * 2 * 2 + 1024; // sample rate * byte_rate * channels

    float WaveAudioSource::Read() {
        if (read_pos >= audio_end) {
            return 0.f;
        }
        const int32_t next = (int32_t)((*(const uint32_t*)read_pos) << 16);
        read_pos += 2;

        return next * kSampleRatio;
    }

    float WaveAudioSource::Read(const uint8_t** read_pos_) const {
        const int32_t next = (int32_t)((*(const uint32_t*)(*read_pos_)) << 16);
        (*read_pos_) += 2;

        return next * kSampleRatio;
    }

    void WaveAudioSource::Write(const float value) {
        const float sample_max = (float)((1 << (format.bit_rate - 1)) - 1);        
        const int32_t next = (uint32_t)roundf(value * sample_max);
        memcpy(write_pos, (uint8_t*)&next, format.bit_rate);
        write_pos += 2; // TODO: Byte rate
    }

    int32_t WaveAudioSource::FindMarkerPivot(const int32_t marker_id) const {
        const int32_t marker_index = marker_id - 1;
        if (marker_index < 0 ||
            (cue_starts[marker_index].type != CT_IMPLIED &&
             cue_starts[marker_index].type != CT_DEFAULT)) {
            return marker_id;
        }        
        for (int32_t index = marker_index - 1; index >= 0; --index) {
            if (cue_starts[index].type == CT_LEFT_RIGHT ||
                cue_starts[index].type == CT_RIGHT) {
                return index + 1; // to id
            }
        }
        for (int32_t index = marker_index + 1; index < cue_starts.size(); ++index) {
            if (cue_starts[index].type == CT_LEFT_RIGHT ||
                cue_starts[index].type == CT_LEFT) {
                return index + 1; // to id
            }
        }
        return marker_id;
    }

    void WaveAudioSource::CorrectImpliedMarkers() {
        const int pivot_id = FindMarkerPivot(selected_marker);
        if (pivot_id == 0 || pivot_id >= (int)cue_starts.size() || selected_marker == pivot_id) {
            return;
        }
        const MixScript::Cue& pivot_cue = cue_starts[pivot_id - 1];
        uint8_t const * const start = pivot_cue.start;
        const int64_t delta = cue_starts[selected_marker - 1].start - start;
        const float new_delta = fabsf(static_cast<float>(delta) / (selected_marker - pivot_id));
        const int pivot_index = pivot_id - 1;        
        auto update_marker = [&, this](MixScript::Cue& cue, const int32_t index) -> bool {
            if (FindMarkerPivot(index + 1) == pivot_id) {
                if (int samples = static_cast<int>((index - pivot_index) * new_delta)) {
                    samples &= ~(0x04 - 1);
                    cue.start = start + samples;
                    return true;
                }
            }
            return false;
        };
        if (pivot_cue.type == CT_LEFT || pivot_cue.type == CT_LEFT_RIGHT) {
            for (int32_t index = pivot_index - 1; index >= 0; --index) {
                if (!update_marker(cue_starts[index], index)) {
                    break;
                }
            }
        }
        if (pivot_cue.type == CT_RIGHT || pivot_cue.type == CT_LEFT_RIGHT) {
            for (int32_t index = pivot_index + 1; index < cue_starts.size(); ++index) {
                if (!update_marker(cue_starts[index], index)) {
                    break;
                }
            }
        }
        const float samples_per_beat = new_delta / (4 * format.channels * ByteRate(format));
        bpm = 60.f * format.sample_rate / samples_per_beat;
    }

    void WaveAudioSource::MoveSelectedMarker(const int32_t num_samples) {
        if (selected_marker <= 0) {
            return;
        }
        const int32_t alignment = format.channels * ByteRate(format);
        const int32_t num_bytes = num_samples * alignment;
        const int selected_marker_index = selected_marker - 1;        
        cue_starts[selected_marker_index].start += num_bytes;
        if (cue_starts[selected_marker_index].type == CT_IMPLIED) {
            CorrectImpliedMarkers();
        }
        if (selected_marker_index > 0 &&
            cue_starts[selected_marker_index - 1].start > cue_starts[selected_marker_index].start) {
            std::swap(cue_starts[selected_marker_index - 1], cue_starts[selected_marker_index]);
            --selected_marker;
        }
        else if (selected_marker_index < cue_starts.size() - 1 &&
            cue_starts[selected_marker_index + 1].start < cue_starts[selected_marker_index].start) {
            std::swap(cue_starts[selected_marker_index + 1], cue_starts[selected_marker_index]);
            ++selected_marker;
        }        
    }

    void WaveAudioSource::AddMarker(const CueType type /*= CT_DEFAULT*/) {
        assert((read_pos - audio_start) % 4 == 0);
        auto it = cue_starts.begin();
        for (; it != cue_starts.end(); ++it) {
            if ((*it).start > read_pos) {
                break;
            }
        }
        const bool change_default = type == CT_DEFAULT && cue_starts.size() == 0;
        it = cue_starts.insert(it, { read_pos, change_default ? CT_LEFT_RIGHT : type });
        selected_marker = 1 + static_cast<int>(it - cue_starts.begin());
    }

    void WaveAudioSource::UpdateMarker(const CueType type) {
        if (selected_marker <= 0) {
            return;
        }
        cue_starts[selected_marker - 1].type = type;
    }

    void WaveAudioSource::DeleteMarker() {
        if (selected_marker <= 0) {
            return;
        }
        const int selected_marker_index = selected_marker - 1;
        cue_starts.erase(cue_starts.begin() + selected_marker_index);
        if (selected_marker_index >= cue_starts.size()) {
            --selected_marker;
        }
    }

    void WaveAudioSource::TryWrap() {
        if (read_pos >= audio_end) {
            read_pos = audio_start;
        }
    }

    WaveAudioSource::~WaveAudioSource() {
        OutputDebugString("Destroying Source");
    }

    WaveAudioSource::WaveAudioSource(const char* file_path, const WaveAudioFormat& format_, WaveAudioBuffer* buffer_,
        const AudioRegion& region_, const std::vector<uint32_t>& cue_offsets):
        file_name(file_path),
        format(format_),
        buffer(buffer_),
        audio_start(region_.start),
        audio_end(region_.end),
        selected_marker(-1),
        bpm(-1.f) {
        read_pos = region_.start;
        last_read_pos = 0;
        write_pos = region_.start;
        for (const uint32_t cue_offset : cue_offsets) {
            cue_starts.push_back({ audio_start + 4 * cue_offset, CT_DEFAULT });
        }
        if (cue_starts.size()) {
            cue_starts.front().type = CT_LEFT_RIGHT;
        }
    }

    // Returns true if on the cue
    bool WaveAudioSource::Cue(uint8_t const * const position, uint32_t& cue_id) const {
        cue_id = 0;
        for (const MixScript::Cue& cue : cue_starts) {
            uint8_t const * const cue_pos = cue.start;
            ++cue_id;
            if (cue_pos == position) {
                return true;
            }
            else if (cue_pos > position) {
                --cue_id;
                break;
            }
        }
        return false;
    }

    void ResetToCue(std::unique_ptr<WaveAudioSource>& source_, const uint32_t cue_id) {
        WaveAudioSource& source = *source_.get();

        if (cue_id == 0) {
            source.read_pos = source.audio_start;
        }
        else if (cue_id <= source.cue_starts.size()) {
            source.read_pos = source.cue_starts[cue_id - 1].start;
        }
        source.last_read_pos = static_cast<int32_t>(source.read_pos - source.audio_start);
        assert(source.last_read_pos.load() % 4 == 0);
    }

    void ResetToPos(WaveAudioSource& source, uint8_t const * const position) {
        source.read_pos = position;
        source.last_read_pos = static_cast<int32_t>(position - source.audio_start);
        assert(source.last_read_pos.load() % 4 == 0);
    }

    MixScript::Cue* TrySelectMarker(WaveAudioSource& source, uint8_t const * const position, const int tolerance) {
        int index = 0;
        for (MixScript::Cue& cue : source.cue_starts) {                        
            if (abs(static_cast<int>(cue.start - position)) <= tolerance) {
                source.selected_marker = index + 1;
                return &cue;
            }
            ++index;
        }
        return nullptr;
    }

    void ReadSamples(std::unique_ptr<WaveAudioSource>& source_, float* left, float* right, int samples_to_read) {
        WaveAudioSource& source = *source_.get();

        for (int32_t i = 0; i < samples_to_read; ++i) {
            source.TryWrap();
            *left = source.Read();
            *right = source.Read();
            ++left;
            ++right;
        }
    }
   
    const uint8_t * WaveAudioSource::SelectedMarkerPos() const {
        if (selected_marker <= 0) {
            return audio_start;
        }
        return cue_starts[selected_marker - 1].start;
    }

    template <typename Params>
    void MixerControl<Params>::ClearMovements(uint8_t const * const start, uint8_t const * const end) {
        if (!movements.empty()) {
            movements.erase(
                std::remove_if(movements.begin(), movements.end(), [&](const MixerControl::movement_type& movement) {
                return movement.cue_pos > start && movement.cue_pos <= end;
            }), movements.end());
        }
    }

    void UpdateGainValue(const WaveAudioSource& source, const GainParams& params, MixerControl<GainParams>& control,
        const float interpolation_percent, const bool update_param_on_selected_marker) {
        const int cue_id = source.selected_marker;
        if (!update_param_on_selected_marker || cue_id > 0) {
            uint8_t const * const marker_pos = update_param_on_selected_marker ? source.cue_starts[cue_id - 1].start :
                (source.audio_start + source.last_read_pos);
            int gain_cue_id = 0;
            bool found = false;
            // TODO: Binary Search
            for (Movement<GainParams>& movement : control.movements) {
                // TODO: Decide if it is easier to separate automation points from cues, or if
                // automation and cues should stay in sync.
                if (movement.cue_pos == marker_pos) {
                    movement.params = params;
                    movement.threshold_percent = interpolation_percent;
                    movement.transition_samples = (int64_t)TimeMsToBytes(source.format, 5.f);
                    found = true;
                    break;
                }
                else if (movement.cue_pos > marker_pos) {
                    break;
                }
                ++gain_cue_id;
            }
            if (!found) {
                if (gain_cue_id >= control.movements.size()) {
                    Movement<GainParams>& movement = control.Add(params, marker_pos);
                    movement.threshold_percent = interpolation_percent;
                    movement.transition_samples = (int64_t)TimeMsToBytes(source.format, 5.f);
                }
                else {
                    control.movements.insert(control.movements.begin() + gain_cue_id,
                        Movement<GainParams>{ params, MFT_LINEAR, interpolation_percent,
                        (int64_t)TimeMsToBytes(source.format, 5.f), marker_pos });
                }
            }
        }
    }

    float InterpolateMix(const float param, const float inv_duration, const MixFadeType fade_type) {
        switch (fade_type) {
        case MFT_LINEAR:
            return param * inv_duration;
        case MFT_SQRT:
            return sqrtf(param * inv_duration);
        case MFT_TRIG:
            return sinf(param * inv_duration * M_PI_2);
        case MFT_EXP:
            return expf(param * inv_duration)/M_E;
        }

        return param;
    }

    template<typename Params>
    Movement<Params>& MixerControl<Params>::Add(const Params& params, uint8_t const * const position) {
        movements.emplace_back(Movement<Params>{ params, MFT_LINEAR, 0.f, 0, position });
        return movements.back();
    }

    template<typename Params>
    float MixerControl<Params>::Apply(uint8_t const * const position, const float sample) const {
        if (movements.empty() ||
            bypass) {
            return sample;
        }

        const auto interval = std::lower_bound(movements.begin(), movements.end(), position,
            [](const Movement<Params>& lhs, uint8_t const * const rhs) { 
                return lhs.cue_pos < rhs; 
        });

        if (interval == movements.begin()) {
            return movements.front().params.Apply(sample);
        }
        if (interval == movements.end()) {
            return movements.back().params.Apply(sample);
        }

        const Movement<Params>& start_state = *(interval - 1);
        const Movement<Params>& end_state = *interval;        

        const int64_t t = (int64_t)(position - start_state.cue_pos);            
        const int64_t duration = (int64_t)(end_state.cue_pos - start_state.cue_pos);

        const float start_value = start_state.params.Apply(sample);
        float ratio = (float)t / (float)duration;
        // If transition_samples is zero, assume threshold_percent is being used instead.
        if (end_state.transition_samples != 0) {
            if (t < duration - end_state.transition_samples) {
                return start_value;
            }
            if (duration > end_state.transition_samples) {
                ratio = 1.f - (duration - t) / (float)end_state.transition_samples;
            }
        }
        else {                
            if (ratio < end_state.threshold_percent) {
                return start_value;
            }
                
            // TODO: Clean up
            if (end_state.threshold_percent > 0.f) {
                uint8_t const * const start_pos = start_state.cue_pos +
                    static_cast<int32_t>((float)duration * end_state.threshold_percent);
                ratio = (float)(position - start_pos) / (float)(end_state.cue_pos - start_pos);
            }
        }

        const float end_value = end_state.params.Apply(sample);
        return InterpolateMix(end_value - start_value, ratio, end_state.interpolation_type) + start_value;
    }

    float WaveAudioSource::ReadAndProcess() {
        uint8_t const * const starting_read_pos = read_pos;
        float sample = gain_control.Apply(starting_read_pos, Read());
        sample = fader_control.Apply(starting_read_pos, sample);
        return sample;
    }
    
    std::unique_ptr<WaveAudioSource> LoadWaveFile(const char* file_path) {

        uint8_t* large_file_buffer = new uint8_t[kMaxAudioBufferSize];

        HANDLE file = CreateFile(file_path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
            nullptr);
        DWORD bytes_read = 0;
        if (file != INVALID_HANDLE_VALUE)
        {                        
            const bool read_entire_file = ReadFile(file, (LPVOID)large_file_buffer, kMaxAudioBufferSize - 1, &bytes_read, 
                nullptr) != 0;            
            CloseHandle(file);
        }

        WaveAudioBuffer* wav_buffer = new WaveAudioBuffer(large_file_buffer, static_cast<uint32_t>(bytes_read));
        WaveAudioFormat format;
        std::vector<uint32_t> cues;
        AudioRegion region;
        ParseWaveFile(&format, wav_buffer, &cues, &region);

        return std::unique_ptr<WaveAudioSource>(new WaveAudioSource(file_path, format, wav_buffer, region, cues));
    }

    bool WriteWaveFile(const char* file_path, const std::unique_ptr<WaveAudioSource>& source) {
        HANDLE file = CreateFile(file_path, GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (file != INVALID_HANDLE_VALUE) {            
            DWORD bytes_written = 0;
            std::vector<uint8_t> file_buffer(std::move(
                ToByteBuffer(source->format, AudioRegionC{ source->audio_start, source->audio_end })));
            const DWORD byte_count = (DWORD)file_buffer.size();
            BOOL bWriteComplete = WriteFile(file, &file_buffer[0], byte_count, &bytes_written, nullptr);
            assert(bytes_written == byte_count);
            CloseHandle(file);
            return bWriteComplete;
        }

        return false;
    }
    
    MixerControl<GainParams>& WaveAudioSource::GetControl(const MixScript::SourceAction action) {
        if (action == MixScript::SA_MULTIPLY_FADER_GAIN) {
            return fader_control;
        }
        return gain_control;
    }

    const MixerControl<GainParams>& WaveAudioSource::GetControl(const MixScript::SourceAction action) const {
        if (action == MixScript::SA_MULTIPLY_FADER_GAIN) {
            return fader_control;
        }
        return gain_control;
    }
}