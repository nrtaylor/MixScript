// VolumeNormalization.cpp : Defines the entry point for the console application.
//

#pragma once

//#define PROFILE_ENABLE_NO_INLINE

//#undef UNICODE
//#include <windows.h>
//#include <stdint.h>
//#define _USE_MATH_DEFINES
#include <math.h>
#include <assert.h>
#include <array>
#include <vector>
#include <atomic>
#include <thread>

#include <xmmintrin.h>
#include <smmintrin.h>
#include <pmmintrin.h>

namespace Loudness
{

    static float DBToGain(
        const float flDB)
    {
        //return powf(10.f, flDB * 0.05f);
        // 10^x = exp(ln(10)*x)
        const static float flLn10_20 = 2.30258512f * 0.05f;        
        return expf(flDB * flLn10_20);
    }

    static float GainToDB(
        const float flGain)
    {
        return 20.f * log10f(flGain);
    }

    class TwoPoleFilter2
    {
    private:
        //alignas(16) float arY1Y2X1X2[4];
        const __m128 mCoefficients; // b1, b2, -a1, -a2
        __m128 mY1Y2X1X2;

        float b0;

        //float &x1, &x2;
        //float &y1, &y2;
    public:
        TwoPoleFilter2(
            float _b0,
            float _b1,
            float _b2,
            float _a1,
            float _a2)
            :
            b0(_b0),
            //x1(arY1Y2X1X2[2]),
            //x2(arY1Y2X1X2[3]),
            //y1(arY1Y2X1X2[0]),
            //y2(arY1Y2X1X2[1]),
            mCoefficients(_mm_setr_ps(-_a1, -_a2, _b1, _b2))
        {
            Reset();
        }

#ifdef PROFILE_ENABLE_NO_INLINE
        __declspec(noinline)
#endif
        float Process(const float x0)
        {
            __m128 input = _mm_setr_ps(b0 * x0, 0.f, x0, 0.f);            
            __m128 sample = _mm_dp_ps(mY1Y2X1X2, mCoefficients, 0xF1);
            sample = _mm_add_ps(input, sample);
            mY1Y2X1X2 = _mm_shuffle_ps(mY1Y2X1X2, mY1Y2X1X2, 0x80);
            mY1Y2X1X2 = _mm_blend_ps(sample, mY1Y2X1X2, 0x0A);
            
            return _mm_cvtss_f32(sample);
        }

        void Reset()
        {
            mY1Y2X1X2 = _mm_set_ss(0.f);
        }
    };

    class TwoPoleFilter
    {
    private:
        float b0, b1, b2;
        float     a1, a2;

        float x1, x2;
        float y1, y2;
    public:
        TwoPoleFilter(
            float _b0,
            float _b1,
            float _b2,
            float _a1,
            float _a2)
            :
            b0(_b0),
            b1(_b1),
            b2(_b2),
            a1(_a1),
            a2(_a2),
            x1(0.f),
            x2(0.f),
            y1(0.f),
            y2(0.f)
        { }

#ifdef PROFILE_ENABLE_NO_INLINE
        __declspec(noinline)
#endif
float Process(const float x0)
{
    float sample = b0*x0 + b1*x1 + b2*x2
        - a1*y1 - a2*y2;
    y2 = y1; y1 = sample;

    x2 = x1; x1 = x0;
    return sample;
}

        void Reset()
        {
            x1 = x2 = y1 = y2 = 0.f;
        }
    };

    class ExponentialEnvelope
    {
    private:
        float coeff;
        float ms, sampleRate; // for debugging
    public:
        ExponentialEnvelope(
            float _ms,
            float _sampleRate)
            : ms(_ms),
            sampleRate(_sampleRate)
        {
            const float flSamples = (ms * sampleRate) / 1000;
            coeff = expf(-1.f / flSamples);
        }

        float Process(const float x0, float& y1) const
        {
            y1 = x0 + coeff * (y1 - x0);
            return y1;
        }

        float Advance(const float y1) const
        {
            return coeff * y1;
        }
    };

    class Compressor
    {
    private:
        float flThresholdDB;
        float flThresholdGain;
        float flRatio;

        float y1;
        ExponentialEnvelope tAttack;
        ExponentialEnvelope tRelease;

        float flMaxGainReduction;
    public:

        Compressor(
            const float _flThresholdDB,
            const float _flSlope,
            const float _flSampleRate,
            const float _flAttackMs,
            const float _flReleaseMs) :
            flThresholdDB(_flThresholdDB),
            flThresholdGain(DBToGain(_flThresholdDB)),
            flRatio(1.f / _flSlope),
            y1(0.f),
            tAttack(_flAttackMs, _flSampleRate),
            tRelease(_flReleaseMs, _flSampleRate),
            flMaxGainReduction(0.f)
        {}

#ifdef PROFILE_ENABLE
        __declspec(noinline)
#endif
        bool Process(const float x0, float& out_gain)
        {
            float flOverDb = 0.f;
            const float x0Abs = fabsf(x0);
            if (x0Abs > flThresholdGain)
            {
                const float x0db = GainToDB(x0Abs);
                flOverDb = x0db - flThresholdDB;
            }

            if (flOverDb > y1)
            {
                tAttack.Process(flOverDb, y1);
            }
            else
            {
                tRelease.Process(flOverDb, y1);
            }

            if (y1 > FLT_EPSILON)
            {
                // DBToGain(flOverDb / slope ) / DBToGain(flOverDb) simplified.
                float flGain = DBToGain(y1 * (flRatio - 1.f));

                if ((1.f - flGain) > flMaxGainReduction)
                {
                    flMaxGainReduction = 1.f - flGain;
                }
                out_gain = flGain;
                return true;
            }
            else
            {
                out_gain = 1.f;
                return false;
            }
        }
    };

    class Limiter
    {
    private:
        float y1Envelope;
        ExponentialEnvelope tAttack;
        ExponentialEnvelope tRelease;

