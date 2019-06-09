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
}
