#include "drumgen_variation.hpp"

#include "drumgen_pattern.hpp"
#include "drumgen_rng.hpp"

#include <cmath>
#include <cstdint>
#include <limits>

namespace {

inline float clampf(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

inline int clampi(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

int loops_between_mutations(float vary, int bars_per_loop) {
    const float target_bars = 1.0f + 7.0f * powf(1.0f - vary, 2.5f);
    const int safe_bars_per_loop = bars_per_loop > 0 ? bars_per_loop : 1;
    return clampi((int)ceilf(target_bars / (float)safe_bars_per_loop - 1e-6f), 1, 64);
}

int pick_bar_index(const PatternStateBlob* pattern, const ControlSnapshot& controls, DrumGenRng* rng) {
    if (!pattern || pattern->bars <= 1) {
        return 0;
    }
    if (controls.fill > 0.12f && rng->next_float() < 0.55f) {
        return pattern->bars - 1;
    }
    return rng->next_int(0, pattern->bars - 1);
}

}  // namespace

void drumgen_reset_variation_progress(VariationStateBlob* variation) {
    if (!variation) {
        return;
    }
    variation->version = DRUMGEN_VARIATION_STATE_VERSION;
    variation->completed_loops = 0;
    variation->last_mutation_loop = 0;
}

bool drumgen_apply_loop_variation(PatternStateBlob* pattern,
                                  VariationStateBlob* variation,
                                  const ControlSnapshot& controls) {
    if (!pattern || !variation) {
        return false;
    }

    if (variation->version != DRUMGEN_VARIATION_STATE_VERSION) {
        drumgen_reset_variation_progress(variation);
    }

    if (variation->completed_loops < std::numeric_limits<int64_t>::max()) {
        variation->completed_loops += 1;
    }

    const float vary = clampf(controls.vary, 0.0f, 1.0f);
    if (vary <= 0.0001f) {
        return false;
    }

    const int interval_loops = loops_between_mutations(vary, controls.bars);
    if ((variation->completed_loops - variation->last_mutation_loop) < interval_loops) {
        return false;
    }

    variation->last_mutation_loop = variation->completed_loops;

    if (vary >= 0.999f) {
        drumgen_regenerate_pattern(pattern, controls, false);
        return true;
    }

    DrumGenRng decision_rng;
    decision_rng.seed(controls.seed ^
                      ((uint32_t)variation->completed_loops * 2246822519u) ^
                      ((uint32_t)pattern->generation_serial * 3266489917u));

    const float roll = decision_rng.next_float();
    if (vary < 0.20f) {
        drumgen_refresh_bar(pattern, controls, pick_bar_index(pattern, controls, &decision_rng));
    } else if (vary < 0.45f) {
        if (roll < 0.60f) {
            drumgen_refresh_bar(pattern, controls, pick_bar_index(pattern, controls, &decision_rng));
        } else {
            drumgen_regenerate_pattern(pattern, controls, true);
        }
    } else if (vary < 0.75f) {
        if (roll < 0.24f) {
            drumgen_refresh_bar(pattern, controls, pick_bar_index(pattern, controls, &decision_rng));
            if (pattern->bars > 1 && decision_rng.next_float() < 0.35f) {
                drumgen_refresh_bar(pattern, controls, pick_bar_index(pattern, controls, &decision_rng));
            }
        } else if (roll < 0.58f) {
            drumgen_regenerate_pattern(pattern, controls, true);
        } else {
            drumgen_regenerate_pattern(pattern, controls, false);
        }
    } else {
        if (roll < 0.14f) {
            drumgen_refresh_bar(pattern, controls, pick_bar_index(pattern, controls, &decision_rng));
        } else if (roll < 0.34f) {
            drumgen_regenerate_pattern(pattern, controls, true);
        } else {
            drumgen_regenerate_pattern(pattern, controls, false);
        }
    }

    return true;
}
