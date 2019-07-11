// nFilters - simple IIR filters
// Author - Nic Taylor

#include "nFilters.h"

#define _USE_MATH_DEFINES
#include <math.h>

namespace nMath {
    // Based on 'Cookbook formulae for audio EQ biquad filter coefficients' by Robert Bristow-Johnson.
    TwoPoleFilterParams TwoPoleButterworthLowShelfConfig(const float cuttoff_percent, const float db)
    {
        constexpr float ln10_40 = 2.30258512f / 40.f;
        const float sqrt_gain = expf(db * ln10_40);
        const float sqrt_gain_2 = 2.f * sqrtf(sqrt_gain);

        const float filter_cutoff = 2.f * (float)M_PI * cuttoff_percent;
        const float omega_cos = cosf(filter_cutoff);
        const float omega_sin = sinf(filter_cutoff);
        const float alpha = omega_sin * (float)M_SQRT1_2;

        const float inv_a0 = 1.f / ((sqrt_gain + 1) + (sqrt_gain - 1) * omega_cos + sqrt_gain_2 * alpha);
        const float a1 = -2 * ((sqrt_gain - 1) + (sqrt_gain + 1) * omega_cos);
        const float a2 = (sqrt_gain + 1) + (sqrt_gain - 1) * omega_cos - sqrt_gain_2 * alpha;
        const float b0 = sqrt_gain*((sqrt_gain + 1) - (sqrt_gain - 1) * omega_cos + sqrt_gain_2 * alpha);
        const float b1 = 2 * sqrt_gain *((sqrt_gain - 1) - (sqrt_gain + 1) * omega_cos);
        const float b2 = sqrt_gain * ((sqrt_gain + 1) - (sqrt_gain - 1) * omega_cos - sqrt_gain_2 * alpha);

        return TwoPoleFilterParams(b0 * inv_a0, b1 * inv_a0, b2 * inv_a0, a1 * inv_a0, a2 * inv_a0);
    }

    TwoPoleFilterParams TwoPoleButterworthHighShelfConfig(const float cuttoff_percent, const float gain_db)
    {
        const float sqrt_gain = powf(10.f, gain_db / 40.f);
        const float sqrt_gain_2 = 2.f * sqrtf(sqrt_gain);

        const float filter_cutoff = 2.f * (float)M_PI * cuttoff_percent;
        const float omega_cos = cosf(filter_cutoff);
        const float omega_sin = sinf(filter_cutoff);
        const float alpha = omega_sin * (float)M_SQRT1_2;

        const float inv_a0 = 1.f / ((sqrt_gain + 1) - (sqrt_gain - 1) * omega_cos + sqrt_gain_2 * alpha);
        const float a1 = 2 * ((sqrt_gain - 1) - (sqrt_gain + 1) * omega_cos);
        const float a2 = (sqrt_gain + 1) - (sqrt_gain - 1) * omega_cos - sqrt_gain_2 * alpha;
        const float b0 = sqrt_gain*((sqrt_gain + 1) + (sqrt_gain - 1) * omega_cos + sqrt_gain_2 * alpha);
        const float b1 = -2 * sqrt_gain *((sqrt_gain - 1) + (sqrt_gain + 1) * omega_cos);
        const float b2 = sqrt_gain * ((sqrt_gain + 1) + (sqrt_gain - 1) * omega_cos - sqrt_gain_2 * alpha);

        return TwoPoleFilterParams(b0 * inv_a0, b1 * inv_a0, b2 * inv_a0, a1 * inv_a0, a2 * inv_a0);
    }
}