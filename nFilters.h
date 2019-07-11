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

    struct TwoPoleFilterParams {
        TwoPoleFilterParams(const float _b0, const float _b1, const float _b2,
            const float _a1, const float _a2) : b0(_b0), b1(_b1), b2(_b2), a1(_a1), a2(_a2) {}
        float b0, b1, b2;
        float     a1, a2;
    };

    class TwoPoleFilter {
    private:        
        float x1, x2;
        float y1, y2;
        bool bypass;
    public:
        TwoPoleFilter() :
            x1(0.f), x2(0.f), y1(0.f), y2(0.f), bypass(false) { }

        void Bypass(const bool _bypass) { bypass = _bypass; }

        float Apply(const TwoPoleFilterParams& params, const float x0) {
            float sample = params.b0 * x0 + params.b1 * x1 + params.b2 * x2
                - params.a1 * y1 - params.a2 * y2;
            y2 = y1; y1 = sample;
            x2 = x1; x1 = x0;
            return !bypass ? sample : x0;
        }
    };
    
    inline TwoPoleFilterParams TwoPoleNullConfig() {
        return TwoPoleFilterParams(1.f, 0.f, 0.f, 0.f, 0.f);
    }
    TwoPoleFilterParams TwoPoleButterworthLowShelfConfig(const float cuttoff_percent, const float gain_db);
    TwoPoleFilterParams TwoPoleButterworthHighShelfConfig(const float cuttoff_percent, const float gain_db);
}
