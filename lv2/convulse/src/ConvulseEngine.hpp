#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace flues::convulse {

static constexpr uint32_t kMaxKernelSize = 1024;
static constexpr uint32_t kKernelCrossfadeSamples = 128;
static constexpr float kHalfPi = 1.57079632679489661923f;
static constexpr float kTwoPi = 6.28318530717958647692f;

enum class KernelMode : int {
    Sine = 0,
    Saw = 1,
    Pulse = 2,
    Noise = 3,
    FM = 4,
    Chirp = 5
};

struct Params {
    float dryWet = 0.5f;
    float kernelSize = 256.0f;
    float mode = 0.0f;
    float pitch = 220.0f;
    float shape = 0.5f;
    float decay = 0.6f;
    float refresh = 0.5f;
    float stereoWidth = 0.3f;
    float feedback = 10.0f;
    float drive = 0.35f;
};

class Engine {
public:
    explicit Engine(double sampleRate = 48000.0)
        : sampleRate_(sampleRate) {
        reset();
    }

    void setSampleRate(double sampleRate) {
        sampleRate_ = std::max(1.0, sampleRate);
        reset();
    }

    void reset() {
        historyL_.fill(0.0f);
        historyR_.fill(0.0f);
        activeKernelL_.fill(0.0f);
        activeKernelR_.fill(0.0f);
        pendingKernelL_.fill(0.0f);
        pendingKernelR_.fill(0.0f);
        writeIndex_ = 0;
        prevWetL_ = 0.0f;
        prevWetR_ = 0.0f;
        activeKernelSize_ = 0;
        pendingKernelSize_ = 0;
        kernelBlendRemaining_ = 0;
        samplesUntilRefresh_ = std::numeric_limits<uint64_t>::max();
        queueKernelBuild(false);
    }

    void setParams(const Params& incoming) {
        Params clamped = incoming;
        clamped.dryWet = clampf(clamped.dryWet, 0.0f, 1.0f);
        clamped.kernelSize = static_cast<float>(clampi(static_cast<int>(std::lround(clamped.kernelSize)), 32, static_cast<int>(kMaxKernelSize)));
        clamped.mode = static_cast<float>(clampi(static_cast<int>(std::lround(clamped.mode)), 0, 5));
        clamped.pitch = clampf(clamped.pitch, 20.0f, 2000.0f);
        clamped.shape = clampf(clamped.shape, 0.0f, 1.0f);
        clamped.decay = clampf(clamped.decay, 0.0f, 1.0f);
        clamped.refresh = clampf(clamped.refresh, 0.0f, 8.0f);
        clamped.stereoWidth = clampf(clamped.stereoWidth, 0.0f, 1.0f);
        clamped.feedback = clampf(clamped.feedback, 0.0f, 95.0f);
        clamped.drive = clampf(clamped.drive, 0.0f, 2.0f);

        const bool kernelChanged =
            std::fabs(clamped.kernelSize - params_.kernelSize) > 0.5f ||
            std::fabs(clamped.mode - params_.mode) > 0.5f ||
            std::fabs(clamped.pitch - params_.pitch) > 0.001f ||
            std::fabs(clamped.shape - params_.shape) > 0.0001f ||
            std::fabs(clamped.decay - params_.decay) > 0.0001f ||
            std::fabs(clamped.stereoWidth - params_.stereoWidth) > 0.0001f;

        const bool refreshChanged = std::fabs(clamped.refresh - params_.refresh) > 0.0001f;
        params_ = clamped;

        if (kernelChanged || activeKernelSize_ == 0) {
            queueKernelBuild(activeKernelSize_ > 0);
        }

        if (refreshChanged) {
            samplesUntilRefresh_ = std::numeric_limits<uint64_t>::max();
        }
    }

