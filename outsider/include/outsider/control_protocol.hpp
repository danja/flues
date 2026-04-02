#pragma once

#include "command_packet.hpp"
#include "protocol.hpp"
#include "semaphore_models.hpp"
#include "transport_snapshot.hpp"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <string_view>

namespace outsider {

enum class ControlMessageType : std::uint8_t {
    Invalid = 0,
    Hello,
    Transport,
    Status,
    Heartbeat,
    Goodbye,
    Welcome,
    Command,
    Params,
    Error
};

struct ControlMessage {
    ControlMessageType type = ControlMessageType::Invalid;
    std::uint32_t protocol_version = 0;
    std::uint16_t session_slot = 0;
    std::uint16_t endpoint_slot = 0;
    bool authority = false;
    bool authority_accepted = false;
    bool connected_to_server = false;
    bool playing = false;
    bool fatal = false;
    std::uint32_t heartbeat_interval_ms = 750;
    std::uint64_t block_counter = 0;
    std::uint64_t last_command_id = 0;
    double bar = 0.0;
    double beat = 0.0;
    double beats_per_bar = 4.0;
    double bpm = 120.0;
    double sample_rate = 48000.0;
    std::uint32_t block_size = 0;
    float current_gain = 1.0f;
    float peak_l = 0.0f;
    float peak_r = 0.0f;
    RuntimeState current_state = RuntimeState::Bypass;
    OutsiderMode mode = OutsiderMode::Bypass;
    TargetState target_state = TargetState::Play;
    CommandPacket command{};
    PMixParams p_mix_params{};
    EMixParams e_mix_params{};
    std::string server_name;
    std::string code;
    std::string message;
};

inline std::string json_escape_string(std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size() + 8);
    for (const char ch : text) {
        switch (ch) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped.push_back(ch); break;
        }
    }
    return escaped;
}

inline std::size_t find_json_value_start(std::string_view json, std::string_view key) {
    const std::string needle = std::string("\"") + std::string(key) + "\"";
    const std::size_t key_pos = json.find(needle);
    if (key_pos == std::string_view::npos) {
        return std::string_view::npos;
    }
    const std::size_t colon_pos = json.find(':', key_pos + needle.size());
    if (colon_pos == std::string_view::npos) {
        return std::string_view::npos;
    }
    std::size_t value_pos = colon_pos + 1;
    while (value_pos < json.size() && std::isspace(static_cast<unsigned char>(json[value_pos]))) {
        ++value_pos;
    }
    return value_pos;
}

inline bool json_get_bool(std::string_view json, std::string_view key, bool* out) {
    if (!out) {
        return false;
    }
    const std::size_t pos = find_json_value_start(json, key);
    if (pos == std::string_view::npos) {
        return false;
    }
    if (json.substr(pos, 4) == "true") {
        *out = true;
        return true;
    }
    if (json.substr(pos, 5) == "false") {
        *out = false;
        return true;
    }
    return false;
}

inline bool json_get_string(std::string_view json, std::string_view key, std::string* out) {
    if (!out) {
        return false;
    }
    const std::size_t pos = find_json_value_start(json, key);
    if (pos == std::string_view::npos || pos >= json.size() || json[pos] != '"') {
        return false;
    }
    std::string value;
    for (std::size_t i = pos + 1; i < json.size(); ++i) {
        const char ch = json[i];
        if (ch == '\\' && i + 1 < json.size()) {
            const char next = json[++i];
            switch (next) {
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                default: value.push_back(next); break;
            }
            continue;
        }
        if (ch == '"') {
            *out = value;
            return true;
        }
        value.push_back(ch);
    }
    return false;
}

