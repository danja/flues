#pragma once

#include "../InterfaceStrategy.hpp"
#include "../utils/ExcitationGen.hpp"
#include <algorithm>

namespace flues::pm {

class FluteStrategy : public InterfaceStrategy {
public:
    explicit FluteStrategy(float sampleRate)
        : InterfaceStrategy(sampleRate) {}

    float process(float input) override {
        const float softness = 0.45f + intensity * 0.4f;
        const float gateFactor = gate ? 1.0f : 0.0f;
        const float breath = whiteNoise(intensity * 0.04f * gateFactor, &rng);
        const float mixed = (input + breath) * softness;
        const float shaped = mixed - (mixed * mixed * mixed) * 0.35f;
        return std::clamp(shaped, -0.49f, 0.49f);
    }

    void reset() override {}

    const char* getName() const override {
        return "FluteStrategy";
    }

private:
    Random rng;
};

} // namespace flues::pm
