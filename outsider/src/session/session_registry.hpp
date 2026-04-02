#pragma once

#include "outsider/protocol.hpp"
#include "outsider/semaphore_models.hpp"
#include "outsider/transport_snapshot.hpp"

#include <cstdint>
#include <vector>

namespace outsider {

struct EndpointRecord {
    std::uint16_t session_slot = 1;
    std::uint16_t endpoint_slot = 1;
    bool connected = false;
    bool authority_claimed = false;
    bool authority_active = false;
    OutsiderMode mode = OutsiderMode::Bypass;
    TargetState current_state = TargetState::Play;
    float current_gain = 1.0f;
    std::uint64_t last_command_id = 0;
    std::uint64_t last_heartbeat_ms = 0;
    PMixParams p_mix_params{};
    EMixParams e_mix_params{};
};

class SessionRegistry {
public:
    SessionRegistry();

    void clear();
    void seed_demo_data();

    const std::vector<EndpointRecord>& endpoints() const;
    const TransportSnapshot& transport() const;
    std::uint16_t selected_session() const;

private:
    std::vector<EndpointRecord> endpoints_;
    TransportSnapshot transport_;
    std::uint16_t selected_session_ = 1;
};

}  // namespace outsider

