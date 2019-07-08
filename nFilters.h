// nFilters - simple IIR filters
// Author - Nic Taylor

#pragma once

namespace nMath {
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
}
