#pragma once

#include "../InterfaceStrategy.hpp"
#include "../utils/NonlinearityLib.hpp"
#include <algorithm>
#include <cmath>

namespace flues::pm {

class HitStrategy : public InterfaceStrategy {
public:
    explicit HitStrategy(float sampleRate)
        : InterfaceStrategy(sampleRate) {}

    float process(float input) override {
        const float drive = 2.0f + intensity * 8.0f;
        const float folded = sineFold(input, drive);
        const float hardness = 0.35f + intensity * 0.55f;
        const float shaped = (folded >= 0.0f ? 1.0f : -1.0f) * std::pow(std::abs(folded), hardness);
        return std::clamp(shaped, -1.0f, 1.0f);
    }

    void reset() override {}

    const char* getName() const override {
        return "HitStrategy";
    }
};

} // namespace flues::pm
