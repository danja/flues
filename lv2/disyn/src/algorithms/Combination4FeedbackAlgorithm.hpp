#pragma once

#include "AlgorithmOutput.hpp"
#include "AlgorithmUtils.hpp"

namespace flues::disyn {

class Combination4FeedbackAlgorithm {
public:
    explicit Combination4FeedbackAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f), modPhase(0.0f), feedbackSample(0.0f) {}

    void reset() {
        phase = 0.0f;
        modPhase = 0.0f;
        feedbackSample = 0.0f;
    }

    AlgorithmOutput process(float pitch, float param1, float param2, float param3) {
        const float modfmIndex = expoMap(param1, 0.01f, 8.0f);
        const float feedbackGain = param2 * 0.95f;
        const float drive = 1.0f + std::clamp(param3, 0.0f, 1.0f) * 4.0f;

        const float modifiedFreq = pitch + feedbackSample * feedbackGain * pitch;

        phase = stepPhase(phase, modifiedFreq, sampleRate);
        modPhase = stepPhase(modPhase, modifiedFreq, sampleRate);
        const float modulator = std::cos(TWO_PI * modPhase);
        const float carrier = std::cos(TWO_PI * phase);
        const float output = carrier * std::exp(modfmIndex * (modulator - 1.0f));

        feedbackSample = output;

        const float shaped = std::tanh(output * drive);
        const float scaled = shaped * 0.5f;
        const float secondary = output * 0.5f;
        return {scaled, secondary};
    }

private:
    float sampleRate;
    float phase;
    float modPhase;
    float feedbackSample;
};

} // namespace flues::disyn