inline bool json_get_uint64(std::string_view json, std::string_view key, std::uint64_t* out) {
    if (!out) {
        return false;
    }
    const std::size_t pos = find_json_value_start(json, key);
    if (pos == std::string_view::npos) {
        return false;
    }
    std::size_t end = pos;
    while (end < json.size()) {
        const char ch = json[end];
        if (!(std::isdigit(static_cast<unsigned char>(ch)) || ch == '+' || ch == '-')) {
            break;
        }
        ++end;
    }
    if (end == pos) {
        return false;
    }
    char buffer[64];
    const std::size_t len = std::min<std::size_t>(end - pos, sizeof(buffer) - 1);
    std::memcpy(buffer, json.data() + pos, len);
    buffer[len] = '\0';
    *out = static_cast<std::uint64_t>(std::strtoull(buffer, nullptr, 10));
    return true;
}

inline bool json_get_uint32(std::string_view json, std::string_view key, std::uint32_t* out) {
    std::uint64_t value = 0;
    if (!json_get_uint64(json, key, &value)) {
        return false;
    }
    *out = static_cast<std::uint32_t>(value);
    return true;
}

inline bool json_get_uint16(std::string_view json, std::string_view key, std::uint16_t* out) {
    std::uint64_t value = 0;
    if (!json_get_uint64(json, key, &value)) {
        return false;
    }
    *out = static_cast<std::uint16_t>(value);
    return true;
}

inline bool json_get_double(std::string_view json, std::string_view key, double* out) {
    if (!out) {
        return false;
    }
    const std::size_t pos = find_json_value_start(json, key);
    if (pos == std::string_view::npos) {
        return false;
    }
    std::size_t end = pos;
    while (end < json.size()) {
        const char ch = json[end];
        if (!(std::isdigit(static_cast<unsigned char>(ch)) || ch == '+' || ch == '-' || ch == '.' || ch == 'e' || ch == 'E')) {
            break;
        }
        ++end;
    }
    if (end == pos) {
        return false;
    }
    char buffer[64];
    const std::size_t len = std::min<std::size_t>(end - pos, sizeof(buffer) - 1);
    std::memcpy(buffer, json.data() + pos, len);
    buffer[len] = '\0';
    *out = std::strtod(buffer, nullptr);
    return true;
}

