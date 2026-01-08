#pragma once

#include "AlgorithmOutput.hpp"
#include "AlgorithmUtils.hpp"

namespace flues::disyn {

class Novel1MultistageAlgorithm {
public:
    explicit Novel1MultistageAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f), modPhase(0.0f) {}

    void reset() {
        phase = 0.0f;
        modPhase = 0.0f;
    }

    AlgorithmOutput process(float pitch, float param1, float param2, float param3) {
        const float tanhDrive = expoMap(param1, 0.1f, 10.0f);
        const float expDepth = expoMap(param2, 0.1f, 1.5f);
        const float ringCarrierMult = 0.5f + param3 * 4.5f;

        phase = stepPhase(phase, pitch, sampleRate);
        const float input = std::sin(TWO_PI * phase);

        const float stage1 = std::tanh(tanhDrive * input);
        const float stage2 = stage1 * std::exp(expDepth * stage1);

        modPhase = stepPhase(modPhase, pitch * ringCarrierMult, sampleRate);
        const float carrier = std::sin(TWO_PI * modPhase);
        const float stage3 = stage2 * (1.0f + carrier);

        const float output = stage3 * 0.25f;
        const float secondary = stage2 * 0.25f;
        return {output, secondary};
    }

private:
    float sampleRate;
    float phase;
    float modPhase;
};

} // namespace flues::disyn