        float flHeadroomGain, flHeadroomInvGain;
        float flSigma;
        const uint32_t dwLookAheadSamples;
        uint32_t dwPeakDistance;
        bool bPeaked;
        const float flEnvelopScalor = 1.f / 0.61656f; // Guarantee attack will overshoot 1.0 by the sample that clips.
    public:
        Limiter(
            const float _flSampleRate,
            const float _flLookAheadMs,
            const float _flReleaseMs,
            const float _flHeadroomDb,
            const float _flSigma)
            :
            dwLookAheadSamples((uint32_t)roundf(_flLookAheadMs * _flSampleRate / 1000)),
            dwPeakDistance(0),
            tAttack(_flLookAheadMs, _flSampleRate),
            tRelease(_flReleaseMs, _flSampleRate),
            flSigma(_flSigma),
            y1Envelope(0.f),
            bPeaked(false),
            flHeadroomGain(DBToGain(_flHeadroomDb)),
            flHeadroomInvGain(1.f / flHeadroomGain)
        {
        }

        bool Peaked() const
        {
            return bPeaked;
        }

#ifdef PROFILE_ENABLE
        __declspec(noinline)
#endif
        float Process(const float x0, const float flAhead)
        {
            if (fabsf(flAhead) < (1.f - flSigma) * flHeadroomGain)
            {
                if (bPeaked)
                {
                    if (dwPeakDistance == 0)
                    {
                        y1Envelope = tRelease.Advance(y1Envelope);
                    }
                    else
                    {
                        --dwPeakDistance;
                        tAttack.Process(1.0, y1Envelope);
                    }
                }                
            }
            else
            {
                dwPeakDistance = dwLookAheadSamples;
                tAttack.Process(1.0, y1Envelope);
                bPeaked = true;
            }
            
            if (y1Envelope < 1.0e-10f) // -200 db
            {
                return x0;
            }
            else
            {
                float flEnvelopeValue = y1Envelope * flEnvelopScalor;
                if (flEnvelopeValue > 1.f)
                {
                    flEnvelopeValue = 1.f;
                }
                const float absx0 = fabsf(x0);
                assert(absx0 < 1.f || flEnvelopeValue == 1.f); // not limiting very well otherwise

                const float flLimitedValue = tanhf(x0) * flHeadroomGain;
                //return x0 * (1.f - flEnvelopeValue) + flLimitedValue * flEnvelopeValue;
                return x0 - flEnvelopeValue * (x0 + flLimitedValue);
            }
        }
    };

    TwoPoleFilter TwoPoleButterworthLPFConfig(
        const float flCuttoffPercent)
    {
        const float flFilterCutoff = 2.f * (float)M_PI * flCuttoffPercent;
        const float flFilterCutoffCos = cosf(flFilterCutoff);
        const float flFilterAlpha = sinf(flFilterCutoff) / (float)M_SQRT2;
        const float flFilterInvA0 = 1.f / (1 + flFilterAlpha);
        return TwoPoleFilter(
            (0.5f * (1 - flFilterCutoffCos)) * flFilterInvA0,
            (1 - flFilterCutoffCos) * flFilterInvA0,
            (0.5f * (1 - flFilterCutoffCos)) * flFilterInvA0,
            (-2 * flFilterCutoffCos)  * flFilterInvA0,
            (1 - flFilterAlpha)* flFilterInvA0);
    }

    TwoPoleFilter TwoPoleButterworthHPFConfig(
        const float flCuttoffPercent)
    {
        const float flFilterCutoff = 2.f * (float)M_PI * flCuttoffPercent;
        const float flFilterCutoffCos = cosf(flFilterCutoff);
        const float flFilterAlpha = sinf(flFilterCutoff) / (float)M_SQRT2;
        const float flFilterInvA0 = 1.f / (1 + flFilterAlpha);
        return TwoPoleFilter(
            (0.5f * (1 + flFilterCutoffCos)) * flFilterInvA0,
            -(1 + flFilterCutoffCos) * flFilterInvA0,
            (0.5f * (1 + flFilterCutoffCos)) * flFilterInvA0,
            (-2 * flFilterCutoffCos)  * flFilterInvA0,
            (1 - flFilterAlpha)* flFilterInvA0);
    }

    TwoPoleFilter TwoPoleButterworthHighShelfConfig(
        const float flCuttoffPercent,
        const float flGainDB)
    {

        const float flAmp = powf(10.f, 4.f / 40.f);
        const float flAmpSqrt2 = 2.f * sqrtf(flAmp);

        const float flFilterCutoff = 2.f * (float)M_PI * flCuttoffPercent;
        const float flOmegaCos = cosf(flFilterCutoff);
        const float flOmegaSin = sinf(flFilterCutoff);
        const float flAlpha = flOmegaSin / (float)M_SQRT2;

        const float flFilterInvA0 = 1.f / ((flAmp + 1) - (flAmp - 1)*flOmegaCos + flAmpSqrt2 * flAlpha);
        const float a1 = 2 * ((flAmp - 1) - (flAmp + 1) * flOmegaCos);
        const float a2 = (flAmp + 1) - (flAmp - 1) * flOmegaCos - flAmpSqrt2 * flAlpha;
        const float b0 = flAmp*((flAmp + 1) + (flAmp - 1)*flOmegaCos + flAmpSqrt2 * flAlpha);
        const float b1 = -2 * flAmp *((flAmp - 1) + (flAmp + 1) * flOmegaCos);
        const float b2 = flAmp * ((flAmp + 1) + (flAmp - 1) * flOmegaCos - flAmpSqrt2 * flAlpha);

        return TwoPoleFilter(
            b0 * flFilterInvA0,
            b1 * flFilterInvA0,
            b2 * flFilterInvA0,
            a1 * flFilterInvA0,
            a2 * flFilterInvA0);
    }

    class TwoStageKWeightFilter
    {
    private:
        TwoPoleFilter tStage1;
        TwoPoleFilter tStage2;
    public:

        TwoStageKWeightFilter(
            const uint32_t dwSampleRate) :
            tStage1(TwoPoleButterworthHighShelfConfig(1500.f / dwSampleRate, 4.0f)),
            tStage2(TwoPoleButterworthHPFConfig(53.897f / dwSampleRate))
        {}

