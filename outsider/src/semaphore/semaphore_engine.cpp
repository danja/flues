#include "semaphore_engine.hpp"

#include "../session/session_registry.hpp"
#include "../transport/transport_authority.hpp"
#include "outsider/semaphore_models.hpp"

namespace outsider {

CommandPacket SemaphoreEngine::preview_for(const EndpointRecord& endpoint,
                                           const SessionRegistry& registry,
                                           const TransportAuthority&) const {
    if (endpoint.mode == OutsiderMode::PMix) {
        PMixModel model;
        return model.preview(registry.transport(),
                             endpoint.p_mix_params,
                             endpoint.current_gain > 0.5f).command;
    }
    if (endpoint.mode == OutsiderMode::EMix) {
        EMixModel model;
        return model.preview(registry.transport(), endpoint.e_mix_params).command;
    }

    CommandPacket bypass{};
    bypass.mode = OutsiderMode::Bypass;
    bypass.target_state = TargetState::Play;
    bypass.target_gain = 1.0f;
    bypass.duration_beats = 0.0f;
    bypass.apply_at_bar = static_cast<std::uint32_t>(registry.transport().bar);
    bypass.apply_at_step16 = 0;
    return bypass;
}

}  // namespace outsider

