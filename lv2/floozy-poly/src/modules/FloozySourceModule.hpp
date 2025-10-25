#pragma once

#include <algorithm>
#include <cmath>

#include "../../../pm-synth/src/Random.hpp"
#include "../../../disyn/src/modules/OscillatorModule.hpp"

namespace flues::floozy_poly {

class FloozySourceModule {
public:
    explicit FloozySourceModule(float sampleRate = 44100.0f)
        : sampleRate(sampleRate),
          oscillator(sampleRate),
          algorithm(flues::disyn::AlgorithmType::TANH_SQUARE),
          param1(0.55f),
          param2(0.5f),
          toneLevel(0.7f),
          noiseLevel(0.1f),
          dcLevel(0.5f) {}

    void reset() {
        oscillator.reset();
    }

    void setAlgorithm(float value) {
        int index = static_cast<int>(std::round(std::clamp(value, 0.0f, 6.0f)));
        algorithm = static_cast<flues::disyn::AlgorithmType>(index);
    }

    void setParam1(float value) {
        param1 = std::clamp(value, 0.0f, 1.0f);
    }

    void setParam2(float value) {
        param2 = std::clamp(value, 0.0f, 1.0f);
    }

    void setToneLevel(float value) {
        toneLevel = std::clamp(value, 0.0f, 1.0f);
    }

    void setNoiseLevel(float value) {
        noiseLevel = std::clamp(value, 0.0f, 1.0f);
    }

    void setDCLevel(float value) {
        dcLevel = std::clamp(value, 0.0f, 1.0f);
    }

    float process(float frequency) {
        const float osc = oscillator.process(algorithm, param1, param2, frequency) * toneLevel;
        const float noise = rng.uniformSignedFloat() * noiseLevel;
        const float dc = dcLevel;
        return osc + noise + dc;
    }

private:
    float sampleRate;
    flues::disyn::OscillatorModule oscillator;
    flues::disyn::AlgorithmType algorithm;
    float param1;
    float param2;
    float toneLevel;
    float noiseLevel;
    float dcLevel;
    flues::pm::Random rng;
};

} // namespace flues::floozy_poly