inline bool parse_control_message(std::string_view line, ControlMessage* out) {
    if (!out) {
        return false;
    }

    ControlMessage msg{};
    std::string type;
    if (!json_get_string(line, "type", &type)) {
        return false;
    }

    if (type == "hello") {
        msg.type = ControlMessageType::Hello;
        json_get_uint32(line, "protocol_version", &msg.protocol_version);
        json_get_uint16(line, "session_slot", &msg.session_slot);
        json_get_uint16(line, "endpoint_slot", &msg.endpoint_slot);
        json_get_bool(line, "authority", &msg.authority);
    } else if (type == "transport") {
        msg.type = ControlMessageType::Transport;
        json_get_uint16(line, "session_slot", &msg.session_slot);
        json_get_uint16(line, "endpoint_slot", &msg.endpoint_slot);
        json_get_bool(line, "playing", &msg.playing);
        json_get_double(line, "bar", &msg.bar);
        json_get_double(line, "beat", &msg.beat);
        json_get_double(line, "beats_per_bar", &msg.beats_per_bar);
        json_get_double(line, "bpm", &msg.bpm);
        json_get_double(line, "sample_rate", &msg.sample_rate);
        json_get_uint32(line, "block_size", &msg.block_size);
        json_get_uint64(line, "block_counter", &msg.block_counter);
    } else if (type == "status") {
        msg.type = ControlMessageType::Status;
        json_get_uint16(line, "session_slot", &msg.session_slot);
        json_get_uint16(line, "endpoint_slot", &msg.endpoint_slot);
        json_get_bool(line, "connected_to_server", &msg.connected_to_server);
        double value = 0.0;
        if (json_get_double(line, "current_gain", &value)) {
            msg.current_gain = static_cast<float>(value);
        }
        std::string state_text;
        if (json_get_string(line, "current_state", &state_text)) {
            parse_runtime_state_wire(state_text, &msg.current_state);
        }
        json_get_uint64(line, "last_command_id", &msg.last_command_id);
        if (json_get_double(line, "peak_l", &value)) {
            msg.peak_l = static_cast<float>(value);
        }
        if (json_get_double(line, "peak_r", &value)) {
            msg.peak_r = static_cast<float>(value);
        }
    } else if (type == "heartbeat") {
        msg.type = ControlMessageType::Heartbeat;
        json_get_uint16(line, "session_slot", &msg.session_slot);
        json_get_uint16(line, "endpoint_slot", &msg.endpoint_slot);
        json_get_uint64(line, "last_command_id", &msg.last_command_id);
    } else if (type == "goodbye") {
        msg.type = ControlMessageType::Goodbye;
        json_get_uint16(line, "session_slot", &msg.session_slot);
        json_get_uint16(line, "endpoint_slot", &msg.endpoint_slot);
    } else if (type == "welcome") {
        msg.type = ControlMessageType::Welcome;
        json_get_uint32(line, "protocol_version", &msg.protocol_version);
        json_get_string(line, "server_name", &msg.server_name);
        json_get_uint16(line, "session_slot", &msg.session_slot);
        json_get_uint16(line, "endpoint_slot", &msg.endpoint_slot);
        json_get_bool(line, "authority_accepted", &msg.authority_accepted);
        json_get_uint32(line, "heartbeat_interval_ms", &msg.heartbeat_interval_ms);
    } else if (type == "command") {
        msg.type = ControlMessageType::Command;
        json_get_uint16(line, "session_slot", &msg.session_slot);
        json_get_uint16(line, "endpoint_slot", &msg.endpoint_slot);
        json_get_uint64(line, "command_id", &msg.command.command_id);
        std::string mode_text;
        if (json_get_string(line, "mode", &mode_text)) {
            parse_mode_wire(mode_text, &msg.command.mode);
            msg.mode = msg.command.mode;
        }
        std::string state_text;
        if (json_get_string(line, "target_state", &state_text)) {
            parse_target_state_wire(state_text, &msg.command.target_state);
            msg.target_state = msg.command.target_state;
        }
        double value = 0.0;
        if (json_get_double(line, "target_gain", &value)) {
            msg.command.target_gain = static_cast<float>(value);
        }
        if (json_get_double(line, "duration_beats", &value)) {
            msg.command.duration_beats = static_cast<float>(value);
        }
        json_get_uint32(line, "apply_at_bar", &msg.command.apply_at_bar);
        std::uint32_t step16 = 0;
        if (json_get_uint32(line, "apply_at_step16", &step16)) {
            msg.command.apply_at_step16 = static_cast<std::uint8_t>(step16);
        }
    } else if (type == "params") {
        msg.type = ControlMessageType::Params;
        json_get_uint16(line, "session_slot", &msg.session_slot);
        json_get_uint16(line, "endpoint_slot", &msg.endpoint_slot);
        std::string mode_text;
        if (json_get_string(line, "mode", &mode_text)) {
            parse_mode_wire(mode_text, &msg.mode);
        }
        double value = 0.0;
        std::uint32_t int_value = 0;
        if (msg.mode == OutsiderMode::PMix) {
            if (json_get_uint32(line, "granularity_bars", &int_value)) {
                msg.p_mix_params.granularity_bars = static_cast<int>(int_value);
            }
            if (json_get_double(line, "maintain_weight", &value)) {
                msg.p_mix_params.maintain_weight = static_cast<float>(value);
            }
            if (json_get_double(line, "fade_weight", &value)) {
                msg.p_mix_params.fade_weight = static_cast<float>(value);
            }
            if (json_get_double(line, "cut_weight", &value)) {
                msg.p_mix_params.cut_weight = static_cast<float>(value);
            }
            if (json_get_double(line, "fade_dur_max_fraction", &value)) {
                msg.p_mix_params.fade_dur_max_fraction = static_cast<float>(value);
            }
            if (json_get_double(line, "bias_percent", &value)) {
                msg.p_mix_params.bias_percent = static_cast<float>(value);
            }
        } else if (msg.mode == OutsiderMode::EMix) {
            if (json_get_uint32(line, "total_bars", &int_value)) {
                msg.e_mix_params.total_bars = static_cast<int>(int_value);
            }
            if (json_get_uint32(line, "division", &int_value)) {
                msg.e_mix_params.division = static_cast<int>(int_value);
            }
            if (json_get_uint32(line, "steps", &int_value)) {
                msg.e_mix_params.steps = static_cast<int>(int_value);
            }
            if (json_get_double(line, "offset", &value)) {
                msg.e_mix_params.offset = static_cast<int>(value);
            }
            if (json_get_double(line, "fade_bars", &value)) {
                msg.e_mix_params.fade_bars = static_cast<float>(value);
            }
        }
    } else if (type == "error") {
        msg.type = ControlMessageType::Error;
        json_get_string(line, "code", &msg.code);
        json_get_string(line, "message", &msg.message);
        json_get_bool(line, "fatal", &msg.fatal);
        json_get_uint16(line, "session_slot", &msg.session_slot);
        json_get_uint16(line, "endpoint_slot", &msg.endpoint_slot);
    } else {
        return false;
    }

    *out = msg;
    return true;
}

