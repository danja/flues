#pragma once

#include "AlgorithmOutput.hpp"
#include "AlgorithmUtils.hpp"

namespace flues::disyn {

class DirichletPulseAlgorithm {
public:
    explicit DirichletPulseAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f) {}

    void reset() {
        phase = 0.0f;
    }

    AlgorithmOutput process(float pitch, float param1, float param2, float param3) {
        const int harmonics = std::max(1, static_cast<int>(std::round(1.0f + param1 * 63.0f)));
        const float tilt = -3.0f + param2 * 18.0f;
        const float shape = std::clamp(param3, 0.0f, 1.0f);

        phase = stepPhase(phase, pitch, sampleRate);
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
        const float base = (value / static_cast<float>(harmonics)) * tiltFactor;
        const float shaped = std::tanh(base * (1.0f + shape * 4.0f));
        const float output = base * (1.0f - shape) + shaped * shape;
        const float secondary = base;
        return {output, secondary};
    }

private:
    float sampleRate;
    float phase;
};

} // namespace flues::disyn
