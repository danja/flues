#pragma once

#include <algorithm>

#include "../Random.hpp"

namespace flues::pm {

class SourcesModule {
public:
    explicit SourcesModule(float sampleRate = 44100.0f)
        : sampleRate(sampleRate),
          dcLevel(0.5f),
          noiseLevel(0.15f),
          toneLevel(0.0f),
          sawtoothPhase(0.0f),
          sawtoothFrequency(440.0f) {}

    void setDCLevel(float value) {
        dcLevel = std::clamp(value, 0.0f, 1.0f);
    }

    void setNoiseLevel(float value) {
        noiseLevel = std::clamp(value, 0.0f, 1.0f);
    }

    void setToneLevel(float value) {
        toneLevel = std::clamp(value, 0.0f, 1.0f);
    }

    float process(float cv) {
        sawtoothFrequency = cv;

        const float dc = dcLevel;
        const float noise = rng.uniformSignedFloat() * noiseLevel;

        const float phaseIncrement = sawtoothFrequency / sampleRate;
        sawtoothPhase += phaseIncrement;
        if (sawtoothPhase >= 1.0f) {
            sawtoothPhase -= 1.0f;
        }
        const float saw = (sawtoothPhase * 2.0f - 1.0f) * toneLevel;

        return dc + noise + saw;
    }

    void reset() {
        sawtoothPhase = 0.0f;
    }

private:
    float sampleRate;
    float dcLevel;
    float noiseLevel;
    float toneLevel;
    float sawtoothPhase;
    float sawtoothFrequency;
    Random rng;
};

} // namespace flues::pm