inline std::string encode_hello_message(std::uint16_t session_slot,
                                        std::uint16_t endpoint_slot,
                                        bool authority) {
    std::ostringstream oss;
    oss << "{\"type\":\"hello\",\"protocol_version\":" << kProtocolVersion
        << ",\"session_slot\":" << session_slot
        << ",\"endpoint_slot\":" << endpoint_slot
        << ",\"authority\":" << (authority ? "true" : "false")
        << ",\"capabilities\":{\"audio_stereo\":true,\"midi\":false,\"control_only\":true}}";
    return oss.str();
}

inline std::string encode_transport_message(const TransportSnapshot& snapshot) {
    std::ostringstream oss;
    oss << "{\"type\":\"transport\""
        << ",\"session_slot\":" << snapshot.session_slot
        << ",\"endpoint_slot\":" << snapshot.endpoint_slot
        << ",\"playing\":" << (snapshot.playing ? "true" : "false")
        << ",\"bar\":" << snapshot.bar
        << ",\"beat\":" << snapshot.beat
        << ",\"beats_per_bar\":" << snapshot.beats_per_bar
        << ",\"bpm\":" << snapshot.bpm
        << ",\"sample_rate\":" << snapshot.sample_rate
        << ",\"block_size\":" << snapshot.block_size
        << ",\"block_counter\":" << snapshot.block_counter
        << "}";
    return oss.str();
}

inline std::string encode_status_message(std::uint16_t session_slot,
                                         std::uint16_t endpoint_slot,
                                         bool connected_to_server,
                                         float current_gain,
                                         RuntimeState current_state,
                                         std::uint64_t last_command_id,
                                         float peak_l,
                                         float peak_r) {
    std::ostringstream oss;
    oss << "{\"type\":\"status\""
        << ",\"session_slot\":" << session_slot
        << ",\"endpoint_slot\":" << endpoint_slot
        << ",\"connected_to_server\":" << (connected_to_server ? "true" : "false")
        << ",\"current_gain\":" << current_gain
        << ",\"current_state\":\"" << runtime_state_wire_name(current_state) << "\""
        << ",\"last_command_id\":" << last_command_id
        << ",\"peak_l\":" << peak_l
        << ",\"peak_r\":" << peak_r
        << "}";
    return oss.str();
}

inline std::string encode_heartbeat_message(std::uint16_t session_slot,
                                            std::uint16_t endpoint_slot,
                                            std::uint64_t last_command_id) {
    std::ostringstream oss;
    oss << "{\"type\":\"heartbeat\""
        << ",\"session_slot\":" << session_slot
        << ",\"endpoint_slot\":" << endpoint_slot
        << ",\"last_command_id\":" << last_command_id
        << "}";
    return oss.str();
}

inline std::string encode_goodbye_message(std::uint16_t session_slot,
                                          std::uint16_t endpoint_slot) {
    std::ostringstream oss;
    oss << "{\"type\":\"goodbye\""
        << ",\"session_slot\":" << session_slot
        << ",\"endpoint_slot\":" << endpoint_slot
        << "}";
    return oss.str();
}

