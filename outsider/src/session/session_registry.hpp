#pragma once

#include "outsider/protocol.hpp"
#include "outsider/semaphore_models.hpp"
#include "outsider/transport_snapshot.hpp"

#include <cstdint>
#include <mutex>
#include <vector>

namespace outsider {

struct PersistedEndpointState {
    std::uint16_t session_slot = 1;
    std::uint16_t endpoint_slot = 1;
    std::uint8_t group_slot = 0;
    bool follows_group = false;
    OutsiderMode mode = OutsiderMode::Bypass;
    PMixParams p_mix_params{};
    EMixParams e_mix_params{};
};

struct PersistedGroupState {
    std::uint16_t session_slot = 1;
    std::uint8_t group_slot = 1;
    OutsiderMode mode = OutsiderMode::Bypass;
    PMixParams p_mix_params{};
    EMixParams e_mix_params{};
};

struct PersistedSessionState {
    std::uint16_t selected_session = 1;
    std::vector<PersistedGroupState> groups;
    std::vector<PersistedEndpointState> endpoints;
};

struct GroupRecord {
    std::uint16_t session_slot = 1;
    std::uint8_t group_slot = 1;
    OutsiderMode mode = OutsiderMode::Bypass;
    std::uint64_t params_revision = 1;
    PMixParams p_mix_params{};
    EMixParams e_mix_params{};
};

struct EffectiveSemaphoreState {
    OutsiderMode mode = OutsiderMode::Bypass;
    PMixParams p_mix_params{};
    EMixParams e_mix_params{};
    bool from_group = false;
    std::uint8_t group_slot = 0;
    std::uint64_t params_revision = 0;
};

struct EndpointRecord {
    std::uint16_t session_slot = 1;
    std::uint16_t endpoint_slot = 1;
    std::uint8_t group_slot = 0;
    bool follows_group = false;
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
    bool assign_endpoint_group(std::uint16_t session_slot,
                               std::uint16_t endpoint_slot,
                               std::uint8_t group_slot);
    bool set_endpoint_follows_group(std::uint16_t session_slot,
                                    std::uint16_t endpoint_slot,
                                    bool follows_group);
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
    bool set_group_mode(std::uint16_t session_slot,
                        std::uint8_t group_slot,
                        OutsiderMode mode);
    bool adjust_group_p_mix_granularity(std::uint16_t session_slot,
                                        std::uint8_t group_slot,
                                        int delta);
    bool adjust_group_p_mix_bias(std::uint16_t session_slot,
                                 std::uint8_t group_slot,
                                 float delta_percent);
    bool adjust_group_e_mix_steps(std::uint16_t session_slot,
                                  std::uint8_t group_slot,
                                  int delta);
    bool adjust_group_e_mix_offset(std::uint16_t session_slot,
                                   std::uint8_t group_slot,
                                   int delta);
    bool endpoint_snapshot(std::uint16_t session_slot,
                           std::uint16_t endpoint_slot,
                           EndpointRecord* out) const;
    bool group_snapshot(std::uint16_t session_slot,
                        std::uint8_t group_slot,
                        GroupRecord* out) const;
    bool effective_semaphore_state(std::uint16_t session_slot,
                                   std::uint16_t endpoint_slot,
                                   EffectiveSemaphoreState* out) const;
    PersistedSessionState persistent_snapshot() const;
    void restore_persisted_state(const PersistedSessionState& state);
    std::uint64_t persistent_revision() const;

    std::vector<EndpointRecord> endpoints_snapshot() const;
    std::vector<GroupRecord> groups_snapshot(std::uint16_t session_slot = 0) const;
    TransportSnapshot transport_snapshot() const;
    std::uint16_t selected_session() const;

private:
    EndpointRecord& upsert_endpoint_locked(std::uint16_t session_slot,
                                           std::uint16_t endpoint_slot,
                                           bool* created = nullptr);
    GroupRecord& upsert_group_locked(std::uint16_t session_slot,
                                     std::uint8_t group_slot,
                                     bool* created = nullptr,
                                     const EndpointRecord* seed_from_endpoint = nullptr);
    static void apply_default_mode_locked(EndpointRecord* endpoint);
    static void apply_default_group_locked(GroupRecord* group);
    static void apply_persisted_state_locked(EndpointRecord* endpoint,
                                             const PersistedEndpointState& persisted);
    static void apply_persisted_group_locked(GroupRecord* group,
                                             const PersistedGroupState& persisted);
    void touch_group_followers_locked(std::uint16_t session_slot,
                                      std::uint8_t group_slot);
    void bump_persistent_revision_locked();

    mutable std::mutex mutex_;
    std::vector<EndpointRecord> endpoints_;
    std::vector<GroupRecord> groups_;
    TransportSnapshot transport_;
    std::uint16_t selected_session_ = 1;
    std::uint64_t persistent_revision_ = 1;
};

}  // namespace outsider