        float Process(const float x0)
        {
            return tStage2.Process(tStage1.Process(x0));
        }

        void Reset()
        {
            tStage1.Reset();
            tStage2.Reset();
        }
    };

    struct Step
    {
        float flSumSquares;
    };

    struct Block
    {
        float flLoudnessK;
        float flSumSquares;
    };

    enum EBUMeasurement
    {
        EBU_LUFS,
        EBU_LRA
    };

    template<uint32_t BlockDurationMs, EBUMeasurement EBUType, uint32_t StepDurationMs = 100>
    struct SlidingWindow
    {
        static const uint32_t dwNumberSteps = BlockDurationMs / StepDurationMs;

        const float flGammaAbs = -70.f;
        const float flRelativeThrehsold = constexpr (EBUType == EBU_LUFS ? -10.f : -20.f);

        SlidingWindow(
            uint32_t dwSampleRate,
            uint32_t dwSamples)
        {
            dwSamplesPerBlock = BlockDurationMs * dwSampleRate / 1000;
            dwSamplesPerStep = StepDurationMs * dwSampleRate / 1000;

            vBlocks.reserve(dwSamples / dwSamplesPerStep + 1); // 10 100ms steps/second for 3 minutes
            std::fill(arSteps.begin(), arSteps.end(), Step{ 0.f });

            Reset();
        }

        void Reset()
        {
            vBlocks.clear();
            dwStepIndex = 0;
            ptCurrentStep = &arSteps[0];
            dwSampleIndexInternal = dwSampleIndex = 0;
            flTotalBlockSum = 0.f;
        }

        uint32_t dwSamplesPerBlock;
        uint32_t dwSamplesPerStep;

        std::vector<Block> vBlocks;
        std::array<Step, dwNumberSteps> arSteps;
        Step* ptCurrentStep;
        uint32_t dwStepIndex;
        uint32_t dwSampleIndex;
        uint32_t dwSampleIndexInternal;
        float flTotalBlockSum;

        float flLoudnessKAbs;
        float flGammaRel;
        union
        {
            float flLoudnessKGated;
            float flLoudnessRangeLU;
        } tCompute;
        float flLoudnessRangeKLow;
        float flLoudnessRangeKHigh;

        void Process(const float flSquare)
        {
            ptCurrentStep->flSumSquares += flSquare;
            ++dwSampleIndex;
            if (dwSampleIndex % dwSamplesPerStep == 0)
            {
                if (dwStepIndex >= dwNumberSteps - 1)
                {
                    float flBlockSum = 0.f;
                    for (const Step& step : arSteps)
                    {
                        flBlockSum += step.flSumSquares;
                    }
                    const float flPower = flBlockSum / dwSamplesPerBlock;
                    const float flLoudnessK = -0.691f + 10.f * log10f(2.f * flPower);
                    if (flLoudnessK > flGammaAbs)
                    {
                        vBlocks.emplace_back(Block{ flLoudnessK, flBlockSum });
                        flTotalBlockSum += flBlockSum;
                    }
                }

                ++dwStepIndex;
                ptCurrentStep = &arSteps[dwStepIndex % dwNumberSteps];
                ptCurrentStep->flSumSquares = 0.f;
            }            
        }

        void ProcessStep(const float flSquares)
        {
            ptCurrentStep->flSumSquares = flSquares;
            if (dwStepIndex >= dwNumberSteps - 1)
            {
                float flBlockSum = 0.f;
                for (const Step& step : arSteps)
                {
                    flBlockSum += step.flSumSquares;
                }
                const float flPower = flBlockSum / dwSamplesPerBlock;
                const float flLoudnessK = -0.691f + 10.f * log10f(2.f * flPower);
                if (flLoudnessK > flGammaAbs)
                {
                    vBlocks.emplace_back(Block{ flLoudnessK, flBlockSum });
                    flTotalBlockSum += flBlockSum;
                }
            }

            ++dwStepIndex;
            ptCurrentStep = &arSteps[dwStepIndex % dwNumberSteps];
            ptCurrentStep->flSumSquares = 0.f;
        }

        void Process4(const float flSquares)
        {
            ptCurrentStep->flSumSquares += flSquares;
            //dwSampleIndex += 4;
            dwSampleIndexInternal += 4;
            if (dwSampleIndexInternal == dwSamplesPerStep)
            {
                dwSampleIndexInternal = 0;
                if (dwStepIndex >= dwNumberSteps - 1)
                {
                    float flBlockSum = 0.f;
                    for (const Step& step : arSteps)
                    {
                        flBlockSum += step.flSumSquares;
                    }
                    const float flPower = flBlockSum / dwSamplesPerBlock;
                    const float flLoudnessK = -0.691f + 10.f * log10f(2.f * flPower);
                    if (flLoudnessK > flGammaAbs)
                    {
                        vBlocks.emplace_back(Block{ flLoudnessK, flBlockSum });
                        flTotalBlockSum += flBlockSum;
                    }
                }

                ++dwStepIndex;
                ptCurrentStep = &arSteps[dwStepIndex % dwNumberSteps];
                ptCurrentStep->flSumSquares = 0.f;
            }
        }

        static float PowerToLoudnessK(const float flPower)
        {
            return -0.691f + 10.f * log10f(2.f * flPower);
        }

        float LoudnessKAbs() const
        {
            const uint32_t dwBlockCount = vBlocks.size();
            const float flPower = flTotalBlockSum / (dwBlockCount * dwSamplesPerBlock);
            return PowerToLoudnessK(flPower);
        }

