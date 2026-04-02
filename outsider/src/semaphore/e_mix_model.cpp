#include "outsider/semaphore_models.hpp"

#include <algorithm>
#include <cmath>

namespace outsider {

static bool euclid_hit(int step_index, int pulses, int slots) {
    if (slots <= 0 || pulses <= 0) {
        return false;
    }
    if (pulses >= slots) {
        return true;
    }
    const int current = static_cast<int>(std::floor((step_index * pulses) / static_cast<double>(slots)));
    const int next = static_cast<int>(std::floor(((step_index + 1) * pulses) / static_cast<double>(slots)));
    return current != next;
}

SemaphoreDecision EMixModel::preview(const TransportSnapshot& transport,
                                     const EMixParams& params) const {
    SemaphoreDecision decision{};
    decision.active = true;
    decision.command.mode = OutsiderMode::EMix;

    const int total_bars = std::max(1, params.total_bars);
    const int division = std::max(1, params.division);
    const int steps = std::clamp(params.steps, 0, division);
    const double bars_per_step = total_bars / static_cast<double>(division);

    const double absolute_bar = transport.bar + (transport.beat / std::max(1.0, transport.beats_per_bar));
    const double cycle_bar = std::fmod(absolute_bar, static_cast<double>(total_bars));
    const int step_index = std::clamp(static_cast<int>(std::floor(cycle_bar / bars_per_step)), 0, division - 1);
    const int rotated_step = (step_index + params.offset % division + division) % division;
    const bool active = euclid_hit(rotated_step, steps, division);

    decision.command.target_state = active ? TargetState::Play : TargetState::Mute;
    decision.command.target_gain = active ? 1.0f : 0.0f;
    decision.command.duration_beats = std::max(0.0f, params.fade_bars) * static_cast<float>(transport.beats_per_bar);
    decision.command.apply_at_bar = static_cast<std::uint32_t>(transport.bar);
    decision.command.apply_at_step16 = static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::floor((transport.beat / std::max(1.0, transport.beats_per_bar)) * 16.0)), 0, 15));
    return decision;
}

}  // namespace outsider

