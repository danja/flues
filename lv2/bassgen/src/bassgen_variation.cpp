#include "bassgen_variation.hpp"

#include "bassgen_pattern.hpp"
#include "bassgen_rng.hpp"

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

int loops_between_mutations(float vary, double bars_per_loop) {
    const float target_bars = 1.0f + 7.0f * powf(1.0f - vary, 2.5f);
    const double safe_bars_per_loop = bars_per_loop > 0.0 ? bars_per_loop : 1.0;
    return clampi((int)ceil(target_bars / safe_bars_per_loop - 1e-9), 1, 64);
}

float partial_note_strength(float vary) {
    return clampf(0.16f + vary * 0.84f, 0.16f, 1.0f);
}

}  // namespace

void bassgen_reset_variation_progress(VariationStateBlob* variation) {
    if (!variation) {
        return;
    }

    variation->version = BASSGEN_VARIATION_STATE_VERSION;
    variation->completed_loops = 0;
    variation->last_mutation_loop = 0;
}

bool bassgen_apply_loop_variation(PatternStateBlob* pattern,
                                  VariationStateBlob* variation,
                                  const ControlSnapshot& controls,
                                  double beats_per_bar) {
    if (!pattern || !variation) {
        return false;
    }

    if (variation->version != BASSGEN_VARIATION_STATE_VERSION) {
        bassgen_reset_variation_progress(variation);
    }

    if (variation->completed_loops < std::numeric_limits<int64_t>::max()) {
        variation->completed_loops += 1;
    }

    const float vary = clampf(controls.vary, 0.0f, 1.0f);
    if (vary <= 0.0001f) {
        return false;
    }

    const double safe_beats_per_bar = beats_per_bar > 0.0 ? beats_per_bar : 4.0;
    const double bars_per_loop = (double)controls.length_beats / safe_beats_per_bar;
    const int interval_loops = loops_between_mutations(vary, bars_per_loop);
    if ((variation->completed_loops - variation->last_mutation_loop) < interval_loops) {
        return false;
    }

    variation->last_mutation_loop = variation->completed_loops;

    if (vary >= 0.999f) {
        bassgen_regenerate_pattern(pattern, controls, true, true);
        return true;
    }

    BassGenRng decision_rng;
    decision_rng.seed(controls.seed ^
                      ((uint32_t)variation->completed_loops * 2246822519u) ^
                      ((uint32_t)pattern->generation_serial * 3266489917u));

    const float roll = decision_rng.next_float();
    if (vary < 0.20f) {
        bassgen_partial_note_mutation(pattern, controls, partial_note_strength(vary) * 0.45f);
    } else if (vary < 0.45f) {
        if (roll < 0.68f) {
            bassgen_partial_note_mutation(pattern, controls, partial_note_strength(vary) * 0.70f);
        } else {
            bassgen_regenerate_pattern(pattern, controls, false, true);
        }
    } else if (vary < 0.75f) {
        if (roll < 0.30f) {
            bassgen_partial_note_mutation(pattern, controls, partial_note_strength(vary));
        } else if (roll < 0.74f) {
            bassgen_regenerate_pattern(pattern, controls, false, true);
        } else {
            bassgen_regenerate_pattern(pattern, controls, true, false);
        }
    } else {
        if (roll < 0.16f) {
            bassgen_partial_note_mutation(pattern, controls, partial_note_strength(vary));
        } else if (roll < 0.38f) {
            bassgen_regenerate_pattern(pattern, controls, false, true);
        } else if (roll < 0.70f) {
            bassgen_regenerate_pattern(pattern, controls, true, false);
        } else {
            bassgen_regenerate_pattern(pattern, controls, true, true);
        }
    }

    return true;
}
