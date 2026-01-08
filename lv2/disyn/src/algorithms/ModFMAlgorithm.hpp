#pragma once

#include "AlgorithmOutput.hpp"
#include "AlgorithmUtils.hpp"

namespace flues::disyn {

class ModFMAlgorithm {
public:
    explicit ModFMAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f), modPhase(0.0f) {}

    void reset() {
        phase = 0.0f;
        modPhase = 0.0f;
    }

    AlgorithmOutput process(float pitch, float param1, float param2, float param3) {
        const float index = expoMap(param1, 0.01f, 8.0f);
        const float ratio = expoMap(param2, 0.25f, 6.0f);
        const float feedback = std::clamp(param3, 0.0f, 1.0f) * 0.8f;

        phase = stepPhase(phase, pitch, sampleRate);
        modPhase = stepPhase(modPhase, pitch * ratio, sampleRate);

        const float carrier = std::cos(phase * TWO_PI);
        const float modPhaseRad = modPhase * TWO_PI;
        const float modulator = std::cos(modPhaseRad + feedback * std::sin(modPhaseRad));
        const float envelope = std::exp(-index);

        const float output = carrier * std::exp(index * (modulator - 1.0f)) * envelope * 0.6f;
        const float secondary = carrier * modulator * envelope * 0.6f;
        return {output, secondary};
    }

private:
    float sampleRate;
    float phase;
    float modPhase;
};

} // namespace flues::disyn