        void Compute()
        {
            flLoudnessKAbs = LoudnessKAbs();
            flGammaRel = flLoudnessKAbs + flRelativeThrehsold;

            float flTotalBlockSumRel = 0.f;
            uint32_t dwBlockCountRel = 0;

            switch (EBUType)
            {
            case EBU_LUFS:
            {
                for (const Block& tBlock : vBlocks)
                {
                    if (tBlock.flLoudnessK > flGammaRel)
                    {
                        ++dwBlockCountRel;
                        flTotalBlockSumRel += tBlock.flSumSquares;
                    }
                }

                const float flPowerGated = flTotalBlockSumRel / (dwBlockCountRel * dwSamplesPerBlock);
                tCompute.flLoudnessKGated = PowerToLoudnessK(flPowerGated);
            }
            break;
            case EBU_LRA:
            {
                if (vBlocks.size() > 1)
                {
                    const auto itEnd = std::remove_if(vBlocks.begin(), vBlocks.end(), [&](const Block& tBlock) { return tBlock.flLoudnessK < flGammaRel; });
                    std::sort(vBlocks.begin(), itEnd, [](const Block& lhs, const Block& rhs) { return lhs.flLoudnessK < rhs.flLoudnessK; });

                    dwBlockCountRel = itEnd - vBlocks.begin();

                    const float flPercentLow = 0.1f;
                    const float flPercentHigh = 0.95f;

                    const uint32_t dwIndexLow = (uint32_t)roundf((dwBlockCountRel - 1)*flPercentLow);
                    const uint32_t dwIndexHigh = (uint32_t)roundf((dwBlockCountRel - 1)*flPercentHigh);

                    flLoudnessRangeKLow = vBlocks[dwIndexLow].flLoudnessK;
                    flLoudnessRangeKHigh = vBlocks[dwIndexHigh].flLoudnessK;

                    tCompute.flLoudnessRangeLU = vBlocks[dwIndexHigh].flLoudnessK - vBlocks[dwIndexLow].flLoudnessK;
                }
                else
                {
                    tCompute.flLoudnessRangeLU = 0.f;
                }
            }
            break;
            }
        }

        float LUFS() const
        {
            assert(EBUType == EBU_LUFS);
            return tCompute.flLoudnessKGated;
        }

        float LRA() const
        {
            assert(EBUType == EBU_LRA);
            return tCompute.flLoudnessRangeLU;
        }
    };

    typedef SlidingWindow<3000, EBU_LRA> WindowLRA;
    typedef SlidingWindow<400, EBU_LUFS> WindowLUFS;

    struct WaveFormat
    {
        uint32_t dwBitRate;
        uint32_t dwChannels;
        uint32_t dwSampleRate;
    };

    struct NormalizationParams
    {
        float flLoudnessTargetLUFS;
        float flMaxLoudnessRangeLU;
        float flCompressorSlope;
        float flLookAheadMs;
    };

    class AudioSource
    {
    private:
        uint8_t* const ptSampleStartPos;
        const uint8_t* ptReadPos;
        uint8_t* ptWritePos;
        const uint32_t dwByteRate;        
        const uint32_t dwChannels;
        const uint32_t dwSampleRate;
        const uint32_t dwReadRate;
        const uint32_t dwSampleMask;

        uint32_t dwReadSample;
        uint32_t dwWriteSample;
        uint32_t dwSamples;        
        const float flSampleMaxValue;
        const float flSampleRatio;
        const __m128 mConst;
    public:
        AudioSource() :
            ptSampleStartPos(nullptr),
            ptReadPos(nullptr),
            ptWritePos(nullptr),
            dwByteRate(0),
            dwChannels(0),
            dwSampleRate(0),
            dwReadSample(0),
            dwWriteSample(0),
            dwSamples(0),
            flSampleMaxValue(0.f),
            dwReadRate(0),
            flSampleRatio(1.f / (float)((uint32_t)1 << (uint32_t)31)),
            mConst(_mm_load1_ps(&flSampleRatio)),
            dwSampleMask(0)
        {}

        AudioSource(
            const WaveFormat& _tFormat,
            uint8_t* _ptBuffer,
            const uint32_t _dwBufferSize) :
            ptSampleStartPos(_ptBuffer),
            ptReadPos(_ptBuffer),
            ptWritePos(_ptBuffer),
            dwChannels(_tFormat.dwChannels),
            dwSampleRate(_tFormat.dwSampleRate),
            dwByteRate(_tFormat.dwBitRate / 8),
            flSampleMaxValue((float)((1 << (_tFormat.dwBitRate - 1)) - 1)),
            flSampleRatio(1.f / (float)((uint32_t)1 << (uint32_t)31)),
            mConst(_mm_load1_ps(&flSampleRatio)),
            dwReadRate(dwChannels * dwByteRate),
            dwSampleMask(dwByteRate == 3 ? 8 : 16)
        {
            dwSamples = (_dwBufferSize / dwByteRate) / _tFormat.dwChannels;
            dwReadSample = 0;
            dwWriteSample = 0;
        }

        uint32_t Samples() const
        {
            return dwSamples;
        }

        uint32_t SampleRate() const
        {
            return dwSampleRate;
        }

        bool CanRead(const uint32 dwCount = 1) const
        {
            return dwReadSample + dwCount <= dwSamples;
        }

#ifdef PROFILE_ENABLE_NO_INLINE
        __declspec(noinline)
#endif // PROFILE_ENABLE_NO_INLINE        
        void Read4(float (&arValues)[4])
        {
            alignas(16) int32_t arInts[4];
            for (int i = 0; i < 4; ++i)
            {
                arInts[i] = (int32_t)((*(const uint32_t*)ptReadPos) << dwSampleMask);
                ptReadPos += dwReadRate;
            }
            
            __m128i mInts = _mm_load_si128((__m128i*)&arInts[0]);
            __m128 dst = _mm_cvtepi32_ps(mInts);
            dst = _mm_mul_ps(dst, mConst);
            _mm_store_ps(&arValues[0], dst);

            dwReadSample += 4;            
        }

        float Read()
        {
            int32_t dwValue = 0;
            memcpy(((uint8_t*)&dwValue) + (4 - dwByteRate), ptReadPos, dwByteRate);
            ptReadPos += dwReadRate;

            const float flSample = dwValue * flSampleRatio;
            ++dwReadSample;
            return flSample;
        }

        bool CanWrite() const
        {
            return dwWriteSample < dwSamples;
        }

