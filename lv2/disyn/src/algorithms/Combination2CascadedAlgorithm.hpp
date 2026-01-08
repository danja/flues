#pragma once

#include "AlgorithmOutput.hpp"
#include "AlgorithmUtils.hpp"

namespace flues::disyn {

class Combination2CascadedAlgorithm {
public:
    explicit Combination2CascadedAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f), cascade1Phase(0.0f), cascade2Phase(0.0f) {}

    void reset() {
        phase = 0.0f;
        cascade1Phase = 0.0f;
        cascade2Phase = 0.0f;
    }

    AlgorithmOutput process(float pitch, float param1, float param2, float param3) {
        const float dsfDecay = 0.5f + param1 * 0.45f;
        const float asymRatio = param2;
        const float tanhDrive = param3 * 5.0f;

        phase = stepPhase(phase, pitch, sampleRate);
        const float theta = TWO_PI * 1.5f;
        const float denom = 1.0f - 2.0f * dsfDecay * std::cos(theta) + dsfDecay * dsfDecay;
        const float stage1 = (std::sin(TWO_PI * phase) - dsfDecay * std::sin(TWO_PI * phase - theta))
            / (denom + EPSILON);

        const float stage2 = processAsymmetricFM(std::abs(stage1), asymRatio, pitch, sampleRate,
                                                 cascade1Phase, cascade2Phase);

        const float stage3 = std::tanh(stage2 * tanhDrive);
        const float output = stage3 * 0.6f;
        const float secondary = stage2 * 0.6f;
        return {output, secondary};
    }

private:
    float sampleRate;
    float phase;
    float cascade1Phase;
    float cascade2Phase;
};

} // namespace flues::disyn
