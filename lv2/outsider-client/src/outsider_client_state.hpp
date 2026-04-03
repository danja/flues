#pragma once

#include "outsider/command_packet.hpp"

#include <cstdint>

namespace outsider_client {

struct PersistedConfig {
    float enable = 1.0f;
    float session_slot = 1.0f;
    float endpoint_slot = 0.0f;
    float authority = 1.0f;
    float reconnect = 1.0f;
    float fallback_gain = 1.0f;
    float demo_mode = 0.0f;
};

inline float runtime_state_to_port(outsider::RuntimeState state) {
    return static_cast<float>(static_cast<std::uint8_t>(state));
}

inline float mode_to_port(outsider::OutsiderMode mode) {
    return static_cast<float>(static_cast<std::uint8_t>(mode));
}

enum class DemoMode : std::uint8_t {
    Off = 0,
    Pulse = 1,
    PMix = 2,
    EMix = 3
};

struct ServerParamsSnapshot {
    outsider::OutsiderMode mode = outsider::OutsiderMode::Bypass;
    int p_mix_granularity_bars = 0;
    float p_mix_bias_percent = 0.0f;
    int e_mix_steps = 0;
    int e_mix_division = 0;
    int e_mix_offset = 0;
};

inline DemoMode demo_mode_from_port(float value) {
    int rounded = static_cast<int>(value + 0.5f);
    if (rounded < 0) rounded = 0;
    if (rounded > 3) rounded = 3;
    return static_cast<DemoMode>(rounded);
}

inline float demo_mode_to_port(DemoMode mode) {
    return static_cast<float>(static_cast<std::uint8_t>(mode));
}

inline outsider::RuntimeState command_state_to_runtime(outsider::TargetState state) {
    switch (state) {
        case outsider::TargetState::Mute: return outsider::RuntimeState::Mute;
        case outsider::TargetState::FadeIn: return outsider::RuntimeState::FadeIn;
        case outsider::TargetState::FadeOut: return outsider::RuntimeState::FadeOut;
        case outsider::TargetState::Play:
        default: return outsider::RuntimeState::Play;
    }
}

}  // namespace outsider_client