        void Write(const float flOutput)
        {
            const int32_t dwValue = flOutput * flSampleMaxValue;
            *(int32_t*)ptWritePos = dwValue;            
            ptWritePos += dwReadRate;

            ++dwWriteSample;
        }

        void Reset()
        {
            ptReadPos = ptSampleStartPos;
            dwReadSample = 0;

            ptWritePos = ptSampleStartPos;
            dwWriteSample = 0;
        }
    };

    // See ITU-R BS.1770-4
    // The Below functions can be used to generate the filter coefficients for 48000kHz.
    //TwoPoleFilter tStage1(1.5351249f, -2.6916961f, 1.1983928f, -1.69065929f, 0.732480f);
    //TwoPoleFilter tStage2(1.0f, -2.0f, 1.0f, -1.990047f, 0.99007225f);
    namespace ReproduceEBUR128Numbers
    {
        float FindFrequency()
        {
            double flA02 = 0.99007225036621;
            double flAlpha = (1.0 - flA02) / (1.0 + flA02);
            double flFreq = asin(flAlpha * M_SQRT2) * 48000 * 0.5 * M_1_PI;
            return (float)flFreq;
            // flFreq = 53.8966713
        }

        float FindHighPassFreq()
        {
            const double flAmp = pow(10.0, 4.0 / 40.0);
            const double flAmpSqrt2 = 2.0 * sqrt(flAmp);
            const double flTargetB0 = 1.53512485958697;
            const double flTargetA1 = -1.690659293182;
            const double flTargetA2 = 0.73248077421585;
            double flBestFrequency = 500.0;
            double flBestErrorSq = FLT_MAX;

            for (double flFrequency = flBestFrequency; flFrequency < 5000.0; flFrequency += 0.001)
            {
                const double flOmega = 2 * M_PI * flFrequency / 48000.0;
                const double flOmegaCos = cos(flOmega);
                const double flOmegaSin = sin(flOmega);
                const double flAlpha = flOmegaSin / M_SQRT2;

                const double a0Inv = 1.0 / ((flAmp + 1) - (flAmp - 1)*flOmegaCos + flAmpSqrt2 * flAlpha);
                const double a1 = 2 * ((flAmp - 1) - (flAmp + 1) * flOmegaCos);
                const double a2 = (flAmp + 1) - (flAmp - 1) * flOmegaCos - flAmpSqrt2 * flAlpha;
                const double b0 = flAmp*((flAmp + 1) + (flAmp - 1)*flOmegaCos + flAmpSqrt2 * flAlpha);

                double flErrorA1 = (a1 * a0Inv) - flTargetA1;
                double flErrorA2 = (a2 * a0Inv) - flTargetA2;
                double flErrorB0 = (b0 * a0Inv) - flTargetB0;

                double flErrorSq = flErrorA1 * flErrorA1 + flErrorA2 * flErrorA2 + flErrorB0 * flErrorB0;
                if (flErrorSq < flBestErrorSq)
                {
                    flBestErrorSq = flErrorSq;
                    flBestFrequency = flFrequency;
                }
            }

            return (float)flBestFrequency;
        }
    }

    //void TestReadWriteInPlace(AudioSource &tSource, AudioSource &tCopy)
    //{
    //    while (tSource.CanRead())
    //    {
    //        assert(tSource.CanWrite());
    //        assert(tCopy.CanRead());
    //        const float flSample = tSource.Read();
    //        tSource.Write(flSample);
    //        const float flNewValue = tCopy.Read();
    //        assert(fabsf(flNewValue - flSample) < FLT_EPSILON);
    //    }
    //}


    template<typename T>
    void MeasureKWeightedLoudness(
        T &tSource,
        WindowLUFS &tMomentaryWindow2,
        WindowLRA &tShortTermWindow2,
        TwoStageKWeightFilter& tFilter,
        const uint32_t dwMinimumDurationMs = 3000)
    {
#ifdef PROFILE_ENABLE
        BROFILER_CATEGORY("Measure", Profiler::Color::YellowGreen);
#endif // PROFILE_ENABLE

        const uint32_t dwMinimumSamples = tSource.SampleRate() * dwMinimumDurationMs / 1000;
        const uint32_t dwSamples = max(dwMinimumSamples, tSource.Samples());

        alignas(16) float arSampleSquares[4];
        __m128 dst;
        for (uint32_t i = 0; i < dwSamples; i += tMomentaryWindow2.dwSamplesPerStep)
        {
            float flAccumulator = 0.f;
            if (i + tMomentaryWindow2.dwSamplesPerStep > dwSamples)
            {
                break;
            }
            for (uint32_t j = 0; j < tMomentaryWindow2.dwSamplesPerStep; j += 4)
            {
                if (!tSource.CanRead(4))
                {
                    tSource.Reset();
                }
                tSource.Read4(arSampleSquares);

                for (uint32 k = 0; k < 4; ++k)
                {
                    //const float flSample = tSource.Read();
                    const float flSampleK = tFilter.Process(arSampleSquares[k]);
                    arSampleSquares[k] = flSampleK;
                }

                dst = _mm_load_ps(&arSampleSquares[0]);
                dst = _mm_dp_ps(dst, dst, 0xF1);
                flAccumulator += _mm_cvtss_f32(dst);
            }
            tMomentaryWindow2.ProcessStep(flAccumulator);
            tShortTermWindow2.ProcessStep(flAccumulator);
        }
        //for (uint32_t i = 0; i < dwSamples; ++i)
        //{
        //    if (!tSource.CanRead())
        //    {
        //        tSource.Reset();
        //    }

        //    const float flSample = tSource.Read();
        //    float flSampleK = tFilter.Process(flSample);

        //    const float flSquare = flSampleK * flSampleK;
        //    tMomentaryWindow2.Process(flSquare);
        //    tShortTermWindow2.Process(flSquare);
        //}
    }

    struct NormalizationResult
    {
        struct Loudness
        {
            float flLoudnessKGated = 0.f;
            float flLoudnessRangeLU = 0.f;
        };

