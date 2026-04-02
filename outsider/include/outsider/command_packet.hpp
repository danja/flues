#pragma once

#include "protocol.hpp"

#include <cstdint>

namespace outsider {

struct CommandPacket {
    std::uint64_t command_id = 0;
    OutsiderMode mode = OutsiderMode::Bypass;
    TargetState target_state = TargetState::Play;
    float target_gain = 1.0f;
    float duration_beats = 0.0f;
    std::uint32_t apply_at_bar = 0;
    std::uint8_t apply_at_step16 = 0;
};

}  // namespace outsider

