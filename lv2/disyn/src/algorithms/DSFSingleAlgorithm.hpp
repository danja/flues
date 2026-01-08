#pragma once

#include "AlgorithmOutput.hpp"
#include "AlgorithmUtils.hpp"

namespace flues::disyn {

class DSFSingleAlgorithm {
public:
    explicit DSFSingleAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f), secondaryPhase(0.0f) {}

    void reset() {
        phase = 0.0f;
        secondaryPhase = 0.0f;
    }

    AlgorithmOutput process(float pitch, float param1, float param2, float /*param3*/) {
        const float decay = std::min(param1 * 0.98f, 0.98f);
        const float ratio = expoMap(param2, 0.5f, 4.0f);

        phase = stepPhase(phase, pitch, sampleRate);
        secondaryPhase = stepPhase(secondaryPhase, pitch * ratio, sampleRate);

        const float w = phase * TWO_PI;
        const float t = secondaryPhase * TWO_PI;

        const float output = computeDSFComponent(w, t, decay) * 0.5f;
        return {output, output};
    }

private:
    float sampleRate;
    float phase;
    float secondaryPhase;
};

} // namespace flues::disyn
