#include "bassgen_state.hpp"

#include "bassgen_pattern.hpp"
#include "bassgen_variation.hpp"

#include <cstring>

namespace {

struct LegacyControlSnapshotV1 {
    int root_note;
    int scale;
    int genre;
    int channel;
    int length_beats;
    int subdivision;
    float density;
    int reg;
    float hold;
    float accent;
    uint32_t seed;
    int action_new;
    int action_notes;
    int action_rhythm;
};

inline int clampi(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

ControlSnapshot convert_legacy_controls(const LegacyControlSnapshotV1& legacy) {
    ControlSnapshot controls = bassgen_default_controls();
    controls.root_note = legacy.root_note;
    controls.scale = legacy.scale;
    controls.genre = legacy.genre;
    controls.channel = legacy.channel;
    controls.length_beats = legacy.length_beats;
    controls.subdivision = legacy.subdivision;
    controls.density = legacy.density;
    controls.reg = legacy.reg;
    controls.hold = legacy.hold;
    controls.accent = legacy.accent;
    controls.seed = legacy.seed;
    controls.action_new = legacy.action_new;
    controls.action_notes = legacy.action_notes;
    controls.action_rhythm = legacy.action_rhythm;
    controls.vary = 0.0f;
    return controls;
}

}  // namespace

LV2_State_Status bassgen_save_state(LV2_State_Store_Function store,
                                    LV2_State_Handle handle,
                                    const BassGenStateURIDs& urids,
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

LV2_State_Status bassgen_restore_state(LV2_State_Retrieve_Function retrieve,
                                       LV2_State_Handle handle,
                                       const BassGenStateURIDs& urids,
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
        *controls = bassgen_clamp_controls(*controls);
    }

    const void* pattern_data = retrieve(handle, urids.pattern, &size, &type, &flags);
    if (pattern_data && size == sizeof(PatternStateBlob) && type == urids.atom_chunk) {
        memcpy(pattern, pattern_data, sizeof(PatternStateBlob));
        pattern->version = BASSGEN_PATTERN_STATE_VERSION;
        pattern->event_count = clampi(pattern->event_count, 0, BASSGEN_MAX_EVENTS);
        pattern->pattern_steps = clampi(pattern->pattern_steps, 1, BASSGEN_MAX_PATTERN_STEPS);
        pattern->steps_per_beat = clampi(pattern->steps_per_beat, 1, 6);
        *pattern_valid = true;
    }

    const void* variation_data = retrieve(handle, urids.variation, &size, &type, &flags);
    if (variation_data && size == sizeof(VariationStateBlob) && type == urids.atom_chunk) {
        memcpy(variation, variation_data, sizeof(VariationStateBlob));
        variation->version = BASSGEN_VARIATION_STATE_VERSION;
    } else {
        bassgen_reset_variation_progress(variation);
    }

    return LV2_STATE_SUCCESS;
}
