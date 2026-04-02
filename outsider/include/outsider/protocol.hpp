#pragma once

#include <cstdint>

namespace outsider {

inline constexpr std::uint32_t kProtocolVersion = 1;
inline constexpr const char* kDefaultListenUri = "ws://127.0.0.1:7342/outsider/v1";

enum class OutsiderMode : std::uint8_t {
    Bypass = 0,
    PMix = 1,
    EMix = 2
};

enum class TargetState : std::uint8_t {
    Play = 0,
    Mute = 1,
    FadeIn = 2,
    FadeOut = 3
};

inline const char* mode_name(OutsiderMode mode) {
    switch (mode) {
        case OutsiderMode::PMix: return "P-Mix";
        case OutsiderMode::EMix: return "E-Mix";
        case OutsiderMode::Bypass:
        default: return "Bypass";
    }
}

inline const char* target_state_name(TargetState state) {
    switch (state) {
        case TargetState::Mute: return "Mute";
        case TargetState::FadeIn: return "Fade In";
        case TargetState::FadeOut: return "Fade Out";
        case TargetState::Play:
        default: return "Play";
    }
}

}  // namespace outsider

