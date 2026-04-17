#include "drumgen_state.hpp"

#include "drumgen_pattern.hpp"
#include "drumgen_variation.hpp"

#include <cstring>

namespace {

struct LegacyControlSnapshotV1 {
    int genre;
    int channel;
    int kit_map;
    int bars;
    int resolution;
    float density;
    float variation;
    float fill;
    uint32_t seed;
    float kick_amt;
    float backbeat_amt;
    float hat_amt;
    float aux_amt;
    int action_new;
    int action_mutate;
    int action_fill;
};

inline int clampi(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

ControlSnapshot convert_legacy_controls(const LegacyControlSnapshotV1& legacy) {
    ControlSnapshot controls = drumgen_default_controls();
    controls.genre = legacy.genre;
    controls.channel = legacy.channel;
    controls.kit_map = legacy.kit_map;
    controls.bars = legacy.bars;
    controls.resolution = legacy.resolution;
    controls.density = legacy.density;
    controls.variation = legacy.variation;
    controls.fill = legacy.fill;
    controls.seed = legacy.seed;
    controls.kick_amt = legacy.kick_amt;
    controls.backbeat_amt = legacy.backbeat_amt;
    controls.hat_amt = legacy.hat_amt;
    controls.aux_amt = legacy.aux_amt;
    controls.tom_amt = legacy.aux_amt;
    controls.metal_amt = legacy.aux_amt;
    controls.action_new = legacy.action_new;
    controls.action_mutate = legacy.action_mutate;
    controls.action_fill = legacy.action_fill;
    controls.vary = 0.0f;
    return controls;
}

}  // namespace

LV2_State_Status drumgen_save_state(LV2_State_Store_Function store,
                                    LV2_State_Handle handle,
                                    const DrumGenStateURIDs& urids,
                                    const ControlSnapshot& controls,
                                    const PatternStateBlob& pattern,
                                    const VariationStateBlob& variation) {
    if (!store) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    const uint32_t flags = LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE;
    store(handle, urids.controls, &controls, sizeof(ControlSnapshot), urids.atom_chunk, flags);
    store(handle, urids.pattern, &pattern, sizeof(PatternStateBlob), urids.atom_chunk, flags);
    store(handle, urids.variation, &variation, sizeof(VariationStateBlob), urids.atom_chunk, flags);
    return LV2_STATE_SUCCESS;
}

LV2_State_Status drumgen_restore_state(LV2_State_Retrieve_Function retrieve,
                                       LV2_State_Handle handle,
                                       const DrumGenStateURIDs& urids,
                                       ControlSnapshot* controls,
                                       PatternStateBlob* pattern,
                                       VariationStateBlob* variation,
                                       bool* pattern_valid) {
    if (!retrieve || !controls || !pattern || !variation || !pattern_valid) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    size_t size = 0;
    uint32_t type = 0;
    uint32_t flags = 0;

    const void* controls_data = retrieve(handle, urids.controls, &size, &type, &flags);
    if (controls_data && type == urids.atom_chunk) {
        if (size == sizeof(ControlSnapshot)) {
            memcpy(controls, controls_data, sizeof(ControlSnapshot));
        } else if (size == sizeof(LegacyControlSnapshotV1)) {
            const LegacyControlSnapshotV1* legacy = (const LegacyControlSnapshotV1*)controls_data;
            *controls = convert_legacy_controls(*legacy);
        }
        *controls = drumgen_clamp_controls(*controls);
    }

    const void* pattern_data = retrieve(handle, urids.pattern, &size, &type, &flags);
    if (pattern_data && size == sizeof(PatternStateBlob) && type == urids.atom_chunk) {
        memcpy(pattern, pattern_data, sizeof(PatternStateBlob));
        pattern->version = DRUMGEN_PATTERN_STATE_VERSION;
        pattern->bars = clampi(pattern->bars, DRUMGEN_MIN_BARS, DRUMGEN_MAX_BARS);
        pattern->steps_per_beat = clampi(pattern->steps_per_beat, 1, 4);
        pattern->steps_per_bar = clampi(pattern->steps_per_bar, 4, 16);
        pattern->total_steps = clampi(pattern->total_steps, 1, DRUMGEN_MAX_PATTERN_STEPS);
        *pattern_valid = true;
    }

    const void* variation_data = retrieve(handle, urids.variation, &size, &type, &flags);
    if (variation_data && size == sizeof(VariationStateBlob) && type == urids.atom_chunk) {
        memcpy(variation, variation_data, sizeof(VariationStateBlob));
        variation->version = DRUMGEN_VARIATION_STATE_VERSION;
    } else {
        drumgen_reset_variation_progress(variation);
    }

    return LV2_STATE_SUCCESS;
}
