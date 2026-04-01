#include "../include/voicing_engine.h"
#include "../include/chord_map.h"
#include <stdlib.h>
#include <string.h>

static int cmp_u8(const void *a, const void *b) {
    return (int)(*(const uint8_t *)a) - (int)(*(const uint8_t *)b);
}

static void sort_notes(uint8_t *notes, uint8_t count) {
    qsort(notes, count, sizeof(uint8_t), cmp_u8);
}

static void uniq_notes(uint8_t *notes, uint8_t *count) {
    if (*count == 0) return;
    sort_notes(notes, *count);
    uint8_t w = 1;
    for (uint8_t i = 1; i < *count; ++i) {
        if (notes[i] != notes[w - 1]) {
            notes[w++] = notes[i];
        }
    }
    *count = w;
}

static void add_note(uint8_t *notes, uint8_t *count, int note) {
    if (*count >= ACHORD_MAX_CHORD_NOTES) return;
    if (note < 0) note = 0;
    if (note > 127) note = 127;
    notes[(*count)++] = (uint8_t)note;
}

static void build_formula(const AchordVoicingRequest *request,
                          uint8_t *intervals, uint8_t *count) {
    *count = 0;
    const int use_minor_third = achord_scale_prefers_minor_third(request->scale_index);
    const int half_dim = achord_scale_prefers_half_diminished(request->scale_index);

    switch (request->row & 7) {
        case 0:
            intervals[(*count)++] = 0;
            intervals[(*count)++] = 7;
            intervals[(*count)++] = 12;
            break;
        case 1:
            intervals[(*count)++] = (uint8_t)(use_minor_third ? 3 : 4);
            intervals[(*count)++] = 7;
            intervals[(*count)++] = 12;
            break;
        case 2:
            intervals[(*count)++] = 0;
            intervals[(*count)++] = 4;
            intervals[(*count)++] = 7;
            break;
        case 3:
            intervals[(*count)++] = 0;
            intervals[(*count)++] = 3;
            intervals[(*count)++] = 7;
            break;
        case 4:
            intervals[(*count)++] = 0;
            intervals[(*count)++] = 4;
            intervals[(*count)++] = 7;
            intervals[(*count)++] = 10;
            break;
        case 5:
            intervals[(*count)++] = 0;
            intervals[(*count)++] = 3;
            intervals[(*count)++] = 6;
            intervals[(*count)++] = (uint8_t)(half_dim ? 10 : 9);
            break;
        case 6:
            intervals[(*count)++] = 0;
            intervals[(*count)++] = 4;
            intervals[(*count)++] = 7;
            intervals[(*count)++] = 11;
            break;
        case 7:
        default:
            intervals[(*count)++] = 0;
            intervals[(*count)++] = 3;
            intervals[(*count)++] = 7;
            intervals[(*count)++] = 10;
            break;
    }
}

static void apply_sus(const AchordVoicingRequest *request,
                      uint8_t *intervals, uint8_t count) {
    if (request->sus_mode == ACHORD_SUS_OFF) return;
    if (count < 2) return;

    uint8_t replacement = 0;
    if (request->sus_mode == ACHORD_SUS_2) {
        replacement = achord_scale_degree_interval(request->scale_index, 2);
    } else {
        replacement = achord_scale_degree_interval(request->scale_index, 4);
    }

    for (uint8_t i = 1; i < count; ++i) {
        if (intervals[i] == 3 || intervals[i] == 4) {
            intervals[i] = replacement;
            break;
        }
    }
}

static void apply_inversion(uint8_t *notes, uint8_t count, int8_t inversion_offset) {
    if (count == 0 || inversion_offset == 0) return;
    sort_notes(notes, count);
    if (inversion_offset > 0) {
        for (int i = 0; i < inversion_offset; ++i) {
            notes[0] = (uint8_t)(notes[0] + 12);
            sort_notes(notes, count);
        }
    } else {
        for (int i = 0; i < -inversion_offset; ++i) {
            notes[count - 1] = (uint8_t)(notes[count - 1] - 12);
            sort_notes(notes, count);
        }
    }
}