    void processBlock(const float* inL,
                      const float* inR,
                      float* outL,
                      float* outR,
                      uint32_t nframes,
                      bool transportRunning,
                      bool tempoValid,
                      double bpm) {
        if (nframes == 0) {
            return;
        }

        if (activeKernelSize_ == 0) {
            queueKernelBuild(false);
        }

        const float dryWet = params_.dryWet;
        const float drive = params_.drive;
        const float dryMix = std::cos(dryWet * kHalfPi);
        const float wetMix = std::sin(dryWet * kHalfPi);
        const float wetGain = 1.25f + drive * 1.55f;
        const float feedbackGain = params_.feedback * 0.01f * 0.92f * (1.0f + drive * 0.35f);

        for (uint32_t i = 0; i < nframes; ++i) {
            maybeRefreshKernel(transportRunning, tempoValid, bpm);

            const float dryL = inL ? inL[i] : (inR ? inR[i] : 0.0f);
            const float dryR = inR ? inR[i] : dryL;

            const float exciteL = dryL + softClip(prevWetL_ * feedbackGain);
            const float exciteR = dryR + softClip(prevWetR_ * feedbackGain);

            historyL_[writeIndex_] = exciteL;
            historyR_[writeIndex_] = exciteR;

            const float wetActiveL = convolve(historyL_, activeKernelL_, activeKernelSize_);
            const float wetActiveR = convolve(historyR_, activeKernelR_, activeKernelSize_);

            float wetL = wetActiveL;
            float wetR = wetActiveR;

            if (kernelBlendRemaining_ > 0 && pendingKernelSize_ > 0) {
                const float wetPendingL = convolve(historyL_, pendingKernelL_, pendingKernelSize_);
                const float wetPendingR = convolve(historyR_, pendingKernelR_, pendingKernelSize_);
                const float blend = 1.0f - (static_cast<float>(kernelBlendRemaining_) / static_cast<float>(kKernelCrossfadeSamples));
                wetL = wetActiveL + (wetPendingL - wetActiveL) * blend;
                wetR = wetActiveR + (wetPendingR - wetActiveR) * blend;

                --kernelBlendRemaining_;
                if (kernelBlendRemaining_ == 0) {
                    activeKernelL_ = pendingKernelL_;
                    activeKernelR_ = pendingKernelR_;
                    activeKernelSize_ = pendingKernelSize_;
                    pendingKernelSize_ = 0;
                }
            }

            wetL = softClip(wetL * wetGain);
            wetR = softClip(wetR * wetGain);
            prevWetL_ = wetL;
            prevWetR_ = wetR;

            if (outL) {
                outL[i] = (dryL * dryMix) + (wetL * wetMix);
            }
            if (outR) {
                outR[i] = (dryR * dryMix) + (wetR * wetMix);
            }

            writeIndex_ = (writeIndex_ + 1u) % kMaxKernelSize;
        }
    }

private:
    static inline float clampf(float value, float minValue, float maxValue) {
        if (value < minValue) return minValue;
        if (value > maxValue) return maxValue;
        return value;
    }

    static inline int clampi(int value, int minValue, int maxValue) {
        if (value < minValue) return minValue;
        if (value > maxValue) return maxValue;
        return value;
    }

    static inline float softClip(float x) {
        return std::tanh(x);
    }

    static inline float fracf(float x) {
        return x - std::floor(x);
    }

    static inline float randSigned(uint32_t* state) {
        *state = (*state * 1664525u) + 1013904223u;
        const float value = static_cast<float>(*state & 0x00FFFFFFu) * (1.0f / 16777216.0f);
        return value * 2.0f - 1.0f;
    }

    static inline float wrapPhase(float phase) {
        phase = std::fmod(phase, kTwoPi);
        if (phase < 0.0f) {
            phase += kTwoPi;
        }
        return phase;
    }

    float convolve(const std::array<float, kMaxKernelSize>& history,
                   const std::array<float, kMaxKernelSize>& kernel,
                   uint32_t size) const {
        float sum = 0.0f;
        uint32_t index = writeIndex_;
        for (uint32_t tap = 0; tap < size; ++tap) {
            sum += history[index] * kernel[tap];
            index = (index == 0u) ? (kMaxKernelSize - 1u) : (index - 1u);
        }
        return sum;
    }

