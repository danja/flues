#pragma once

#include "../InterfaceStrategy.hpp"
#include "../utils/NonlinearityLib.hpp"
#include <algorithm>

namespace flues::pm {

class CrystalStrategy : public InterfaceStrategy {
public:
    explicit CrystalStrategy(float sampleRate)
        : InterfaceStrategy(sampleRate),
          phase1(0.0f),
          phase2(0.0f),
          phase3(0.0f) {}

    float process(float input) override {
        phase1 = phase1 * 0.98f + input;
        phase2 = phase2 * 0.95f + input * kPhi;
        phase3 = phase3 * 0.92f + input * kPhi2;

        const float p1 = input * (1.0f + phase1 * 0.3f);
        const float p2 = input * (1.0f + phase2 * 0.3f);
        const float p3 = input * (1.0f + phase3 * 0.3f);

        const float crossCoupling = intensity * 0.3f;
        const float coupled = (p1 + p2 + p3) * (1.0f / 3.0f) +
                              crossCoupling * (p1 * p2 + p2 * p3 + p1 * p3) * 0.1f;

        const float output = cubicWaveshaper(coupled, intensity * 0.2f);
        return std::clamp(output, -1.0f, 1.0f);
    }

    void reset() override {
        phase1 = 0.0f;
        phase2 = 0.0f;
        phase3 = 0.0f;
    }

    void onNoteOn() override {
        reset();
    }

    const char* getName() const override {
        return "CrystalStrategy";
    }

private:
    static constexpr float kPhi = 1.618033988749895f;
    static constexpr float kPhi2 = kPhi * kPhi;
    float phase1;
    float phase2;
    float phase3;
};

} // namespace flues::pm
