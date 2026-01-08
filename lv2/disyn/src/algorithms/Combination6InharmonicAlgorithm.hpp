#pragma once

#include "AlgorithmOutput.hpp"
#include "AlgorithmUtils.hpp"

namespace flues::disyn {

class Combination6InharmonicAlgorithm {
public:
    explicit Combination6InharmonicAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f), formant1Phase(0.0f) {}

    void reset() {
        phase = 0.0f;
        formant1Phase = 0.0f;
    }

    AlgorithmOutput process(float pitch, float param1, float param2, float param3) {
        const float phiRatio = 1.618034f;
        const float pafShift = expoMap(param2, 5.0f, 50.0f);
        const float dsfDecay = 0.5f + param1 * 0.4f;
        const float mix = std::clamp(param3, 0.0f, 1.0f);

        phase = stepPhase(phase, pitch, sampleRate);
        const float theta = TWO_PI * phiRatio;
        const float denom = 1.0f - 2.0f * dsfDecay * std::cos(theta) + dsfDecay * dsfDecay;
        const float dsf = (std::sin(TWO_PI * phase) - dsfDecay * std::sin(TWO_PI * phase - theta))
            / (denom + EPSILON);

        const float formantFreq = pitch * 2.0f + pafShift;
        formant1Phase = stepPhase(formant1Phase, formantFreq, sampleRate);
        const float paf = std::sin(TWO_PI * formant1Phase) * 0.5f;

        const float output = dsf * (1.0f - mix) + paf * mix;
        const float secondary = dsf;
        return {output, secondary};
    }

private:
    float sampleRate;
    float phase;
    float formant1Phase;
};

} // namespace flues::disyn
