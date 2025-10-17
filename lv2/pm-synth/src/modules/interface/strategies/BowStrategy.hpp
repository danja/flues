#pragma once

#include "../InterfaceStrategy.hpp"
#include "../utils/NonlinearityLib.hpp"
#include "../utils/ExcitationGen.hpp"
#include <algorithm>

namespace flues::pm {

class BowStrategy : public InterfaceStrategy {
public:
    explicit BowStrategy(float sampleRate)
        : InterfaceStrategy(sampleRate),
          bowState(0.0f) {}

    float process(float input) override {
        const float bowVelocity = intensity * 0.9f + 0.2f;
        const float slip = input - bowState;
        const float friction = fastTanh(slip * (6.0f + intensity * 12.0f));
        const float grit = whiteNoise(intensity * 0.012f, &rng);
        const float output = friction * (0.55f + intensity * 0.35f) + slip * 0.25f + grit;
        const float stick = 0.8f - intensity * 0.25f;
        bowState = bowState * stick + (input + friction * bowVelocity * 0.05f) * (1.0f - stick);
        return std::clamp(output, -1.0f, 1.0f);
    }

    void reset() override {
        bowState = 0.0f;
    }

    void onNoteOn() override {
        reset();
    }

    const char* getName() const override {
        return "BowStrategy";
    }

private:
    float bowState;
    Random rng;
};

} // namespace flues::pm
