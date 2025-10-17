#pragma once

#include "../InterfaceStrategy.hpp"
#include "../utils/ExcitationGen.hpp"
#include "../utils/NonlinearityLib.hpp"
#include <algorithm>

namespace flues::pm {

class DrumStrategy : public InterfaceStrategy {
public:
    explicit DrumStrategy(float sampleRate)
        : InterfaceStrategy(sampleRate),
          drumEnergy(0.0f) {}

    float process(float input) override {
        const float drive = 1.2f + intensity * 2.2f;
        const float noise = whiteNoise(0.02f + intensity * 0.06f, &rng);

        drumEnergy = drumEnergy * (0.7f - intensity * 0.2f) +
                     std::abs(input) * (0.6f + intensity * 0.7f);

        const float hit = std::tanh(input * drive) + noise;
        const float output = hit * (0.4f + intensity * 0.4f) +
                             (hit >= 0.0f ? 1.0f : -1.0f) * std::min(0.8f, drumEnergy * 0.6f);

        return std::clamp(output, -1.0f, 1.0f);
    }

    void reset() override {
        drumEnergy = 0.0f;
    }

    void onNoteOn() override {
        reset();
    }

    const char* getName() const override {
        return "DrumStrategy";
    }

private:
    float drumEnergy;
    Random rng;
};

} // namespace flues::pm
