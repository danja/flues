#pragma once

#include "AlgorithmOutput.hpp"
#include "AlgorithmUtils.hpp"

namespace flues::disyn {

class Novel3CrossModAlgorithm {
public:
    explicit Novel3CrossModAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f), modPhase(0.0f), secondaryPhase(0.0f) {}

    void reset() {
        phase = 0.0f;
        modPhase = 0.0f;
        secondaryPhase = 0.0f;
    }

    AlgorithmOutput process(float pitch, float param1, float param2, float param3) {
        const float mod1Depth = param1;
        const float mod2Depth = param2;
        const float mix = std::clamp(param3, 0.0f, 1.0f);

        const float baseDsfDecay = 0.7f;
        const float baseDsfRatio = 1.5f;
        const float baseModfmIndex = 0.25f;

        const float dsfRatio = baseDsfRatio + mod2Depth * baseModfmIndex * 0.5f;
        const float modfmIndex = baseModfmIndex + mod1Depth * baseDsfDecay * 1.0f;

        phase = stepPhase(phase, pitch, sampleRate);
        const float theta = TWO_PI * dsfRatio;
        const float denom = 1.0f - 2.0f * baseDsfDecay * std::cos(theta) + baseDsfDecay * baseDsfDecay;
        const float dsf = (std::sin(TWO_PI * phase) - baseDsfDecay * std::sin(TWO_PI * phase - theta))
            / (denom + EPSILON);

        modPhase = stepPhase(modPhase, pitch, sampleRate);
        secondaryPhase = stepPhase(secondaryPhase, pitch, sampleRate);
        const float mod = std::cos(TWO_PI * secondaryPhase);
        const float modfm = std::cos(TWO_PI * modPhase) * std::exp(modfmIndex * (mod - 1.0f));

        const float output = (dsf * (1.0f - mix) + modfm * mix) * 0.7f;
        const float secondary = (dsf - modfm) * 0.7f;
        return {output, secondary};
    }

private:
    float sampleRate;
    float phase;
    float modPhase;
    float secondaryPhase;
};

} // namespace flues::disyn
