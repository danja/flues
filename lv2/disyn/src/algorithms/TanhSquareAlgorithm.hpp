#pragma once

#include "AlgorithmOutput.hpp"
#include "AlgorithmUtils.hpp"

namespace flues::disyn {

class TanhSquareAlgorithm {
public:
    explicit TanhSquareAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f) {}

    void reset() {
        phase = 0.0f;
    }

    AlgorithmOutput process(float pitch, float param1, float param2, float param3) {
        const float drive = expoMap(param1, 0.05f, 5.0f);
        const float trim = expoMap(param2, 0.2f, 1.2f);
        const float bias = (std::clamp(param3, 0.0f, 1.0f) - 0.5f) * 0.8f;

        phase = stepPhase(phase, pitch, sampleRate);
        const float carrier = std::sin(phase * TWO_PI) + bias;
        const float output = std::tanh(carrier * drive) * trim;
        const float secondary = std::tanh(std::sin(phase * TWO_PI) * drive) * trim;
        return {output, secondary};
    }

private:
    float sampleRate;
    float phase;
};

} // namespace flues::disyn
