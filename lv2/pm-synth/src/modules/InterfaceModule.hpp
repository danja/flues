#pragma once

#include <memory>

#include "interface/InterfaceFactory.hpp"

namespace flues::pm {

class InterfaceModule {
public:
    explicit InterfaceModule(float sampleRate = 44100.0f)
        : sampleRate(sampleRate),
          currentType(InterfaceType::REED),
          strategy(InterfaceFactory::createStrategy(currentType, sampleRate)),
          gateState(false) {}

    void setType(int typeValue) {
        if (!InterfaceFactory::isValidType(typeValue)) {
            return;
        }

        const InterfaceType type = static_cast<InterfaceType>(typeValue);
        if (type != currentType) {
            const float oldIntensity = strategy->getIntensity();
            currentType = type;
            strategy = InterfaceFactory::createStrategy(currentType, sampleRate);
            strategy->setIntensity(oldIntensity);
            strategy->setGate(gateState);
        }
    }

    void setIntensity(float value) {
        strategy->setIntensity(value);
    }

    float process(float input) {
        return strategy->process(input);
    }

    void setGate(bool gate) {
        gateState = gate;
        strategy->setGate(gateState);
    }

    void reset() {
        strategy->reset();
        if (gateState) {
            strategy->setGate(true);
        }
    }

    InterfaceType getType() const {
        return currentType;
    }

    float getIntensity() const {
        return strategy->getIntensity();
    }

    const char* getStrategyName() const {
        return strategy->getName();
    }

private:
    float sampleRate;
    InterfaceType currentType;
    std::unique_ptr<InterfaceStrategy> strategy;
    bool gateState;
};

} // namespace flues::pm