    float renderModeSample(KernelMode mode,
                           float phase,
                           float auxPhase,
                           float shape,
                           float t,
                           uint32_t* rngState,
                           float* noiseState) const {
        switch (mode) {
            case KernelMode::Sine:
                return std::sin(phase) + 0.35f * shape * std::sin(phase * 2.0f + auxPhase);
            case KernelMode::Saw: {
                const float saw = fracf(phase / kTwoPi) * 2.0f - 1.0f;
                return saw * (1.0f - 0.35f * shape) + std::sin(phase) * (0.20f + 0.35f * shape);
            }
            case KernelMode::Pulse: {
                const float pulseWidth = 0.08f + shape * 0.84f;
                const float frac = fracf(phase / kTwoPi);
                return (frac < pulseWidth ? 1.0f : -1.0f) * (0.85f - 0.15f * shape);
            }
            case KernelMode::Noise: {
                const float target = randSigned(rngState);
                const float smoothing = 0.05f + (1.0f - shape) * 0.45f;
                *noiseState += (target - *noiseState) * smoothing;
                return *noiseState;
            }
            case KernelMode::FM: {
                const float ratio = 0.25f + shape * 4.0f;
                const float index = 0.35f + shape * 8.0f;
                return std::sin(phase + std::sin(auxPhase * ratio + t * kTwoPi) * index);
            }
            case KernelMode::Chirp: {
                const float sweep = 1.0f + t * (1.0f + shape * 6.0f);
                return std::sin(phase * sweep + auxPhase * 0.3f);
            }
        }
        return std::sin(phase);
    }

    void normalizeKernel(std::array<float, kMaxKernelSize>& kernel, uint32_t size) {
        if (size == 0) {
            return;
        }

        float mean = 0.0f;
        for (uint32_t i = 0; i < size; ++i) {
            mean += kernel[i];
        }
        mean /= static_cast<float>(size);

        float absSum = 0.0f;
        for (uint32_t i = 0; i < size; ++i) {
            kernel[i] -= mean;
            absSum += std::fabs(kernel[i]);
        }

        const float scale = (absSum > 1.0e-6f) ? (1.25f / absSum) : 1.0f;
        for (uint32_t i = 0; i < size; ++i) {
            kernel[i] *= scale;
        }
        for (uint32_t i = size; i < kMaxKernelSize; ++i) {
            kernel[i] = 0.0f;
        }
    }

    void renderKernel(std::array<float, kMaxKernelSize>& left,
                      std::array<float, kMaxKernelSize>& right,
                      uint32_t size,
                      uint32_t variantSeed) {
        left.fill(0.0f);
        right.fill(0.0f);

        const KernelMode mode = static_cast<KernelMode>(clampi(static_cast<int>(std::lround(params_.mode)), 0, 5));
        const float baseFreq = params_.pitch;
        const float width = params_.stereoWidth;
        const float shape = params_.shape;
        const float decayRate = 0.7f + params_.decay * 11.0f;
        const float detune = 1.0f + width * 0.02f;
        const float rightPhaseOffset = width * (0.25f * kTwoPi);

        uint32_t rngLeft = variantSeed ^ 0xA5A5F00Du;
        uint32_t rngRight = variantSeed ^ 0x5A5A0FF1u;
        float phaseL = randSigned(&rngLeft) * kTwoPi;
        float phaseR = phaseL + rightPhaseOffset;
        float auxPhaseL = randSigned(&rngLeft) * kTwoPi;
        float auxPhaseR = auxPhaseL + rightPhaseOffset * 1.7f;
        float noiseL = 0.0f;
        float noiseR = 0.0f;

        for (uint32_t i = 0; i < size; ++i) {
            const float t = (size > 1u) ? static_cast<float>(i) / static_cast<float>(size - 1u) : 0.0f;
            const float env = std::exp(-t * decayRate);
            const float tail = 1.0f - std::pow(t, 2.0f + params_.decay * 4.0f);
            const float amp = env * std::max(0.0f, tail);

            float freqL = baseFreq;
            float freqR = baseFreq * detune;
            if (mode == KernelMode::Chirp) {
                const float sweep = 1.0f + t * (0.5f + shape * 6.0f);
                freqL *= sweep;
                freqR *= sweep * detune;
            }

            phaseL = wrapPhase(phaseL + kTwoPi * (freqL / static_cast<float>(sampleRate_)));
            phaseR = wrapPhase(phaseR + kTwoPi * (freqR / static_cast<float>(sampleRate_)));
            auxPhaseL = wrapPhase(auxPhaseL + kTwoPi * ((freqL * (0.35f + shape * 1.75f)) / static_cast<float>(sampleRate_)));
            auxPhaseR = wrapPhase(auxPhaseR + kTwoPi * ((freqR * (0.35f + shape * 1.75f)) / static_cast<float>(sampleRate_)));

            left[i] = renderModeSample(mode, phaseL, auxPhaseL, shape, t, &rngLeft, &noiseL) * amp;
            right[i] = renderModeSample(mode, phaseR, auxPhaseR, shape, t, &rngRight, &noiseR) * amp;
        }

        normalizeKernel(left, size);
        normalizeKernel(right, size);
    }