        Loudness tPreNorm;
        Loudness tPostNorm;
        float flGainApplied;
        bool bLimiterPeaked;
    };

    template<typename T, bool bLogToConsole = false>
    class LoudnessNormalization
    {
        const NormalizationParams tParams;
        char buff[256]; // debug output
        
        void NormalizeAudioInPlace(
            T &tSource,
            NormalizationResult &tResult,
            TwoStageKWeightFilter &tFilter,
            const float flLoudnessRangeKLow);
    public:
        LoudnessNormalization(
            const NormalizationParams& _tParams) :
            tParams(_tParams)
        {}

        void Process(
            AudioSource& tSource,
            NormalizationResult& tResult)
        {
#ifdef PROFILE_ENABLE
            BROFILER_CATEGORY("Process", Profiler::Color::Bisque);
#endif // PROFILE_ENABLE
            std::fill_n((uint8_t*)(void*)&tResult, sizeof(tResult), 0);
            const uint32_t dwSampleRate = tSource.SampleRate();

            WindowLUFS tMomentaryWindow(dwSampleRate, tSource.Samples());
            WindowLRA tShortTermWindow(dwSampleRate, tSource.Samples());
            TwoStageKWeightFilter tFilter(dwSampleRate);

            MeasureKWeightedLoudness(tSource, tMomentaryWindow, tShortTermWindow, tFilter);

            tMomentaryWindow.Compute();
            tResult.tPreNorm.flLoudnessKGated = tMomentaryWindow.LUFS();
            assert(tResult.tPreNorm.flLoudnessKGated <= 0.f);

            tShortTermWindow.Compute();
            tResult.tPreNorm.flLoudnessRangeLU = tShortTermWindow.LRA();
            assert(tResult.tPreNorm.flLoudnessRangeLU > -FLT_EPSILON);

            if (bLogToConsole)
            {
                const DWORD idThread = GetCurrentThreadId();
                sprintf_s(buff, "Pre %x: %.5f LUFS, +/-%.5f LU\n", idThread, tResult.tPreNorm.flLoudnessKGated, tResult.tPreNorm.flLoudnessRangeLU);
                OutputDebugString(buff);
            }

            tFilter.Reset();
            tSource.Reset();
            NormalizeAudioInPlace(tSource, tResult, tFilter, tShortTermWindow.flLoudnessRangeKLow);

            // Measure again
            {
                tMomentaryWindow.Reset();
                tShortTermWindow.Reset();

                tFilter.Reset();
                tSource.Reset();
                MeasureKWeightedLoudness(tSource, tMomentaryWindow, tShortTermWindow, tFilter);

                tMomentaryWindow.Compute();
                tShortTermWindow.Compute();

                tResult.tPostNorm.flLoudnessKGated = tMomentaryWindow.LUFS();
                tResult.tPostNorm.flLoudnessRangeLU = tShortTermWindow.LRA();

                if (bLogToConsole)
                {
                    const DWORD idThread = GetCurrentThreadId();
                    sprintf_s(buff, "Pst %x: %.5f LUFS, +/-%.5f LU\n", idThread, tMomentaryWindow.LUFS(), tShortTermWindow.LRA());
                    OutputDebugString(buff);
                }
            }
        }
    };

    template<typename T, bool bLogToConsole /*= false*/>    
    void LoudnessNormalization<T, bLogToConsole>::NormalizeAudioInPlace(
        T &tSource,
        NormalizationResult &tResult,
        TwoStageKWeightFilter &tFilter,
        const float flLoudnessRangeKLow)
    {
#ifdef PROFILE_ENABLE
        BROFILER_CATEGORY("Normalize", Profiler::Color::RoyalBlue);
#endif // PROFILE_ENABLE

        const uint32_t dwSampleRate = tSource.SampleRate();

        float flGain = 1.f;
        if (fabsf(tResult.tPreNorm.flLoudnessKGated - tParams.flLoudnessTargetLUFS) > 0.1f)
        {
            flGain = DBToGain(tParams.flLoudnessTargetLUFS - tResult.tPreNorm.flLoudnessKGated);
        }

        bool bCompress = false;
        if (tResult.tPreNorm.flLoudnessRangeLU - tParams.flMaxLoudnessRangeLU > 0.1f)
        {
            // DBToGain(delta / slope ) / DBToGain(delta) simplified.
            const float flMaxCompressorAdjustment = DBToGain((tResult.tPreNorm.flLoudnessRangeLU - tParams.flMaxLoudnessRangeLU) * (1.f / tParams.flCompressorSlope - 1.f));
            flGain *= 1.f / flMaxCompressorAdjustment;
            bCompress = true;
        }

        tResult.flGainApplied = flGain;

        const float flCompressorThreshold = flLoudnessRangeKLow + tParams.flMaxLoudnessRangeLU;
        Compressor tCompressor(flCompressorThreshold, tParams.flCompressorSlope, (float)dwSampleRate, 20.f, 200.f);
        const uint32_t dwLookAheadSamples = (uint32_t)(tParams.flLookAheadMs * dwSampleRate / 1000.f);
        Limiter tLimiter((float)dwSampleRate, tParams.flLookAheadMs, 5.f, -0.1f, 2.f * FLT_EPSILON);

        std::vector<float> vLookAheadBuffer(dwLookAheadSamples);
        uint32_t dwLookAheadIndex = 0;

        alignas(16) float arSamples[4];
        while (tSource.CanRead(4))
        {
            tSource.Read4(arSamples);
            for (int i = 0; i < 4; ++i)
            {
                const float flSample = arSamples[i];

                float flTotalGain = flGain;
                if (bCompress)
                {
                    const float flSampleK = tFilter.Process(flSample);
                    float flCompressorGain = 1.f;
                    if (tCompressor.Process(flSampleK, flCompressorGain))
                    {
                        flTotalGain *= flCompressorGain;
                    }
                }

                const float flOutputSample = flSample * flTotalGain;
                vLookAheadBuffer[dwLookAheadIndex % dwLookAheadSamples] = flOutputSample;
                ++dwLookAheadIndex;
                if (dwLookAheadIndex >= dwLookAheadSamples)
                {
                    assert(tSource.CanWrite());
                    float flSampleToProcess = vLookAheadBuffer[dwLookAheadIndex % dwLookAheadSamples];
                    const float flSampleToWrite = tLimiter.Process(flSampleToProcess, flOutputSample);
                    tSource.Write(flSampleToWrite);
                }
                else
                {
                    tLimiter.Process(0.f, flOutputSample);
                }
            }
        }
        // Finish Lookahead
        while (tSource.CanWrite())
        {
            ++dwLookAheadIndex;
            float flSampleToProcess = vLookAheadBuffer[dwLookAheadIndex % dwLookAheadSamples];
            const float flSampleToWrite = tLimiter.Process(flSampleToProcess, 0.f);
            tSource.Write(flSampleToWrite);
        }

        tResult.bLimiterPeaked = tLimiter.Peaked();
    }

