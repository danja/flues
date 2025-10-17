#pragma once

#include "../InterfaceStrategy.hpp"
#include "../utils/EnergyTracker.hpp"
#include "../utils/NonlinearityLib.hpp"
#include <algorithm>

namespace flues::pm {

class PlasmaStrategy : public InterfaceStrategy {
public:
    explicit PlasmaStrategy(float sampleRate)
        : InterfaceStrategy(sampleRate),
          ampTracker(0.001f, sampleRate),
          phase(0.0f),
          x1(0.0f),
          y1(0.0f) {}

    float process(float input) override {
        const float amplitude = ampTracker.process(input);
        const float beta = intensity * 0.3f;
        const float phaseMod = 1.0f + beta * amplitude;

        phase += 0.1f * phaseMod;
        if (phase > 2.0f * static_cast<float>(M_PI)) {
            phase -= 2.0f * static_cast<float>(M_PI);
        }

        const float freqMod = std::sin(phase) * amplitude * intensity * 0.5f;
        const float allpassCoeff = 0.3f + amplitude * intensity * 0.4f;
        const float dispersed = allpassCoeff * input + x1 - allpassCoeff * y1;

        x1 = input;
        y1 = dispersed;

        float output = dispersed + freqMod;
        if (intensity > 0.5f) {
            output = cubicWaveshaper(output, (intensity - 0.5f) * 0.4f);
        }

        return std::clamp(output, -1.0f, 1.0f);
    }

    void reset() override {
        ampTracker.reset();
        phase = 0.0f;
        x1 = 0.0f;
        y1 = 0.0f;
    }

    void onNoteOn() override {
        reset();
    }

    const char* getName() const override {
        return "PlasmaStrategy";
    }

private:
    AmplitudeTracker ampTracker;
    float phase;
    float x1;
    float y1;
};

} // namespace flues::pm
