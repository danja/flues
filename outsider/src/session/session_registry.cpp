#include "session_registry.hpp"

#include <algorithm>

namespace outsider {

SessionRegistry::SessionRegistry() = default;

void SessionRegistry::bump_persistent_revision_locked() {
    ++persistent_revision_;
}

void SessionRegistry::apply_default_mode_locked(EndpointRecord* endpoint) {
    if (!endpoint) {
        return;
    }

    endpoint->params_revision = 1;
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

void SessionRegistry::apply_persisted_state_locked(EndpointRecord* endpoint,
                                                   const PersistedEndpointState& persisted) {
    if (!endpoint) {
        return;
    }

    endpoint->session_slot = persisted.session_slot;
    endpoint->endpoint_slot = persisted.endpoint_slot;
    endpoint->mode = persisted.mode;
    endpoint->p_mix_params = persisted.p_mix_params;
    endpoint->e_mix_params = persisted.e_mix_params;
    endpoint->connected = false;
    endpoint->authority_claimed = false;
    endpoint->authority_active = false;
    endpoint->current_state = RuntimeState::Bypass;
    endpoint->current_gain = 1.0f;
    endpoint->last_command_id = 0;
    endpoint->last_heartbeat_ms = 0;
    endpoint->last_transport_block_counter = 0;
    endpoint->peak_l = 0.0f;
    endpoint->peak_r = 0.0f;
    endpoint->params_revision = 1;
}

EndpointRecord& SessionRegistry::upsert_endpoint_locked(std::uint16_t session_slot,
                                                        std::uint16_t endpoint_slot,
                                                        bool* created) {
    if (created) {
        *created = false;
    }
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
    if (created) {
        *created = true;
    }
    return endpoints_.back();
}

void SessionRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    endpoints_.clear();
    transport_ = {};
    selected_session_ = 1;
    bump_persistent_revision_locked();
}

void SessionRegistry::mark_hello(std::uint16_t session_slot,
                                 std::uint16_t endpoint_slot,
                                 bool authority_claimed) {
    std::lock_guard<std::mutex> lock(mutex_);
    bool created = false;
    EndpointRecord& endpoint = upsert_endpoint_locked(session_slot, endpoint_slot, &created);
    endpoint.connected = true;
    endpoint.authority_claimed = authority_claimed;
    endpoint.last_heartbeat_ms = 1;
    if (selected_session_ == 0 || selected_session_ == 1) {
        const std::uint16_t previous = selected_session_;
        selected_session_ = session_slot;
        if (selected_session_ != previous) {
            bump_persistent_revision_locked();
        }
    }
    if (created) {
        bump_persistent_revision_locked();
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
    bool created = false;
    EndpointRecord& endpoint = upsert_endpoint_locked(transport.session_slot,
                                                      transport.endpoint_slot,
                                                      &created);
    endpoint.connected = true;
    endpoint.authority_claimed = transport.authority;
    endpoint.last_transport_block_counter = transport.block_counter;
    if (selected_session_ == 0 || selected_session_ == 1) {
        const std::uint16_t previous = selected_session_;
        selected_session_ = transport.session_slot;
        if (selected_session_ != previous) {
            bump_persistent_revision_locked();
        }
    }
    if (created) {
        bump_persistent_revision_locked();
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
    bool created = false;
    EndpointRecord& endpoint = upsert_endpoint_locked(session_slot, endpoint_slot, &created);
    endpoint.connected = true;
    endpoint.current_state = current_state;
    endpoint.current_gain = std::clamp(current_gain, 0.0f, 1.0f);
    endpoint.last_command_id = last_command_id;
    endpoint.peak_l = std::max(0.0f, peak_l);
    endpoint.peak_r = std::max(0.0f, peak_r);
    endpoint.last_heartbeat_ms = 1;
    if (selected_session_ == 0 || selected_session_ == 1) {
        const std::uint16_t previous = selected_session_;
        selected_session_ = session_slot;
        if (selected_session_ != previous) {
            bump_persistent_revision_locked();
        }
    }
    if (created) {
        bump_persistent_revision_locked();
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
            endpoint.params_revision++;
            bump_persistent_revision_locked();
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
            endpoint.params_revision++;
            bump_persistent_revision_locked();
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
            endpoint.params_revision++;
            bump_persistent_revision_locked();
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
            endpoint.params_revision++;
            bump_persistent_revision_locked();
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
            endpoint.params_revision++;
            bump_persistent_revision_locked();
            return true;
        }
    }
    return false;
}

bool SessionRegistry::endpoint_snapshot(std::uint16_t session_slot,
                                        std::uint16_t endpoint_slot,
                                        EndpointRecord* out) const {
    if (!out) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for (const EndpointRecord& endpoint : endpoints_) {
        if (endpoint.session_slot == session_slot && endpoint.endpoint_slot == endpoint_slot) {
            *out = endpoint;
            return true;
        }
    }
    return false;
}

PersistedSessionState SessionRegistry::persistent_snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);

    PersistedSessionState snapshot{};
    snapshot.selected_session = selected_session_ > 0 ? selected_session_ : 1;
    snapshot.endpoints.reserve(endpoints_.size());

    for (const EndpointRecord& endpoint : endpoints_) {
        PersistedEndpointState persisted{};
        persisted.session_slot = endpoint.session_slot;
        persisted.endpoint_slot = endpoint.endpoint_slot;
        persisted.mode = endpoint.mode;
        persisted.p_mix_params = endpoint.p_mix_params;
        persisted.e_mix_params = endpoint.e_mix_params;
        snapshot.endpoints.push_back(persisted);
    }

    std::sort(snapshot.endpoints.begin(),
              snapshot.endpoints.end(),
              [](const PersistedEndpointState& a, const PersistedEndpointState& b) {
                  if (a.session_slot != b.session_slot) {
                      return a.session_slot < b.session_slot;
                  }
                  return a.endpoint_slot < b.endpoint_slot;
              });

    return snapshot;
}

void SessionRegistry::restore_persisted_state(const PersistedSessionState& state) {
    std::lock_guard<std::mutex> lock(mutex_);

    endpoints_.clear();
    transport_ = {};
    selected_session_ = state.selected_session > 0 ? state.selected_session : 1;

    endpoints_.reserve(state.endpoints.size());
    for (const PersistedEndpointState& persisted : state.endpoints) {
        EndpointRecord endpoint{};
        apply_default_mode_locked(&endpoint);
        apply_persisted_state_locked(&endpoint, persisted);
        endpoints_.push_back(endpoint);
    }

    bump_persistent_revision_locked();
}

std::uint64_t SessionRegistry::persistent_revision() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return persistent_revision_;
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