    //void Old()
    //{
    //    const __m128 miGains = _mm_load1_ps(&flGain);

    //    alignas(16) float arSamples[4];
    //    alignas(16) float arCompressorGains[4];
    //    while (tSource.CanRead(4))
    //    {
    //        //const float flSample = tSource.Read();
    //        tSource.Read4(arSamples);
    //        bool bCompressed = false;
    //        for (int i = 0; i < 4; ++i)
    //        {
    //            if (bCompress)
    //            {
    //                float flSampleK = tFilter.Process(arSamples[i]);
    //                bCompressed |= tCompressor.Process(flSampleK, arCompressorGains[i]);
    //            }
    //        }

    //        __m128 miTotalGains = miGains;
    //        if (bCompressed)
    //        {
    //            __m128 miCompressorGains = _mm_load_ps(&arCompressorGains[0]);
    //            miTotalGains = _mm_mul_ps(miTotalGains, miCompressorGains);
    //        }

    //        //const float flOutputSample = flSample * flTotalGain;
    //        __m128 miOutputSamples = _mm_load_ps(&arSamples[0]);
    //        miOutputSamples = _mm_mul_ps(miOutputSamples, miTotalGains);
    //        _mm_store_ps(&arSamples[0], miOutputSamples);

    //        for (int i = 0; i < 4; ++i)
    //        {
    //            const float flOutputSample = arSamples[i];
    //            vLookAheadBuffer[dwLookAheadIndex % dwLookAheadSamples] = flOutputSample;
    //            ++dwLookAheadIndex;
    //            if (dwLookAheadIndex >= dwLookAheadSamples)
    //            {
    //                assert(tSource.CanWrite());
    //                float flSampleToProcess = vLookAheadBuffer[dwLookAheadIndex % dwLookAheadSamples];
    //                const float flSampleToWrite = tLimiter.Process(flSampleToProcess, flOutputSample);
    //                tSource.Write(flSampleToWrite);
    //            }
    //            else
    //            {
    //                tLimiter.Process(0.f, flOutputSample);
    //            }
    //        }
    //    }
    //}

    struct LoudnessNormalizationRequest
    {
        const NormalizationParams tParams;
        NormalizationResult tResult;

        const char* szFileNameIn;
        const char* szFileNameOut;
    };

    AudioSource GetAudioSource(
        const char* szFileName,
        uint32_t& out_dwBytesRead,
        uint8_t* parIOBuffer,
        const uint32_t dwIOBufferSize)
    {
#ifdef PROFILE_ENABLE
        BROFILER_CATEGORY("Read File", Profiler::Color::RoyalBlue);
#endif // PROFILE_ENABLE

        WaveFormat tFormat;
        DWORD dwBytesRead = 0;
        BOOL bReadComplete = FALSE;

        HANDLE hFileInput = CreateFile(szFileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFileInput != INVALID_HANDLE_VALUE)
        {
            bReadComplete = ReadFile(hFileInput, (LPVOID)parIOBuffer, dwIOBufferSize, &dwBytesRead, nullptr);
            CloseHandle(hFileInput);
        }

        out_dwBytesRead = (uint32_t)dwBytesRead;

        if (!bReadComplete ||
            !dwBytesRead)
        {
            return AudioSource();
        }

        bool bEof = dwBytesRead < dwIOBufferSize;
        uint8_t* ptReadPos = parIOBuffer;

        bool finished = false;
        bool readRIFF = false;
        bool readFormat = false;
        bool dataFound = false;
        while (!dataFound)
        {
            const uint32_t chunkId = *(uint32_t*)ptReadPos;
            ptReadPos += 4;
            const uint32_t chunkSize = *(uint32_t*)ptReadPos;
            ptReadPos += 4;
            switch (chunkId)
            {
            case (uint32_t)('R' | ('I' << 8) | ('F' << 16) | ('F' << 24)) :
                ptReadPos += 4;
                readRIFF = true;
                break;
            case (uint32_t)('f' | ('m' << 8) | ('t' << 16) | (' ' << 24)) :
            {
                uint8_t* ptFormatPos = ptReadPos;
                const uint16_t uFormatTag = *(decltype(uFormatTag)*)ptFormatPos;
                assert(uFormatTag == 0x0001);
                ptFormatPos += sizeof(uFormatTag);

                const uint16_t uChannels = *(decltype(uChannels)*)ptFormatPos;
                assert(uChannels == 1 ||
                    uChannels == 2);
                tFormat.dwChannels = uChannels;
                ptFormatPos += sizeof(uChannels);

                const uint32_t dwSampleRate = *(decltype(dwSampleRate)*)ptFormatPos;
                assert(dwSampleRate <= 48000);
                tFormat.dwSampleRate = dwSampleRate;
                ptFormatPos += sizeof(dwSampleRate);

                ptFormatPos += 6; // skip stuff

                const uint16_t uBitRate = *(decltype(uBitRate)*)ptFormatPos;
                assert(uBitRate == 16 ||
                    uBitRate == 24);
                tFormat.dwBitRate = uBitRate;

                ptReadPos += chunkSize;
                readFormat = true;
            }
                                                                          break;
            case (uint32_t)('d' | ('a' << 8) | ('t' << 16) | ('a' << 24)) :
                assert(readRIFF && readFormat);
                ptReadPos -= 8;
                dataFound = true;
                break;
            default:
                ptReadPos += chunkSize;
                break;
            }
        }
        const DWORD dwHeaderSize = (DWORD)(ptReadPos - parIOBuffer);

        const uint32_t dataId = *(uint32_t*)ptReadPos;
        assert(dataId == (uint32_t)('d' | ('a' << 8) | ('t' << 16) | ('a' << 24)));
        ptReadPos += 4;
        const uint32_t dataSize = *(uint32_t*)ptReadPos;
        ptReadPos += 4;

        return AudioSource(tFormat, ptReadPos, uint32_t(dwBytesRead - dwHeaderSize));
    }

