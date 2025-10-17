#pragma once

#include <array>
#include <memory>

#include "InterfaceStrategy.hpp"
#include "strategies/PluckStrategy.hpp"
#include "strategies/HitStrategy.hpp"
#include "strategies/ReedStrategy.hpp"
#include "strategies/FluteStrategy.hpp"
#include "strategies/BrassStrategy.hpp"
#include "strategies/BowStrategy.hpp"
#include "strategies/BellStrategy.hpp"
#include "strategies/DrumStrategy.hpp"
#include "strategies/CrystalStrategy.hpp"
#include "strategies/VaporStrategy.hpp"
#include "strategies/QuantumStrategy.hpp"
#include "strategies/PlasmaStrategy.hpp"

namespace flues::pm {

class InterfaceFactory {
public:
    static std::unique_ptr<InterfaceStrategy> createStrategy(InterfaceType type, float sampleRate) {
        switch (type) {
            case InterfaceType::PLUCK:   return std::make_unique<PluckStrategy>(sampleRate);
            case InterfaceType::HIT:     return std::make_unique<HitStrategy>(sampleRate);
            case InterfaceType::REED:    return std::make_unique<ReedStrategy>(sampleRate);
            case InterfaceType::FLUTE:   return std::make_unique<FluteStrategy>(sampleRate);
            case InterfaceType::BRASS:   return std::make_unique<BrassStrategy>(sampleRate);
            case InterfaceType::BOW:     return std::make_unique<BowStrategy>(sampleRate);
            case InterfaceType::BELL:    return std::make_unique<BellStrategy>(sampleRate);
            case InterfaceType::DRUM:    return std::make_unique<DrumStrategy>(sampleRate);
            case InterfaceType::CRYSTAL: return std::make_unique<CrystalStrategy>(sampleRate);
            case InterfaceType::VAPOR:   return std::make_unique<VaporStrategy>(sampleRate);
            case InterfaceType::QUANTUM: return std::make_unique<QuantumStrategy>(sampleRate);
            case InterfaceType::PLASMA:  return std::make_unique<PlasmaStrategy>(sampleRate);
            default:                     return std::make_unique<ReedStrategy>(sampleRate);
        }
    }

    static bool isValidType(int value) {
        return value >= static_cast<int>(InterfaceType::PLUCK) &&
               value <= static_cast<int>(InterfaceType::PLASMA);
    }
};

} // namespace flues::pm
