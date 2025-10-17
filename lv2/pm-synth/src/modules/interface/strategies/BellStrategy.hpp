#pragma once

#include "../InterfaceStrategy.hpp"
#include "../utils/NonlinearityLib.hpp"
#include <algorithm>
#include <cmath>

namespace flues::pm {

class BellStrategy : public InterfaceStrategy {
public:
    explicit BellStrategy(float sampleRate)
        : InterfaceStrategy(sampleRate),
          bellPhase(0.0f) {}

    float process(float input) override {
        bellPhase += 0.1f + intensity * 0.25f;
        if (bellPhase > 2.0f * static_cast<float>(M_PI)) {
            bellPhase -= 2.0f * static_cast<float>(M_PI);
        }

        const float harmonicSpread = 6.0f + intensity * 14.0f;
        const float even = std::sin(input * harmonicSpread + bellPhase) * (0.4f + intensity * 0.4f);
        const float odd = std::sin(input * (harmonicSpread * 0.5f + 2.0f)) * (0.2f + intensity * 0.3f);
        const float bright = fastTanh((even + odd) * (1.1f + intensity * 0.6f));
        return std::clamp(bright, -1.0f, 1.0f);
    }

    void reset() override {
        bellPhase = 0.0f;
    }

    void onNoteOn() override {
        reset();
    }

    const char* getName() const override {
        return "BellStrategy";
    }

private:
    float bellPhase;
};

} // namespace flues::pm