    class LoudnessNormalizationJob
    {
        std::vector<LoudnessNormalizationRequest> vRequests;
        std::vector<std::thread> vThreads;
        uint32_t dwThreadsToRun;
        std::atomic_uint32_t adwNextRequestIndex;
        std::atomic_uint32_t adwFinishedThreads;
        bool bWorking;

        void DoWork(uint32_t dwStartIndex)
        {
            static const uint32_t dwIOBufferSize = 8 * 4096 * 1000;
            uint8_t* parIOBuffer = new uint8_t[dwIOBufferSize + 256];

            const uint32_t dwNumRequests = (uint32_t)vRequests.size();
            uint32_t dwRequestIndex = dwStartIndex;

            while (dwRequestIndex < dwNumRequests)
            {
                if (dwRequestIndex < dwNumRequests)
                {
                    LoudnessNormalizationRequest& tRequest = vRequests[dwRequestIndex];
                    ProcessSingle(tRequest, parIOBuffer, dwIOBufferSize);
                }
                dwRequestIndex = adwNextRequestIndex.fetch_add(1);
            }

            delete[] parIOBuffer;
            adwFinishedThreads.fetch_add(1);
        }

    public:
        LoudnessNormalizationJob() :
            bWorking(false),
            dwThreadsToRun(0)
        {
            adwNextRequestIndex.store(0);
            adwFinishedThreads.store(0);
        }

        void AddRequest(
            const LoudnessNormalizationRequest& tRequest)
        {
            assert(!bWorking);
            vRequests.emplace_back(tRequest);
        }

        void Process()
        {
            const uint32_t dwNumRequests = (uint32_t)vRequests.size();
            if (dwNumRequests == 0)
            {
                return;
            }

            bWorking = true;
            if (dwNumRequests == 1)
            {
                DoWork(0);
                return;
            }

            const uint32_t dwMaxThreads = std::thread::hardware_concurrency();
            const uint32_t dwReservedThreads = 2;
            dwThreadsToRun = min(((int32_t)dwMaxThreads - (int32_t)dwReservedThreads) > 0 ? (dwMaxThreads - dwReservedThreads) : 2, dwNumRequests);

            adwNextRequestIndex.store(dwThreadsToRun - 1);
            adwFinishedThreads.store(0);

            for (uint32_t i = 0; i < dwThreadsToRun; ++i)
            {
                vThreads.emplace_back(std::thread([this, i]
                {
                    DoWork(i);
                }));
            }

            return;
        }

        bool Finished() const
        {
            uint32_t dwFinishedThreads = adwFinishedThreads.load();
            return bWorking &&
                dwFinishedThreads >= dwThreadsToRun;
        }

        static void ProcessSingle(
            LoudnessNormalizationRequest& tRequest,
            uint8_t* parIOBuffer,
            const uint32_t dwIOBufferSize)
        {
            uint32_t dwBytesRead = 0;
            AudioSource tSource = GetAudioSource(tRequest.szFileNameIn, dwBytesRead, parIOBuffer, dwIOBufferSize);
            if (!tSource.CanRead())
            {
                return;
            }

            LoudnessNormalization <decltype(tSource), true > tNormalize(tRequest.tParams);
            tNormalize.Process(tSource, tRequest.tResult);

            HANDLE hFileOutput = CreateFile(tRequest.szFileNameOut, GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hFileOutput != INVALID_HANDLE_VALUE)
            {
                DWORD dwBytesWritten = 0;
                BOOL bWriteComplete = WriteFile(hFileOutput, parIOBuffer, dwBytesRead, &dwBytesWritten, nullptr);
                assert(bWriteComplete && dwBytesWritten == dwBytesRead);
                CloseHandle(hFileOutput);
            }
        }
    };
}

const char* g_szTestRequests[][2] = 
{
    {
        "C:\\Programming\\VolumeNormalization\\seq-3341-2011-8_seq-3342-6-24bit-v02.wav",
        "C:\\Programming\\VolumeNormalization\\Processed\\seq-3341-2011-8_seq-3342-6-24bit-v02.wav"
    }
};


__declspec(noinline) void RunLoudnessProfile()
{
    OutputDebugString("Hello World\n");

#ifdef PROFILE_ENABLE
    BROFILER_FRAME("LoudnessThread")
#endif

    Loudness::NormalizationParams tParams{ -20.f, 10.0f, 2.f, 100.f };
    Loudness::LoudnessNormalizationJob tTestJob;

    Loudness::LoudnessNormalizationRequest tRequest{ tParams, Loudness::NormalizationResult(), nullptr, nullptr };
    const uint32_t dwRequests = sizeof(g_szTestRequests) / sizeof(g_szTestRequests[0]);
    for (int i = 0; i < dwRequests; ++i)
    {
        tRequest.szFileNameIn = g_szTestRequests[i][0];
        tRequest.szFileNameOut = g_szTestRequests[i][1];
        tTestJob.AddRequest(tRequest);
    }

    tTestJob.Process();

    while (!tTestJob.Finished())
    {
#ifdef PROFILE_ENABLE
        BROFILER_FRAME("LoudnessThread")
#endif
        ::Sleep(20);
    }
}

 