inline std::string encode_welcome_message(std::uint16_t session_slot,
                                          std::uint16_t endpoint_slot,
                                          bool authority_accepted,
                                          std::uint32_t heartbeat_interval_ms,
                                          std::string_view server_name = "Outsider") {
    std::ostringstream oss;
    oss << "{\"type\":\"welcome\""
        << ",\"protocol_version\":" << kProtocolVersion
        << ",\"server_name\":\"" << json_escape_string(server_name) << "\""
        << ",\"session_slot\":" << session_slot
        << ",\"endpoint_slot\":" << endpoint_slot
        << ",\"authority_accepted\":" << (authority_accepted ? "true" : "false")
        << ",\"heartbeat_interval_ms\":" << heartbeat_interval_ms
        << "}";
    return oss.str();
}

inline std::string encode_command_message(std::uint16_t session_slot,
                                          std::uint16_t endpoint_slot,
                                          const CommandPacket& command) {
    std::ostringstream oss;
    oss << "{\"type\":\"command\""
        << ",\"command_id\":" << command.command_id
        << ",\"session_slot\":" << session_slot
        << ",\"endpoint_slot\":" << endpoint_slot
        << ",\"mode\":\"" << mode_wire_name(command.mode) << "\""
        << ",\"target_state\":\"" << target_state_wire_name(command.target_state) << "\""
        << ",\"target_gain\":" << command.target_gain
        << ",\"duration_beats\":" << command.duration_beats
        << ",\"apply_at_bar\":" << command.apply_at_bar
        << ",\"apply_at_step16\":" << static_cast<unsigned>(command.apply_at_step16)
        << "}";
    return oss.str();
}

inline std::string encode_params_message(std::uint16_t session_slot,
                                         std::uint16_t endpoint_slot,
                                         OutsiderMode mode,
                                         const PMixParams& p_mix_params,
                                         const EMixParams& e_mix_params) {
    std::ostringstream oss;
    oss << "{\"type\":\"params\""
        << ",\"session_slot\":" << session_slot
        << ",\"endpoint_slot\":" << endpoint_slot
        << ",\"mode\":\"" << mode_wire_name(mode) << "\"";

    if (mode == OutsiderMode::PMix) {
        oss << ",\"params\":{"
            << "\"granularity_bars\":" << p_mix_params.granularity_bars
            << ",\"maintain_weight\":" << p_mix_params.maintain_weight
            << ",\"fade_weight\":" << p_mix_params.fade_weight
            << ",\"cut_weight\":" << p_mix_params.cut_weight
            << ",\"fade_dur_max_fraction\":" << p_mix_params.fade_dur_max_fraction
            << ",\"bias_percent\":" << p_mix_params.bias_percent
            << "}";
    } else if (mode == OutsiderMode::EMix) {
        oss << ",\"params\":{"
            << "\"total_bars\":" << e_mix_params.total_bars
            << ",\"division\":" << e_mix_params.division
            << ",\"steps\":" << e_mix_params.steps
            << ",\"offset\":" << e_mix_params.offset
            << ",\"fade_bars\":" << e_mix_params.fade_bars
            << "}";
    } else {
        oss << ",\"params\":{}";
    }

    oss << "}";
    return oss.str();
}

inline std::string encode_error_message(std::string_view code,
                                        std::string_view message,
                                        bool fatal,
                                        std::uint16_t session_slot = 0,
                                        std::uint16_t endpoint_slot = 0) {
    std::ostringstream oss;
    oss << "{\"type\":\"error\""
        << ",\"code\":\"" << json_escape_string(code) << "\""
        << ",\"message\":\"" << json_escape_string(message) << "\""
        << ",\"fatal\":" << (fatal ? "true" : "false");
    if (session_slot > 0) {
        oss << ",\"session_slot\":" << session_slot;
    }
    if (endpoint_slot > 0) {
        oss << ",\"endpoint_slot\":" << endpoint_slot;
    }
    oss << "}";
    return oss.str();
}

}  // namespace outsider
