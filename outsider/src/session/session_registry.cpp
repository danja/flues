#include "session_registry.hpp"

#include <algorithm>

namespace outsider {

SessionRegistry::SessionRegistry() = default;

void SessionRegistry::apply_default_mode_locked(EndpointRecord* endpoint) {
    if (!endpoint) {
        return;
    }

    endpoint->mode = (endpoint->endpoint_slot % 2u == 0u) ? OutsiderMode::EMix : OutsiderMode::PMix;
    endpoint->current_state = RuntimeState::Bypass;
    endpoint->current_gain = 1.0f;
    endpoint->last_command_id = 0;

    endpoint->p_mix_params.granularity_bars = 2;
    endpoint->p_mix_params.maintain_weight = 25.0f;
    endpoint->p_mix_params.fade_weight = 45.0f;
    endpoint->p_mix_params.cut_weight = 30.0f;
    endpoint->p_mix_params.fade_dur_max_fraction = 0.5f;
    endpoint->p_mix_params.bias_percent = 58.0f;

    endpoint->e_mix_params.total_bars = 8;
    endpoint->e_mix_params.division = 8;
    endpoint->e_mix_params.steps = 5;
    endpoint->e_mix_params.offset = static_cast<int>(endpoint->endpoint_slot % 8u);
    endpoint->e_mix_params.fade_bars = 0.25f;
}

EndpointRecord& SessionRegistry::upsert_endpoint_locked(std::uint16_t session_slot,
                                                        std::uint16_t endpoint_slot) {
    for (EndpointRecord& endpoint : endpoints_) {
        if (endpoint.session_slot == session_slot && endpoint.endpoint_slot == endpoint_slot) {
            return endpoint;
        }
    }

    EndpointRecord endpoint{};
    endpoint.session_slot = session_slot;
    endpoint.endpoint_slot = endpoint_slot;
    apply_default_mode_locked(&endpoint);
    endpoints_.push_back(endpoint);
    return endpoints_.back();
}

void SessionRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    endpoints_.clear();
    transport_ = {};
    selected_session_ = 1;
}

void SessionRegistry::mark_hello(std::uint16_t session_slot,
                                 std::uint16_t endpoint_slot,
                                 bool authority_claimed) {
    std::lock_guard<std::mutex> lock(mutex_);
    EndpointRecord& endpoint = upsert_endpoint_locked(session_slot, endpoint_slot);
    endpoint.connected = true;
    endpoint.authority_claimed = authority_claimed;
    endpoint.last_heartbeat_ms = 1;
    if (selected_session_ == 0 || selected_session_ == 1) {
        selected_session_ = session_slot;
    }
}

void SessionRegistry::mark_goodbye(std::uint16_t session_slot,
                                   std::uint16_t endpoint_slot) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (EndpointRecord& endpoint : endpoints_) {
        if (endpoint.session_slot == session_slot && endpoint.endpoint_slot == endpoint_slot) {
            endpoint.connected = false;
            endpoint.authority_active = false;
            endpoint.last_heartbeat_ms = 0;
            return;
        }
    }
}

void SessionRegistry::update_transport(const TransportSnapshot& transport) {
    std::lock_guard<std::mutex> lock(mutex_);
    transport_ = transport;
    EndpointRecord& endpoint = upsert_endpoint_locked(transport.session_slot, transport.endpoint_slot);
    endpoint.connected = true;
    endpoint.authority_claimed = transport.authority;
    endpoint.last_transport_block_counter = transport.block_counter;
    if (selected_session_ == 0 || selected_session_ == 1) {
        selected_session_ = transport.session_slot;
    }
}

void SessionRegistry::update_status(std::uint16_t session_slot,
                                    std::uint16_t endpoint_slot,
                                    RuntimeState current_state,
                                    float current_gain,
                                    std::uint64_t last_command_id,
                                    float peak_l,
                                    float peak_r) {
    std::lock_guard<std::mutex> lock(mutex_);
    EndpointRecord& endpoint = upsert_endpoint_locked(session_slot, endpoint_slot);
    endpoint.connected = true;
    endpoint.current_state = current_state;
    endpoint.current_gain = std::clamp(current_gain, 0.0f, 1.0f);
    endpoint.last_command_id = last_command_id;
    endpoint.peak_l = std::max(0.0f, peak_l);
    endpoint.peak_r = std::max(0.0f, peak_r);
    endpoint.last_heartbeat_ms = 1;
    if (selected_session_ == 0 || selected_session_ == 1) {
        selected_session_ = session_slot;
    }
}

void SessionRegistry::set_authority_active(std::uint16_t session_slot,
                                           std::uint16_t endpoint_slot) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (EndpointRecord& endpoint : endpoints_) {
        endpoint.authority_active =
            endpoint.connected &&
            endpoint.session_slot == session_slot &&
            endpoint.endpoint_slot == endpoint_slot;
    }
}

bool SessionRegistry::set_endpoint_mode(std::uint16_t session_slot,
                                        std::uint16_t endpoint_slot,
                                        OutsiderMode mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (EndpointRecord& endpoint : endpoints_) {
        if (endpoint.session_slot == session_slot && endpoint.endpoint_slot == endpoint_slot) {
            endpoint.mode = mode;
            return true;
        }
    }
    return false;
}

bool SessionRegistry::adjust_p_mix_granularity(std::uint16_t session_slot,
                                               std::uint16_t endpoint_slot,
                                               int delta) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (EndpointRecord& endpoint : endpoints_) {
        if (endpoint.session_slot == session_slot && endpoint.endpoint_slot == endpoint_slot) {
            endpoint.p_mix_params.granularity_bars =
                std::clamp(endpoint.p_mix_params.granularity_bars + delta, 1, 16);
            return true;
        }
    }
    return false;
}

bool SessionRegistry::adjust_p_mix_bias(std::uint16_t session_slot,
                                        std::uint16_t endpoint_slot,
                                        float delta_percent) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (EndpointRecord& endpoint : endpoints_) {
        if (endpoint.session_slot == session_slot && endpoint.endpoint_slot == endpoint_slot) {
            endpoint.p_mix_params.bias_percent =
                std::clamp(endpoint.p_mix_params.bias_percent + delta_percent, 0.0f, 100.0f);
            return true;
        }
    }
    return false;
}

bool SessionRegistry::adjust_e_mix_steps(std::uint16_t session_slot,
                                         std::uint16_t endpoint_slot,
                                         int delta) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (EndpointRecord& endpoint : endpoints_) {
        if (endpoint.session_slot == session_slot && endpoint.endpoint_slot == endpoint_slot) {
            endpoint.e_mix_params.steps =
                std::clamp(endpoint.e_mix_params.steps + delta, 0, endpoint.e_mix_params.division);
            return true;
        }
    }
    return false;
}

bool SessionRegistry::adjust_e_mix_offset(std::uint16_t session_slot,
                                          std::uint16_t endpoint_slot,
                                          int delta) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (EndpointRecord& endpoint : endpoints_) {
        if (endpoint.session_slot == session_slot && endpoint.endpoint_slot == endpoint_slot) {
            const int division = std::max(1, endpoint.e_mix_params.division);
            int offset = endpoint.e_mix_params.offset + delta;
            while (offset < 0) {
                offset += division;
            }
            endpoint.e_mix_params.offset = offset % division;
            return true;
        }
    }
    return false;
}

std::vector<EndpointRecord> SessionRegistry::endpoints_snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return endpoints_;
}

TransportSnapshot SessionRegistry::transport_snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return transport_;
}

std::uint16_t SessionRegistry::selected_session() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return selected_session_;
}

}  // namespace outsider
