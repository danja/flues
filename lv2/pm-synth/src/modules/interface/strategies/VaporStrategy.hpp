#pragma once

#include "../InterfaceStrategy.hpp"
#include "../utils/ExcitationGen.hpp"
#include "../utils/NonlinearityLib.hpp"
#include <algorithm>

namespace flues::pm {

class VaporStrategy : public InterfaceStrategy {
public:
    explicit VaporStrategy(float sampleRate)
        : InterfaceStrategy(sampleRate),
          chaos1(3.7f),
          chaos2(3.8f),
          chaos3(3.9f),
          prev1(0.0f),
          prev2(0.0f) {}

    float process(float input) override {
        const float r = 2.5f + intensity * 1.5f;
        chaos1.setR(r);
        chaos2.setR(r + 0.1f);
        chaos3.setR(r + 0.2f);

        const float c1 = chaos1.process(0.3f);
        const float c2 = chaos2.process(0.3f);
        const float c3 = chaos3.process(0.3f);

        const float chaosAmount = intensity * 0.6f;
        const float inputAmount = 1.0f - chaosAmount * 0.5f;
        const float mixed = input * inputAmount + (c1 + c2 + c3) * chaosAmount;
        const float feedback = (prev1 * 0.3f + prev2 * 0.2f) * chaosAmount;
        const float turbulent = mixed + feedback;
        const float output = softClip(turbulent, 1.2f);

        prev2 = prev1;
        prev1 = output;

        return std::clamp(output, -1.0f, 1.0f);
    }

    void reset() override {
        chaos1.reset();
        chaos2.reset();
        chaos3.reset();
        prev1 = 0.0f;
        prev2 = 0.0f;
    }

    void onNoteOn() override {
        reset();
    }

    const char* getName() const override {
        return "VaporStrategy";
    }

private:
    ChaoticOscillator chaos1;
    ChaoticOscillator chaos2;
    ChaoticOscillator chaos3;
    float prev1;
    float prev2;
};

} // namespace flues::pm
