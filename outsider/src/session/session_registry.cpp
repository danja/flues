#include "session_registry.hpp"

namespace outsider {

SessionRegistry::SessionRegistry() = default;

void SessionRegistry::clear() {
    endpoints_.clear();
    transport_ = {};
    selected_session_ = 1;
}

void SessionRegistry::seed_demo_data() {
    clear();

    selected_session_ = 1;
    transport_.session_slot = 1;
    transport_.endpoint_slot = 1;
    transport_.authority = true;
    transport_.playing = true;
    transport_.bar = 12.0;
    transport_.beat = 2.5;
    transport_.beats_per_bar = 4.0;
    transport_.bpm = 126.0;
    transport_.sample_rate = 48000.0;
    transport_.block_size = 256;
    transport_.block_counter = 182044;

    EndpointRecord drums;
    drums.session_slot = 1;
    drums.endpoint_slot = 1;
    drums.connected = true;
    drums.authority_claimed = true;
    drums.authority_active = true;
    drums.mode = OutsiderMode::PMix;
    drums.current_state = TargetState::FadeIn;
    drums.current_gain = 0.82f;
    drums.last_command_id = 41;
    drums.last_heartbeat_ms = 120;
    drums.p_mix_params.granularity_bars = 4;
    drums.p_mix_params.maintain_weight = 60.0f;
    drums.p_mix_params.fade_weight = 30.0f;
    drums.p_mix_params.cut_weight = 10.0f;
    drums.p_mix_params.bias_percent = 58.0f;

    EndpointRecord pads;
    pads.session_slot = 1;
    pads.endpoint_slot = 2;
    pads.connected = true;
    pads.authority_claimed = false;
    pads.authority_active = false;
    pads.mode = OutsiderMode::EMix;
    pads.current_state = TargetState::Mute;
    pads.current_gain = 0.00f;
    pads.last_command_id = 22;
    pads.last_heartbeat_ms = 145;
    pads.e_mix_params.total_bars = 64;
    pads.e_mix_params.division = 16;
    pads.e_mix_params.steps = 5;
    pads.e_mix_params.offset = 2;
    pads.e_mix_params.fade_bars = 0.5f;

    EndpointRecord bass;
    bass.session_slot = 1;
    bass.endpoint_slot = 3;
    bass.connected = false;
    bass.authority_claimed = false;
    bass.authority_active = false;
    bass.mode = OutsiderMode::Bypass;
    bass.current_state = TargetState::Play;
    bass.current_gain = 1.0f;
    bass.last_command_id = 0;
    bass.last_heartbeat_ms = 0;

    endpoints_.push_back(drums);
    endpoints_.push_back(pads);
    endpoints_.push_back(bass);
}

const std::vector<EndpointRecord>& SessionRegistry::endpoints() const {
    return endpoints_;
}

const TransportSnapshot& SessionRegistry::transport() const {
    return transport_;
}

std::uint16_t SessionRegistry::selected_session() const {
    return selected_session_;
}

}  // namespace outsider

