#include "cadence_state.hpp"

#include <algorithm>
#include <cstring>

namespace {

inline int clampi(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

void sort_notes(uint8_t* notes, uint8_t count) {
    std::sort(notes, notes + count);
}

}  // namespace

LV2_State_Status cadence_save_state(LV2_State_Store_Function store,
                                    LV2_State_Handle handle,
                                    const CadenceURIDs& urids,
                                    const ChordSlot* playback,
                                    int playback_segment_count,
                                    bool ready,
                                    const VariationStateBlob& variation) {
    if (!store || !playback) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    ProgressionStateBlob progression{};
    progression.version = CADENCE_PROGRESSION_STATE_VERSION;
    progression.segment_count = clampi(playback_segment_count, 0, CADENCE_MAX_SEGMENTS);
    progression.ready = ready ? 1 : 0;

    for (int i = 0; i < progression.segment_count; ++i) {
        const ChordSlot& slot = playback[i];
        SavedChordSlot& saved = progression.slots[i];
        saved.valid = slot.valid ? 1 : 0;
        saved.root_pc = slot.root_pc;
        saved.quality = slot.quality;
        saved.note_count = slot.note_count;
        saved.velocity = slot.velocity;
        for (uint8_t j = 0; j < slot.note_count && j < CADENCE_MAX_CHORD_NOTES; ++j) {
            saved.notes[j] = slot.notes[j];
        }
    }

    const uint32_t flags = LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE;
    store(handle,
          urids.state_progression,
          &progression,
          sizeof(progression),
          urids.atom_Chunk,
          flags);
    store(handle,
          urids.state_variation,
          &variation,
          sizeof(VariationStateBlob),
          urids.atom_Chunk,
          flags);

    return LV2_STATE_SUCCESS;
}

LV2_State_Status cadence_restore_state(LV2_State_Retrieve_Function retrieve,
                                       LV2_State_Handle handle,
                                       const CadenceURIDs& urids,
                                       ChordSlot* playback,
                                       int* playback_segment_count,
                                       bool* ready,
                                       VariationStateBlob* variation) {
    if (!retrieve || !playback || !playback_segment_count || !ready || !variation) {
        return LV2_STATE_ERR_UNKNOWN;
    }

    std::memset(playback, 0, sizeof(ChordSlot) * CADENCE_MAX_SEGMENTS);
    *playback_segment_count = 0;
    *ready = false;
    *variation = cadence_default_variation_state();

    size_t size = 0;
    uint32_t type = 0;
    uint32_t flags = 0;

    const void* progression_data = retrieve(handle, urids.state_progression, &size, &type, &flags);
    if (progression_data && size == sizeof(ProgressionStateBlob) && type == urids.atom_Chunk) {
        const ProgressionStateBlob* progression = (const ProgressionStateBlob*)progression_data;
        if (progression->version == CADENCE_PROGRESSION_STATE_VERSION) {
            *playback_segment_count = clampi(progression->segment_count, 0, CADENCE_MAX_SEGMENTS);
            *ready = progression->ready != 0 && *playback_segment_count > 0;

            for (int i = 0; i < *playback_segment_count; ++i) {
                const SavedChordSlot& saved = progression->slots[i];
                ChordSlot& slot = playback[i];
                slot.valid = saved.valid != 0;
                slot.root_pc = (uint8_t)clampi(saved.root_pc, 0, 11);
                slot.quality = (uint8_t)clampi(saved.quality, 0, QUALITY_MIN7);
                slot.note_count = (uint8_t)clampi(saved.note_count, 0, CADENCE_MAX_CHORD_NOTES);
                slot.velocity = (uint8_t)clampi(saved.velocity, 1, 127);
                for (uint8_t j = 0; j < slot.note_count; ++j) {
                    slot.notes[j] = (uint8_t)clampi(saved.notes[j], 0, 127);
                }
                sort_notes(slot.notes, slot.note_count);
            }
        }
    }

    const void* variation_data = retrieve(handle, urids.state_variation, &size, &type, &flags);
    if (variation_data && size == sizeof(VariationStateBlob) && type == urids.atom_Chunk) {
        const VariationStateBlob* restored = (const VariationStateBlob*)variation_data;
        if (restored->version == CADENCE_VARIATION_STATE_VERSION) {
            *variation = *restored;
        }
    }

    return LV2_STATE_SUCCESS;
}
