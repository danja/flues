#pragma once

#include "AlgorithmOutput.hpp"
#include "AlgorithmUtils.hpp"

namespace flues::disyn {

class Combination5MorphingAlgorithm {
public:
    explicit Combination5MorphingAlgorithm(float sampleRate)
        : sampleRate(sampleRate),
          phase(0.0f),
          modPhase(0.0f),
          secondaryPhase(0.0f),
          formant1Phase(0.0f) {}

    void reset() {
        phase = 0.0f;
        modPhase = 0.0f;
        secondaryPhase = 0.0f;
        formant1Phase = 0.0f;
    }

    AlgorithmOutput process(float pitch, float param1, float param2, float param3) {
        const float morphCurve = 0.5f + std::clamp(param3, 0.0f, 1.0f) * 1.5f;
        const float morphPos = std::pow(std::clamp(param1, 0.0f, 1.0f), morphCurve);
        const float character = param2;

        float output = 0.0f;

        float secondary = 0.0f;

        if (morphPos < 0.5f) {
            const float alpha = morphPos * 2.0f;

            phase = stepPhase(phase, pitch, sampleRate);
            const float dsfDecay = 0.5f + character * 0.4f;
            const float theta = TWO_PI * 1.5f;
            const float denom = 1.0f - 2.0f * dsfDecay * std::cos(theta) + dsfDecay * dsfDecay;
            const float dsf = (std::sin(TWO_PI * phase) - dsfDecay * std::sin(TWO_PI * phase - theta))
                / (denom + EPSILON);

            modPhase = stepPhase(modPhase, pitch, sampleRate);
            secondaryPhase = stepPhase(secondaryPhase, pitch, sampleRate);
            const float modfmIndex = expoMap(character, 0.01f, 8.0f);
            const float mod = std::cos(TWO_PI * secondaryPhase);
            const float modfm = std::cos(TWO_PI * modPhase) * std::exp(modfmIndex * (mod - 1.0f));

            output = dsf * (1.0f - alpha) + modfm * alpha;
            secondary = modfm;
        } else {
            const float alpha = (morphPos - 0.5f) * 2.0f;

            modPhase = stepPhase(modPhase, pitch, sampleRate);
            secondaryPhase = stepPhase(secondaryPhase, pitch, sampleRate);
            const float modfmIndex = expoMap(character, 0.01f, 8.0f);
            const float mod = std::cos(TWO_PI * secondaryPhase);
            const float modfm = std::cos(TWO_PI * modPhase) * std::exp(modfmIndex * (mod - 1.0f));

            formant1Phase = stepPhase(formant1Phase, pitch * 2.0f, sampleRate);
            const float paf = std::sin(TWO_PI * formant1Phase) * 0.5f;

            output = modfm * (1.0f - alpha) + paf * alpha;
            secondary = paf;
        }

        const float scaled = output * 0.6f;
        return {scaled, secondary * 0.6f};
    }

private:
    float sampleRate;
    float phase;
    float modPhase;
    float secondaryPhase;
    float formant1Phase;
};

} // namespace flues::disyn
