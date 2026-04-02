#pragma once

#include "outsider/protocol.hpp"
#include "outsider/semaphore_models.hpp"
#include "outsider/transport_snapshot.hpp"

#include <cstdint>
#include <mutex>
#include <vector>

namespace outsider {

struct EndpointRecord {
    std::uint16_t session_slot = 1;
    std::uint16_t endpoint_slot = 1;
    bool connected = false;
    bool authority_claimed = false;
    bool authority_active = false;
    OutsiderMode mode = OutsiderMode::Bypass;
    RuntimeState current_state = RuntimeState::Bypass;
    float current_gain = 1.0f;
    std::uint64_t last_command_id = 0;
    std::uint64_t last_heartbeat_ms = 0;
    std::uint64_t last_transport_block_counter = 0;
    float peak_l = 0.0f;
    float peak_r = 0.0f;
    std::uint64_t params_revision = 1;
    PMixParams p_mix_params{};
    EMixParams e_mix_params{};
};

class SessionRegistry {
public:
    SessionRegistry();

    void clear();
    void mark_hello(std::uint16_t session_slot,
                    std::uint16_t endpoint_slot,
                    bool authority_claimed);
    void mark_goodbye(std::uint16_t session_slot,
                      std::uint16_t endpoint_slot);
    void update_transport(const TransportSnapshot& transport);
    void update_status(std::uint16_t session_slot,
                       std::uint16_t endpoint_slot,
                       RuntimeState current_state,
                       float current_gain,
                       std::uint64_t last_command_id,
                       float peak_l,
                       float peak_r);
    void set_authority_active(std::uint16_t session_slot,
                              std::uint16_t endpoint_slot);
    bool set_endpoint_mode(std::uint16_t session_slot,
                           std::uint16_t endpoint_slot,
                           OutsiderMode mode);
    bool adjust_p_mix_granularity(std::uint16_t session_slot,
                                  std::uint16_t endpoint_slot,
                                  int delta);
    bool adjust_p_mix_bias(std::uint16_t session_slot,
                           std::uint16_t endpoint_slot,
                           float delta_percent);
    bool adjust_e_mix_steps(std::uint16_t session_slot,
                            std::uint16_t endpoint_slot,
                            int delta);
    bool adjust_e_mix_offset(std::uint16_t session_slot,
                             std::uint16_t endpoint_slot,
                             int delta);
    bool endpoint_snapshot(std::uint16_t session_slot,
                           std::uint16_t endpoint_slot,
                           EndpointRecord* out) const;

    std::vector<EndpointRecord> endpoints_snapshot() const;
    TransportSnapshot transport_snapshot() const;
    std::uint16_t selected_session() const;

private:
    EndpointRecord& upsert_endpoint_locked(std::uint16_t session_slot,
                                           std::uint16_t endpoint_slot);
    static void apply_default_mode_locked(EndpointRecord* endpoint);

    mutable std::mutex mutex_;
    std::vector<EndpointRecord> endpoints_;
    TransportSnapshot transport_;
    std::uint16_t selected_session_ = 1;
};

}  // namespace outsider
