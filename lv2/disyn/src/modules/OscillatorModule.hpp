#pragma once

#include <algorithm>
#include <cmath>

namespace flues::disyn {

const float TWO_PI = 2.0f * M_PI;
const float EPSILON = 1e-8f;

enum class AlgorithmType : int {
    DIRICHLET_PULSE = 0,
    DSF_SINGLE = 1,
    DSF_DOUBLE = 2,
    TANH_SQUARE = 3,
    TANH_SAW = 4,
    PAF = 5,
    MOD_FM = 6
};

// Parameter structures for each algorithm
struct DirichletParams {
    float harmonics;  // 1-64
    float tilt;       // -3 to +15 dB/oct
};

struct DSFParams {
    float decay;  // 0-0.98 or 0-0.96
    float ratio;  // 0.5-4 or 0.5-4.5
};

struct TanhParams {
    float drive;  // 0.05-5 or 0.05-4.5
    float secondary;  // trim (0.2-1.2) or blend (0-1)
};

struct PAFParams {
    float formant;    // 0.5-6 (×f0)
    float bandwidth;  // 50-3000 Hz
};

struct ModFMParams {
    float index;  // 0.01-8
    float ratio;  // 0.25-6
};

class OscillatorModule {
public:
    explicit OscillatorModule(float sampleRate = 44100.0f)
        : sampleRate(sampleRate),
          phase(0.0f),
          modPhase(0.0f),
          secondaryPhase(0.0f),
          secondaryPhaseNeg(0.0f) {}

    void reset() {
        phase = 0.0f;
        modPhase = 0.0f;
        secondaryPhase = 0.0f;
        secondaryPhaseNeg = 0.0f;
    }

    // Main process function - dispatches to algorithm-specific methods
    float process(AlgorithmType algorithm, float param1, float param2, float frequency) {
        switch (algorithm) {
            case AlgorithmType::DIRICHLET_PULSE:
                return processDirichletPulse(param1, param2, frequency);
            case AlgorithmType::DSF_SINGLE:
                return processDSF(param1, param2, frequency);
            case AlgorithmType::DSF_DOUBLE:
                return processDSFDouble(param1, param2, frequency);
            case AlgorithmType::TANH_SQUARE:
                return processTanhSquare(param1, param2, frequency);
            case AlgorithmType::TANH_SAW:
                return processTanhSaw(param1, param2, frequency);
            case AlgorithmType::PAF:
                return processPAF(param1, param2, frequency);
            case AlgorithmType::MOD_FM:
                return processModFM(param1, param2, frequency);
            default:
                return processSine(frequency);
        }
    }

private:
    float sampleRate;
    float phase;
    float modPhase;
    float secondaryPhase;
    float secondaryPhaseNeg;

    // Helper: step phase accumulator forward by frequency
    float stepPhase(float currentPhase, float freq) {
        float next = currentPhase + freq / sampleRate;
        return next - std::floor(next);
    }

    // Fallback: simple sine wave
    float processSine(float frequency) {
        phase = stepPhase(phase, frequency);
        return std::sin(phase * TWO_PI);
    }

    // Algorithm 1: Dirichlet Pulse (Band-Limited Pulse)
    float processDirichletPulse(float param1, float param2, float frequency) {
        // Map parameters: param1=harmonics (1-64), param2=tilt (-3 to +15 dB/oct)
        const int harmonics = std::max(1, static_cast<int>(std::round(1.0f + param1 * 63.0f)));
        const float tilt = -3.0f + param2 * 18.0f;

        phase = stepPhase(phase, frequency);
        const float theta = phase * TWO_PI;

        const float numerator = std::sin((2.0f * harmonics + 1.0f) * theta * 0.5f);
        const float denominator = std::sin(theta * 0.5f);

        float value;
        if (std::abs(denominator) < EPSILON) {
            value = 1.0f;
        } else {
            value = (numerator / denominator) - 1.0f;
        }

        const float tiltFactor = std::pow(10.0f, tilt / 20.0f);
        return (value / static_cast<float>(harmonics)) * tiltFactor;
    }

    // Algorithm 2: Single-Sided DSF
    float processDSF(float param1, float param2, float frequency) {
        // Map parameters: param1=decay (0-0.98), param2=ratio (0.5-4)
        const float decay = std::min(param1 * 0.98f, 0.98f);
        const float ratio = expoMap(param2, 0.5f, 4.0f);

        phase = stepPhase(phase, frequency);
        secondaryPhase = stepPhase(secondaryPhase, frequency * ratio);

        const float w = phase * TWO_PI;
        const float t = secondaryPhase * TWO_PI;

        return computeDSFComponent(w, t, decay);
    }

