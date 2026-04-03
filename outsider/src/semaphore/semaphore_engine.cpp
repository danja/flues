#include "semaphore_engine.hpp"

#include "../session/session_registry.hpp"
#include "../transport/transport_authority.hpp"
#include "outsider/semaphore_models.hpp"

namespace outsider {

CommandPacket SemaphoreEngine::preview_for(const EndpointRecord& endpoint,
                                           const SessionRegistry& registry,
                                           const TransportAuthority&) const {
    const TransportSnapshot transport = registry.transport_snapshot();
    EffectiveSemaphoreState effective{};
    if (!registry.effective_semaphore_state(endpoint.session_slot,
                                            endpoint.endpoint_slot,
                                            &effective)) {
        effective.mode = endpoint.mode;
        effective.p_mix_params = endpoint.p_mix_params;
        effective.e_mix_params = endpoint.e_mix_params;
    }

    if (effective.mode == OutsiderMode::PMix) {
        PMixModel model;
        return model.preview(transport,
                             effective.p_mix_params,
                             endpoint.current_gain > 0.5f).command;
    }
    if (effective.mode == OutsiderMode::EMix) {
        EMixModel model;
        return model.preview(transport, effective.e_mix_params).command;
    }

    CommandPacket bypass{};
    bypass.mode = OutsiderMode::Bypass;
    bypass.target_state = TargetState::Play;
    bypass.target_gain = 1.0f;
    bypass.duration_beats = 0.0f;
    bypass.apply_at_bar = static_cast<std::uint32_t>(transport.bar);
    bypass.apply_at_step16 = 0;
    return bypass;
}

}  // namespace outsider
