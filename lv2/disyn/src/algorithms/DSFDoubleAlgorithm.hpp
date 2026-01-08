#pragma once

#include "AlgorithmOutput.hpp"
#include "AlgorithmUtils.hpp"

namespace flues::disyn {

class DSFDoubleAlgorithm {
public:
    explicit DSFDoubleAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f), secondaryPhase(0.0f), secondaryPhaseNeg(0.0f) {}

    void reset() {
        phase = 0.0f;
        secondaryPhase = 0.0f;
        secondaryPhaseNeg = 0.0f;
    }

    AlgorithmOutput process(float pitch, float param1, float param2, float param3) {
        const float decay = std::min(param1 * 0.96f, 0.96f);
        const float ratio = expoMap(param2, 0.5f, 4.5f);
        const float balance = std::clamp(param3, 0.0f, 1.0f) * 2.0f - 1.0f;
        const float weightPos = 0.5f + balance * 0.5f;
        const float weightNeg = 1.0f - weightPos;

        phase = stepPhase(phase, pitch, sampleRate);
        secondaryPhase = stepPhase(secondaryPhase, pitch * ratio, sampleRate);
        secondaryPhaseNeg = stepPhase(secondaryPhaseNeg, pitch * ratio, sampleRate);

        const float w = phase * TWO_PI;
        const float tPos = secondaryPhase * TWO_PI;
        const float tNeg = -secondaryPhaseNeg * TWO_PI;

        const float positive = computeDSFComponent(w, tPos, decay);
        const float negative = computeDSFComponent(w, tNeg, decay);

        const float output = 0.5f * (positive * weightPos + negative * weightNeg);
        const float secondary = 0.5f * (positive - negative);
        return {output, secondary};
    }

private:
    float sampleRate;
    float phase;
    float secondaryPhase;
    float secondaryPhaseNeg;
};

} // namespace flues::disyn
