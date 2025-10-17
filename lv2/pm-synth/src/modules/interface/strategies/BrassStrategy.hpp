#pragma once

#include "../InterfaceStrategy.hpp"
#include "../utils/NonlinearityLib.hpp"
#include <algorithm>

namespace flues::pm {

class BrassStrategy : public InterfaceStrategy {
public:
    explicit BrassStrategy(float sampleRate)
        : InterfaceStrategy(sampleRate) {}

    float process(float input) override {
        const float drive = 1.5f + intensity * 5.0f;
        float shaped = 0.0f;

        if (input >= 0.0f) {
            const float lifted = input * drive + (0.2f + intensity * 0.35f);
            shaped = fastTanh(std::max(lifted, 0.0f));
        } else {
            const float compressed = -input * (drive * (0.4f + intensity * 0.4f));
            const float limited = std::min(compressed, 1.5f);
            shaped = -std::pow(limited, 1.3f) * (0.35f + (1.0f - intensity) * 0.25f);
        }

        const float buzz = fastTanh(shaped * (1.2f + intensity * 1.5f));
        return std::clamp(buzz + intensity * 0.05f, -1.0f, 1.0f);
    }

    void reset() override {}

    const char* getName() const override {
        return "BrassStrategy";
    }
};

} // namespace flues::pm
