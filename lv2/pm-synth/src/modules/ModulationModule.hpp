#pragma once

#include <algorithm>
#include <cmath>

namespace flues::pm {

struct ModulationState {
    float lfo;
    float am;
    float fm;
};

class ModulationModule {
public:
    explicit ModulationModule(float sampleRate = 44100.0f)
        : sampleRate(sampleRate),
          lfoFrequency(5.0f),
          lfoPhase(0.0f),
          typeLevel(0.5f),
          amDepth(0.0f),
          fmDepth(0.0f) {}

    void setFrequency(float value) {
        const float clamped = std::clamp(value, 0.0f, 1.0f);
        lfoFrequency = 0.1f * std::pow(200.0f, clamped);
    }

    void setTypeLevel(float value) {
        typeLevel = std::clamp(value, 0.0f, 1.0f);
        if (typeLevel < 0.5f) {
            amDepth = (0.5f - typeLevel) * 2.0f;
            fmDepth = 0.0f;
        } else {
            amDepth = 0.0f;
            fmDepth = (typeLevel - 0.5f) * 2.0f;
        }
    }

    ModulationState process() {
        const float phaseIncrement = (lfoFrequency * 2.0f * static_cast<float>(M_PI)) / sampleRate;
        lfoPhase += phaseIncrement;
        if (lfoPhase > 2.0f * static_cast<float>(M_PI)) {
            lfoPhase -= 2.0f * static_cast<float>(M_PI);
        }

        const float lfo = std::sin(lfoPhase);
        const float am = 1.0f - amDepth * 0.5f + (lfo * amDepth * 0.5f);
        const float fm = 1.0f + (lfo * fmDepth * 0.1f);

        return {lfo, am, fm};
    }

    void reset() {
        lfoPhase = 0.0f;
    }

private:
    float sampleRate;
    float lfoFrequency;
    float lfoPhase;
    float typeLevel;
    float amDepth;
    float fmDepth;
};

} // namespace flues::pm
