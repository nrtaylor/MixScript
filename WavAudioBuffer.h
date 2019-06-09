// WaveAudioFormat- reads wav file into memory
// Author - Nic Taylor

#pragma once
#include <vector>
#include "MixScriptShared.h"

namespace MixScript {
    struct WaveAudioBuffer {
        uint8_t* const samples;
        const uint32_t file_size;
        WaveAudioBuffer(uint8_t* const samples_, const uint32_t file_size_) :
            file_size(file_size_),
            samples(samples_) {
        }
    };

    void ParseWaveFile(WaveAudioFormat* format, WaveAudioBuffer* buffer,
        std::vector<uint32_t>* cues, AudioRegion* region);
}