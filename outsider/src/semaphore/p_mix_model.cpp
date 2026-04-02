#include "outsider/semaphore_models.hpp"

#include <algorithm>
#include <cmath>

namespace outsider {

static std::uint32_t hash_u32(std::uint32_t value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

SemaphoreDecision PMixModel::preview(const TransportSnapshot& transport,
                                     const PMixParams& params,
                                     bool currently_audible) const {
    SemaphoreDecision decision{};
    decision.active = true;
    decision.command.mode = OutsiderMode::PMix;
    const int granularity_bars = std::max(1, params.granularity_bars);
    const std::uint32_t current_bar = static_cast<std::uint32_t>(std::max(0.0, std::floor(transport.bar)));
    const std::uint32_t boundary_index = current_bar / static_cast<std::uint32_t>(granularity_bars);
    decision.command.apply_at_bar = (boundary_index + 1u) * static_cast<std::uint32_t>(granularity_bars);
    decision.command.apply_at_step16 = 0;

    const float maintain = std::max(0.0f, params.maintain_weight);
    const float fade = std::max(0.0f, params.fade_weight);
    const float cut = std::max(0.0f, params.cut_weight);
    const float total = std::max(maintain + fade + cut, 1.0f);
    const std::uint32_t h = hash_u32(params.seed ^ (boundary_index * 131u + 17u));
    const float transition_pick =
        (static_cast<float>(h & 0xffffu) / 65535.0f) * total;
    const float bias_percent = std::clamp(params.bias_percent, 0.0f, 100.0f);
    const bool prefer_audible = ((h >> 16) % 100u) < static_cast<std::uint32_t>(bias_percent);

    if (transition_pick < maintain) {
        decision.command.target_state = currently_audible ? TargetState::Play : TargetState::Mute;
        decision.command.target_gain = currently_audible ? 1.0f : 0.0f;
        decision.command.duration_beats = 0.0f;
        return decision;
    }

    if (transition_pick < maintain + fade) {
        decision.command.target_state = prefer_audible ? TargetState::FadeIn : TargetState::FadeOut;
        decision.command.target_gain = prefer_audible ? 1.0f : 0.0f;
        const float beats_per_bar = static_cast<float>(std::max(1.0, transport.beats_per_bar));
        const float max_fade_beats =
            std::max(0.25f, static_cast<float>(granularity_bars) * beats_per_bar *
                                std::max(0.0f, params.fade_dur_max_fraction));
        decision.command.duration_beats = max_fade_beats;
        return decision;
    }

    decision.command.target_state = prefer_audible ? TargetState::Play : TargetState::Mute;
    decision.command.target_gain = prefer_audible ? 1.0f : 0.0f;
    decision.command.duration_beats = 0.0f;
    return decision;
}

}  // namespace outsider