    // Algorithm 3: Double-Sided DSF
    float processDSFDouble(float param1, float param2, float frequency) {
        // Map parameters: param1=decay (0-0.96), param2=ratio (0.5-4.5)
        const float decay = std::min(param1 * 0.96f, 0.96f);
        const float ratio = expoMap(param2, 0.5f, 4.5f);

        phase = stepPhase(phase, frequency);
        secondaryPhase = stepPhase(secondaryPhase, frequency * ratio);
        secondaryPhaseNeg = stepPhase(secondaryPhaseNeg, frequency * ratio);

        const float w = phase * TWO_PI;
        const float tPos = secondaryPhase * TWO_PI;
        const float tNeg = -secondaryPhaseNeg * TWO_PI;

        const float positive = computeDSFComponent(w, tPos, decay);
        const float negative = computeDSFComponent(w, tNeg, decay);

        return 0.5f * (positive + negative);
    }

    // Helper: DSF computation (Moorer discrete summation formula)
    float computeDSFComponent(float w, float t, float decay) {
        const float denominator = 1.0f - 2.0f * decay * std::cos(t) + decay * decay;
        if (std::abs(denominator) < EPSILON) {
            return 0.0f;
        }

        const float numerator = std::sin(w) - decay * std::sin(w - t);
        const float normalise = std::sqrt(1.0f - decay * decay);
        return (numerator / denominator) * normalise;
    }

    // Algorithm 4: Tanh Square (Hyperbolic Tangent Waveshaping)
    float processTanhSquare(float param1, float param2, float frequency) {
        // Map parameters: param1=drive (0.05-5), param2=trim (0.2-1.2)
        const float drive = expoMap(param1, 0.05f, 5.0f);
        const float trim = expoMap(param2, 0.2f, 1.2f);

        phase = stepPhase(phase, frequency);
        const float carrier = std::sin(phase * TWO_PI);
        return std::tanh(carrier * drive) * trim;
    }

    // Algorithm 5: Tanh Saw (Square-to-Saw Transformation)
    float processTanhSaw(float param1, float param2, float frequency) {
        // Map parameters: param1=drive (0.05-4.5), param2=blend (0-1)
        const float drive = expoMap(param1, 0.05f, 4.5f);
        const float blend = std::clamp(param2, 0.0f, 1.0f);

        phase = stepPhase(phase, frequency);
        const float sine = std::sin(phase * TWO_PI);
        const float square = std::tanh(sine * drive);

        secondaryPhase = stepPhase(secondaryPhase, frequency);
        const float cosine = std::cos(secondaryPhase * TWO_PI);
        const float saw = square + cosine * (1.0f - square * square);

        return square * (1.0f - blend) + saw * blend;
    }

    // Algorithm 6: Phase-Aligned Formant (PAF)
    float processPAF(float param1, float param2, float frequency) {
        // Map parameters: param1=formant (0.5-6 ×f0), param2=bandwidth (50-3000 Hz)
        const float ratio = expoMap(param1, 0.5f, 6.0f);
        const float bandwidth = expoMap(param2, 50.0f, 3000.0f);

        phase = stepPhase(phase, frequency);
        secondaryPhase = stepPhase(secondaryPhase, frequency * ratio);

        const float carrier = std::sin(secondaryPhase * TWO_PI);
        const float mod = std::sin(phase * TWO_PI);
        const float decay = std::exp(-bandwidth / sampleRate);
        modPhase = decay * modPhase + (1.0f - decay) * mod;

        return carrier * (0.6f + 0.4f * modPhase);
    }

    // Algorithm 7: Modified FM
    float processModFM(float param1, float param2, float frequency) {
        // Map parameters: param1=index (0.01-8), param2=ratio (0.25-6)
        const float index = expoMap(param1, 0.01f, 8.0f);
        const float ratio = expoMap(param2, 0.25f, 6.0f);

        phase = stepPhase(phase, frequency);
        modPhase = stepPhase(modPhase, frequency * ratio);

        const float carrier = std::cos(phase * TWO_PI);
        const float modulator = std::cos(modPhase * TWO_PI);
        const float envelope = std::exp(-index);

        return carrier * std::exp(index * (modulator - 1.0f)) * envelope;
    }

    // Helper: exponential mapping from normalized 0-1 to min-max range
    float expoMap(float value, float min, float max) {
        const float clamped = std::clamp(value, 0.0f, 1.0f);
        return min * std::pow(max / min, clamped);
    }
};

} // namespace flues::disyn
