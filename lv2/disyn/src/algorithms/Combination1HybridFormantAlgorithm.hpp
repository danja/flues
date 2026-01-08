#pragma once

#include "AlgorithmOutput.hpp"
#include "AlgorithmUtils.hpp"

namespace flues::disyn {

class Combination1HybridFormantAlgorithm {
public:
    explicit Combination1HybridFormantAlgorithm(float sampleRate)
        : sampleRate(sampleRate),
          phase(0.0f),
          modPhase(0.0f),
          formant1Phase(0.0f),
          formant2Phase(0.0f),
          formant3Phase(0.0f) {}

    void reset() {
        phase = 0.0f;
        modPhase = 0.0f;
        formant1Phase = 0.0f;
        formant2Phase = 0.0f;
        formant3Phase = 0.0f;
    }

    AlgorithmOutput process(float pitch, float param1, float param2, float param3) {
        (void)param2;
        const float modfmIndex = expoMap(param1, 0.01f, 3.0f);
        const float formantSpacing = 0.8f + param3 * 0.4f;

        phase = stepPhase(phase, pitch, sampleRate);
        modPhase = stepPhase(modPhase, pitch, sampleRate);
        const float modulator = std::sin(TWO_PI * modPhase);
        const float carrier = std::sin(TWO_PI * phase);
        const float base = carrier * std::exp(-modfmIndex * (std::abs(modulator) - 1.0f)) * 0.4f;

        formant1Phase = stepPhase(formant1Phase, 800.0f * formantSpacing, sampleRate);
        formant2Phase = stepPhase(formant2Phase, 1200.0f * formantSpacing, sampleRate);
        formant3Phase = stepPhase(formant3Phase, 2400.0f * formantSpacing, sampleRate);

        const float formant1 = std::sin(TWO_PI * formant1Phase) * 0.5f;
        const float formant2 = std::sin(TWO_PI * formant2Phase) * 0.5f;
        const float formant3 = std::sin(TWO_PI * formant3Phase) * 0.5f;

        const float output = (base + formant1 + formant2 + formant3) * 0.25f;
        const float secondary = base * 0.5f;
        return {output, secondary};
    }

private:
    float sampleRate;
    float phase;
    float modPhase;
    float formant1Phase;
    float formant2Phase;
    float formant3Phase;
};

} // namespace flues::disyn
