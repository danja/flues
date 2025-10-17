#pragma once

#include "../InterfaceStrategy.hpp"
#include "../utils/NonlinearityLib.hpp"

namespace flues::pm {

class ReedStrategy : public InterfaceStrategy {
public:
    explicit ReedStrategy(float sampleRate)
        : InterfaceStrategy(sampleRate) {}

    float process(float input) override {
        const float stiffness = 2.5f + intensity * 10.0f;
        const float bias = (intensity - 0.5f) * 0.25f;
        const float excited = (input + bias) * stiffness;
        const float core = fastTanh(excited);
        const float gain = 0.6f + intensity * 0.5f;
        const float output = std::clamp(core * gain - bias * 0.3f, -1.0f, 1.0f);
        return output;
    }

    void reset() override {}

    const char* getName() const override {
        return "ReedStrategy";
    }
};

} // namespace flues::pm
