#pragma once

#include "../InterfaceStrategy.hpp"
#include <algorithm>
#include <cmath>

namespace flues::pm {

class PluckStrategy : public InterfaceStrategy {
public:
    explicit PluckStrategy(float sampleRate)
        : InterfaceStrategy(sampleRate),
          lastPeak(0.0f),
          peakDecay(0.999f),
          prevInput(0.0f) {}

    float process(float input) override {
        const float brightness = 0.2f + intensity * 0.45f;
        float response = 0.0f;

        if (std::abs(input) > std::abs(lastPeak)) {
            lastPeak = input;
            response = input;
        } else {
            lastPeak *= peakDecay;
            const float transient = (input - prevInput) * brightness;
            const float damp = 0.35f + (1.0f - intensity) * 0.45f;
            response = input * damp + transient;
        }

        prevInput = input;
        return std::clamp(response, -1.0f, 1.0f);
    }

    void reset() override {
        lastPeak = 0.0f;
        prevInput = 0.0f;
    }

    void onNoteOn() override {
        reset();
    }

    const char* getName() const override {
        return "PluckStrategy";
    }

private:
    float lastPeak;
    float peakDecay;
    float prevInput;
};

} // namespace flues::pm
