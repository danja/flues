#include "outsider/semaphore_models.hpp"

#include <algorithm>

namespace outsider {

SemaphoreDecision PMixModel::preview(const TransportSnapshot& transport,
                                     const PMixParams& params,
                                     bool currently_audible) const {
    SemaphoreDecision decision{};
    decision.active = true;
    decision.command.mode = OutsiderMode::PMix;
    decision.command.apply_at_bar = static_cast<std::uint32_t>(transport.bar) + static_cast<std::uint32_t>(std::max(1, params.granularity_bars));
    decision.command.apply_at_step16 = 0;

    const float maintain = std::max(0.0f, params.maintain_weight);
    const float fade = std::max(0.0f, params.fade_weight);
    const float cut = std::max(0.0f, params.cut_weight);
    const bool prefer_audible = params.bias_percent >= 50.0f;

    if (maintain >= fade && maintain >= cut) {
        decision.command.target_state = currently_audible ? TargetState::Play : TargetState::Mute;
        decision.command.target_gain = currently_audible ? 1.0f : 0.0f;
        decision.command.duration_beats = 0.0f;
        return decision;
    }

    if (fade >= cut) {
        decision.command.target_state = prefer_audible ? TargetState::FadeIn : TargetState::FadeOut;
        decision.command.target_gain = prefer_audible ? 1.0f : 0.0f;
        decision.command.duration_beats = std::max(0.5f, 4.0f * params.fade_dur_max_fraction);
        return decision;
    }

    decision.command.target_state = prefer_audible ? TargetState::Play : TargetState::Mute;
    decision.command.target_gain = prefer_audible ? 1.0f : 0.0f;
    decision.command.duration_beats = 0.0f;
    return decision;
}

}  // namespace outsider

