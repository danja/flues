#pragma once

#include "outsider/command_packet.hpp"

#include <cstdint>

namespace outsider_client {

struct PersistedConfig {
    float enable = 1.0f;
    float session_slot = 1.0f;
    float endpoint_slot = 1.0f;
    float authority = 0.0f;
    float reconnect = 1.0f;
    float fallback_gain = 1.0f;
};

enum class RuntimeState : std::uint8_t {
    Bypass = 0,
    Play = 1,
    Mute = 2,
    FadeIn = 3,
    FadeOut = 4
};

inline float runtime_state_to_port(RuntimeState state) {
    return static_cast<float>(static_cast<std::uint8_t>(state));
}

inline float mode_to_port(outsider::OutsiderMode mode) {
    return static_cast<float>(static_cast<std::uint8_t>(mode));
}

inline RuntimeState command_state_to_runtime(outsider::TargetState state) {
    switch (state) {
        case outsider::TargetState::Mute: return RuntimeState::Mute;
        case outsider::TargetState::FadeIn: return RuntimeState::FadeIn;
        case outsider::TargetState::FadeOut: return RuntimeState::FadeOut;
        case outsider::TargetState::Play:
        default: return RuntimeState::Play;
    }
}

}  // namespace outsider_client

