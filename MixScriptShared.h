// WaveAudioFormat- reads wav file into memory
// Author - Nic Taylor

#pragma once
#include <stdint.h>

namespace MixScript {
    struct WaveAudioFormat {
        uint32_t channels;
        uint32_t sample_rate;
        uint32_t bit_rate;
    };

    // TODO: Consolidate with Mixer::Region
    struct AudioRegion {
        uint8_t * start;
        uint8_t * end;
    };

    struct AudioRegionC {
        uint8_t const * start;
        uint8_t const * end;
    };

    inline uint32_t ByteRate(const WaveAudioFormat& format) {
        return format.bit_rate / 8;
    }

    inline float BytesToTimeMs(const WaveAudioFormat& format, const uint64_t& bytes) {
        return 1000.f * (float)bytes / (float)(ByteRate(format) * format.channels * format.sample_rate);
    }

    // TODO: float or round to int
    inline float TimeMsToBytes(const WaveAudioFormat& format, const float& duration) {
        return duration * (float)(ByteRate(format) * format.channels * format.sample_rate) / 1000.f;
    }
}
