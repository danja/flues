#pragma once

#include "../InterfaceStrategy.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "../utils/ExcitationGen.hpp"

namespace flues::pm {

class QuantumStrategy : public InterfaceStrategy {
public:
    explicit QuantumStrategy(float sampleRate)
        : InterfaceStrategy(sampleRate) {}

    float process(float input) override {
        const int bitDepth = 8 - static_cast<int>(std::floor(intensity * 5.0f));
        const float levels = std::pow(2.0f, static_cast<float>(bitDepth));

        const float scaled = input * levels;
        const float quantized = std::round(scaled) / levels;

        const float nearBoundary = std::abs(scaled - std::round(scaled));
        float boundaryNoise = 0.0f;
        if (nearBoundary > 0.45f) {
            boundaryNoise = whiteNoise(0.01f * intensity, &rng);
        }

        const float output = quantized + boundaryNoise;
        return std::clamp(output, -1.0f, 1.0f);
    }

    void reset() override {}

    const char* getName() const override {
        return "QuantumStrategy";
    }

private:
    Random rng;
};

} // namespace flues::pm