static void apply_spread(uint8_t *notes, uint8_t count, uint8_t spread_mode) {
    if (count < 3) return;
    sort_notes(notes, count);
    switch (spread_mode) {
        case ACHORD_SPREAD_OPEN:
            notes[1] = (uint8_t)(notes[1] + 12);
            break;
        case ACHORD_SPREAD_DROP2:
            if (count >= 4) {
                notes[count - 2] = (uint8_t)(notes[count - 2] - 12);
            } else {
                notes[count - 1] = (uint8_t)(notes[count - 1] - 12);
            }
            break;
        case ACHORD_SPREAD_CLOSE:
        default:
            return;
    }
    sort_notes(notes, count);
}

static void apply_voice_lead(uint8_t *notes, uint8_t count,
                             const uint8_t *previous_notes, uint8_t previous_count) {
    if (count == 0 || previous_count == 0 || !previous_notes) return;
    sort_notes(notes, count);

    uint8_t prev[ACHORD_MAX_CHORD_NOTES];
    if (previous_count > ACHORD_MAX_CHORD_NOTES) previous_count = ACHORD_MAX_CHORD_NOTES;
    memcpy(prev, previous_notes, previous_count);
    sort_notes(prev, previous_count);

    int floor_note = -128;
    for (uint8_t i = 0; i < count; ++i) {
        const int target_index = (i * previous_count) / count;
        const int target = prev[target_index];
        int best = notes[i];
        int best_dist = 999;
        for (int oct = -2; oct <= 2; ++oct) {
            int cand = (int)notes[i] + oct * 12;
            while (cand <= floor_note) cand += 12;
            const int dist = abs(cand - target);
            if (dist < best_dist) {
                best = cand;
                best_dist = dist;
            }
        }
        if (best < 0) best = 0;
        if (best > 127) best = 127;
        notes[i] = (uint8_t)best;
        floor_note = best;
    }
    sort_notes(notes, count);
}

static void apply_register(const AchordVoicingRequest *request,
                           uint8_t *notes, uint8_t *count) {
    uint8_t base[ACHORD_MAX_CHORD_NOTES];
    const uint8_t base_count = *count;
    memcpy(base, notes, base_count);

    if (request->register_mode == ACHORD_REGISTER_16_8 ||
        request->register_mode == ACHORD_REGISTER_16_8_4) {
        for (uint8_t i = 0; i < base_count; ++i) {
            add_note(notes, count, (int)base[i] - 12);
        }
    }
    if (request->register_mode == ACHORD_REGISTER_8_4 ||
        request->register_mode == ACHORD_REGISTER_16_8_4) {
        for (uint8_t i = 0; i < base_count; ++i) {
            add_note(notes, count, (int)base[i] + 12);
        }
    }
}

void achord_build_voicing(const AchordVoicingRequest *request,
                          const uint8_t *previous_notes,
                          uint8_t previous_count,
                          AchordVoicing *out_voicing) {
    uint8_t intervals[8];
    uint8_t interval_count = 0;
    uint8_t notes[ACHORD_MAX_CHORD_NOTES];
    uint8_t count = 0;

    memset(out_voicing, 0, sizeof(*out_voicing));

    build_formula(request, intervals, &interval_count);
    apply_sus(request, intervals, interval_count);

    const uint8_t root = achord_column_root_midi(request->tonic_note,
                                                 request->bank_offset,
                                                 request->col);

    for (uint8_t i = 0; i < interval_count; ++i) {
        add_note(notes, &count, (int)root + intervals[i]);
    }

    if (request->add9_enabled) {
        add_note(notes, &count, (int)root + achord_scale_degree_interval(request->scale_index, 9));
    }

    uniq_notes(notes, &count);
    apply_inversion(notes, count, request->inversion_offset);
    apply_spread(notes, count, request->spread_mode);
    if (request->voice_lead_enabled) {
        apply_voice_lead(notes, count, previous_notes, previous_count);
    }

    if (request->bass_enabled) {
        add_note(notes, &count, (int)root - 12);
    }

    apply_register(request, notes, &count);
    uniq_notes(notes, &count);

    out_voicing->note_count = count;
    memcpy(out_voicing->notes, notes, count);
}
