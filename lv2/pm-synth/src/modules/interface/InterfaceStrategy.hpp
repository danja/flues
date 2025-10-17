#pragma once

#include <stdexcept>
#include <algorithm>

namespace flues::pm {

enum class InterfaceType : int {
    PLUCK = 0,
    HIT = 1,
    REED = 2,
    FLUTE = 3,
    BRASS = 4,
    BOW = 5,
    BELL = 6,
    DRUM = 7,
    CRYSTAL = 8,
    VAPOR = 9,
    QUANTUM = 10,
    PLASMA = 11
};

class InterfaceStrategy {
public:
    explicit InterfaceStrategy(float sampleRate = 44100.0f)
        : sampleRate(sampleRate),
          intensity(0.5f),
          gate(false),
          previousGate(false) {}

    virtual ~InterfaceStrategy() = default;

    virtual float process(float input) = 0;
    virtual void reset() = 0;

    virtual void onNoteOn() {}

    void setIntensity(float value) {
        intensity = std::clamp(value, 0.0f, 1.0f);
    }

    void setGate(bool gateState) {
        previousGate = gate;
        gate = gateState;
        if (gate && !previousGate) {
            onNoteOn();
        }
    }

    float getIntensity() const {
        return intensity;
    }

    virtual const char* getName() const {
        return "InterfaceStrategy";
    }

protected:
    float sampleRate;
    float intensity;
    bool gate;
    bool previousGate;
};

} // namespace flues::pm
