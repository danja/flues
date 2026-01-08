#pragma once

#include "AlgorithmOutput.hpp"
#include "AlgorithmUtils.hpp"

namespace flues::disyn {

class Combination3ParallelBankAlgorithm {
public:
    explicit Combination3ParallelBankAlgorithm(float sampleRate)
        : sampleRate(sampleRate),
          parallel1Phase(0.0f),
          parallel2Phase(0.0f),
          parallel3Phase(0.0f),
          parallel4Phase(0.0f),
          parallel5Phase(0.0f),
          formant1Phase(0.0f),
          formant2Phase(0.0f),
          formant3Phase(0.0f) {}

    void reset() {
        parallel1Phase = 0.0f;
        parallel2Phase = 0.0f;
        parallel3Phase = 0.0f;
        parallel4Phase = 0.0f;
        parallel5Phase = 0.0f;
        formant1Phase = 0.0f;
        formant2Phase = 0.0f;
        formant3Phase = 0.0f;
    }

    AlgorithmOutput process(float pitch, float param1, float param2, float param3) {
        (void)param2;
        const float modfmIndex = expoMap(param1, 0.01f, 8.0f);
        const float mixBalance = param3;

        parallel1Phase = stepPhase(parallel1Phase, pitch, sampleRate);
        parallel2Phase = stepPhase(parallel2Phase, pitch * 1.0f, sampleRate);
        const float mod1 = std::cos(TWO_PI * parallel2Phase);
        const float modfm1 = std::cos(TWO_PI * parallel1Phase) * std::exp(modfmIndex * (mod1 - 1.0f));

        parallel3Phase = stepPhase(parallel3Phase, pitch, sampleRate);
        parallel4Phase = stepPhase(parallel4Phase, pitch * 1.5f, sampleRate);
        const float mod2 = std::cos(TWO_PI * parallel4Phase);
        const float modfm2 = std::cos(TWO_PI * parallel3Phase) * std::exp(modfmIndex * (mod2 - 1.0f));

        parallel5Phase = stepPhase(parallel5Phase, pitch, sampleRate);
        formant1Phase = stepPhase(formant1Phase, pitch * 1.333f, sampleRate);
        const float mod3 = std::cos(TWO_PI * formant1Phase);
        const float modfm3 = std::cos(TWO_PI * parallel5Phase) * std::exp(modfmIndex * (mod3 - 1.0f));

        formant2Phase = stepPhase(formant2Phase, 800.0f, sampleRate);
        formant3Phase = stepPhase(formant3Phase, 2400.0f, sampleRate);
        const float paf1 = std::sin(TWO_PI * formant2Phase) * 0.5f;
        const float paf2 = std::sin(TWO_PI * formant3Phase) * 0.5f;

        const float modfmMix = (modfm1 + modfm2 + modfm3) / 3.0f;
        const float pafMix = (paf1 + paf2) / 2.0f;
        const float output = (modfmMix * (1.0f - mixBalance) + pafMix * mixBalance) * 0.5f;
        const float secondary = (pafMix - modfmMix) * 0.5f;
        return {output, secondary};
    }

private:
    float sampleRate;
    float parallel1Phase;
    float parallel2Phase;
    float parallel3Phase;
    float parallel4Phase;
    float parallel5Phase;
    float formant1Phase;
    float formant2Phase;
    float formant3Phase;
};

} // namespace flues::disyn
