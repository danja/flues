#pragma once

#include <memory>

#include "interface/InterfaceFactory.hpp"

namespace flues::pm {

class InterfaceModule {
public:
    explicit InterfaceModule(float sampleRate = 44100.0f)
        : sampleRate(sampleRate),
          currentType(InterfaceType::REED),
          strategy(InterfaceFactory::createStrategy(currentType, sampleRate)) {}

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
        }
    }

    void setIntensity(float value) {
        strategy->setIntensity(value);
    }

    float process(float input) {
        return strategy->process(input);
    }

    void reset() {
        strategy->reset();
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
};

} // namespace flues::pm