    void queueKernelBuild(bool crossfade) {
        const uint32_t size = static_cast<uint32_t>(clampi(static_cast<int>(std::lround(params_.kernelSize)), 32, static_cast<int>(kMaxKernelSize)));
        rngState_ = (rngState_ * 1664525u) + 1013904223u;

        if (!crossfade || activeKernelSize_ == 0) {
            renderKernel(activeKernelL_, activeKernelR_, size, rngState_);
            activeKernelSize_ = size;
            pendingKernelSize_ = 0;
            kernelBlendRemaining_ = 0;
            return;
        }

        renderKernel(pendingKernelL_, pendingKernelR_, size, rngState_);
        pendingKernelSize_ = size;
        kernelBlendRemaining_ = kKernelCrossfadeSamples;
    }

    void maybeRefreshKernel(bool transportRunning, bool tempoValid, double bpm) {
        if (params_.refresh <= 0.0001f) {
            return;
        }

        if (samplesUntilRefresh_ == std::numeric_limits<uint64_t>::max()) {
            samplesUntilRefresh_ = computeRefreshInterval(transportRunning, tempoValid, bpm);
        }

        if (samplesUntilRefresh_ > 0) {
            --samplesUntilRefresh_;
        }

        if (samplesUntilRefresh_ == 0) {
            queueKernelBuild(true);
            samplesUntilRefresh_ = computeRefreshInterval(transportRunning, tempoValid, bpm);
        }
    }

    uint64_t computeRefreshInterval(bool transportRunning, bool tempoValid, double bpm) const {
        const double refresh = std::max(0.0001, static_cast<double>(params_.refresh));

        double frames = 0.0;
        if (tempoValid && transportRunning && bpm > 0.0) {
            const double quarterNote = sampleRate_ * 60.0 / bpm;
            frames = quarterNote / refresh;
        } else {
            frames = sampleRate_ / refresh;
        }

        if (frames < 1.0) {
            frames = 1.0;
        }
        return static_cast<uint64_t>(std::llround(frames));
    }

    double sampleRate_ = 48000.0;
    Params params_{};

    std::array<float, kMaxKernelSize> historyL_{};
    std::array<float, kMaxKernelSize> historyR_{};
    std::array<float, kMaxKernelSize> activeKernelL_{};
    std::array<float, kMaxKernelSize> activeKernelR_{};
    std::array<float, kMaxKernelSize> pendingKernelL_{};
    std::array<float, kMaxKernelSize> pendingKernelR_{};

    uint32_t writeIndex_ = 0;
    uint32_t activeKernelSize_ = 0;
    uint32_t pendingKernelSize_ = 0;
    uint32_t kernelBlendRemaining_ = 0;
    uint64_t samplesUntilRefresh_ = std::numeric_limits<uint64_t>::max();
    uint32_t rngState_ = 0x1234ABCDu;
    float prevWetL_ = 0.0f;
    float prevWetR_ = 0.0f;
};

} // namespace flues::convulse
