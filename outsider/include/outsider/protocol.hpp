#pragma once

#include <cstdint>
#include <string_view>

namespace outsider {

inline constexpr std::uint32_t kProtocolVersion = 1;
inline constexpr const char* kDefaultListenUri = "ws://127.0.0.1:7342/outsider/v1";
inline constexpr const char* kDefaultListenHost = "127.0.0.1";
inline constexpr std::uint16_t kDefaultListenPort = 7342;
inline constexpr const char* kDefaultListenPath = "/outsider/v1";

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

enum class RuntimeState : std::uint8_t {
    Bypass = 0,
    Play = 1,
    Mute = 2,
    FadeIn = 3,
    FadeOut = 4
};

inline const char* mode_name(OutsiderMode mode) {
    switch (mode) {
        case OutsiderMode::PMix: return "P-Mix";
        case OutsiderMode::EMix: return "E-Mix";
        case OutsiderMode::Bypass:
        default: return "Bypass";
    }
}

inline const char* mode_wire_name(OutsiderMode mode) {
    switch (mode) {
        case OutsiderMode::PMix: return "p_mix";
        case OutsiderMode::EMix: return "e_mix";
        case OutsiderMode::Bypass:
        default: return "bypass";
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

inline const char* target_state_wire_name(TargetState state) {
    switch (state) {
        case TargetState::Mute: return "mute";
        case TargetState::FadeIn: return "fade-in";
        case TargetState::FadeOut: return "fade-out";
        case TargetState::Play:
        default: return "play";
    }
}

inline const char* runtime_state_name(RuntimeState state) {
    switch (state) {
        case RuntimeState::Play: return "Play";
        case RuntimeState::Mute: return "Mute";
        case RuntimeState::FadeIn: return "Fade In";
        case RuntimeState::FadeOut: return "Fade Out";
        case RuntimeState::Bypass:
        default: return "Bypass";
    }
}

inline const char* runtime_state_wire_name(RuntimeState state) {
    switch (state) {
        case RuntimeState::Play: return "play";
        case RuntimeState::Mute: return "mute";
        case RuntimeState::FadeIn: return "fade-in";
        case RuntimeState::FadeOut: return "fade-out";
        case RuntimeState::Bypass:
        default: return "bypass";
    }
}

inline bool parse_mode_wire(std::string_view text, OutsiderMode* out) {
    if (!out) {
        return false;
    }
    if (text == "bypass") {
        *out = OutsiderMode::Bypass;
        return true;
    }
    if (text == "p_mix") {
        *out = OutsiderMode::PMix;
        return true;
    }
    if (text == "e_mix") {
        *out = OutsiderMode::EMix;
        return true;
    }
    return false;
}

inline bool parse_target_state_wire(std::string_view text, TargetState* out) {
    if (!out) {
        return false;
    }
    if (text == "play") {
        *out = TargetState::Play;
        return true;
    }
    if (text == "mute") {
        *out = TargetState::Mute;
        return true;
    }
    if (text == "fade-in") {
        *out = TargetState::FadeIn;
        return true;
    }
    if (text == "fade-out") {
        *out = TargetState::FadeOut;
        return true;
    }
    return false;
}

inline bool parse_runtime_state_wire(std::string_view text, RuntimeState* out) {
    if (!out) {
        return false;
    }
    if (text == "bypass") {
        *out = RuntimeState::Bypass;
        return true;
    }
    if (text == "play") {
        *out = RuntimeState::Play;
        return true;
    }
    if (text == "mute") {
        *out = RuntimeState::Mute;
        return true;
    }
    if (text == "fade-in") {
        *out = RuntimeState::FadeIn;
        return true;
    }
    if (text == "fade-out") {
        *out = RuntimeState::FadeOut;
        return true;
    }
    return false;
}

}  // namespace outsider
