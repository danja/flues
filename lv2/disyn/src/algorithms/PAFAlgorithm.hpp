#pragma once

#include "AlgorithmOutput.hpp"
#include "AlgorithmUtils.hpp"

namespace flues::disyn {

class PAFAlgorithm {
public:
    explicit PAFAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f), secondaryPhase(0.0f), modPhase(0.0f) {}

    void reset() {
        phase = 0.0f;
        secondaryPhase = 0.0f;
        modPhase = 0.0f;
    }

    AlgorithmOutput process(float pitch, float param1, float param2, float param3) {
        const float ratio = expoMap(param1, 0.5f, 6.0f);
        const float bandwidth = expoMap(param2, 50.0f, 3000.0f);
        const float depth = 0.2f + std::clamp(param3, 0.0f, 1.0f) * 0.8f;

        phase = stepPhase(phase, pitch, sampleRate);
        secondaryPhase = stepPhase(secondaryPhase, pitch * ratio, sampleRate);

        const float carrier = std::sin(secondaryPhase * TWO_PI);
        const float mod = std::sin(phase * TWO_PI);
        const float decay = std::exp(-bandwidth / sampleRate);
        modPhase = decay * modPhase + (1.0f - decay) * mod;

        const float output = carrier * ((1.0f - depth) + depth * modPhase) * 0.5f;
        const float secondary = carrier * (0.5f + 0.5f * modPhase) * 0.5f;
        return {output, secondary};
    }

private:
    float sampleRate;
    float phase;
    float secondaryPhase;
    float modPhase;
};

} // namespace flues::disyn
