#include "../include/chord_map.h"
#include <stdio.h>

static const char *k_note_names[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

static const uint8_t k_scale_major[] = {0, 2, 4, 5, 7, 9, 11};
static const uint8_t k_scale_minor[] = {0, 2, 3, 5, 7, 8, 10};
static const uint8_t k_scale_dorian[] = {0, 2, 3, 5, 7, 9, 10};
static const uint8_t k_scale_mixolydian[] = {0, 2, 4, 5, 7, 9, 10};
static const uint8_t k_scale_harm_minor[] = {0, 2, 3, 5, 7, 8, 11};
static const uint8_t k_scale_blues[] = {0, 3, 5, 6, 7, 10};
static const uint8_t k_scale_chromatic[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

static void get_scale(uint8_t scale_index, const uint8_t **intervals, uint8_t *count) {
    switch (scale_index % ACHORD_SCALE_COUNT) {
        case ACHORD_SCALE_NAT_MINOR:
            *intervals = k_scale_minor;
            *count = (uint8_t)(sizeof(k_scale_minor) / sizeof(k_scale_minor[0]));
            break;
        case ACHORD_SCALE_DORIAN:
            *intervals = k_scale_dorian;
            *count = (uint8_t)(sizeof(k_scale_dorian) / sizeof(k_scale_dorian[0]));
            break;
        case ACHORD_SCALE_MIXOLYDIAN:
            *intervals = k_scale_mixolydian;
            *count = (uint8_t)(sizeof(k_scale_mixolydian) / sizeof(k_scale_mixolydian[0]));
            break;
        case ACHORD_SCALE_HARM_MINOR:
            *intervals = k_scale_harm_minor;
            *count = (uint8_t)(sizeof(k_scale_harm_minor) / sizeof(k_scale_harm_minor[0]));
            break;
        case ACHORD_SCALE_BLUES:
            *intervals = k_scale_blues;
            *count = (uint8_t)(sizeof(k_scale_blues) / sizeof(k_scale_blues[0]));
            break;
        case ACHORD_SCALE_CHROMATIC:
            *intervals = k_scale_chromatic;
            *count = (uint8_t)(sizeof(k_scale_chromatic) / sizeof(k_scale_chromatic[0]));
            break;
        case ACHORD_SCALE_MAJOR:
        default:
            *intervals = k_scale_major;
            *count = (uint8_t)(sizeof(k_scale_major) / sizeof(k_scale_major[0]));
            break;
    }
}

static int wrap12(int v) {
    int out = v % 12;
    if (out < 0) out += 12;
    return out;
}

static int nearest_pc_delta(int from_pc, int to_pc) {
    int delta = wrap12(to_pc - from_pc);
    if (delta > 6) delta -= 12;
    return delta;
}

const char *achord_scale_name(uint8_t index) {
    static const char *names[ACHORD_SCALE_COUNT] = {
        "Major", "Natural Minor", "Dorian", "Mixolydian",
        "Harmonic Minor", "Blues", "Chromatic"
    };
    return names[index % ACHORD_SCALE_COUNT];
}

const char *achord_trigger_name(uint8_t index) {
    static const char *names[5] = {
        "Direct", "Quantized 1/16", "Strum Down", "Strum Up", "Repeat 1/8"
    };
    return names[index % 5];
}

const char *achord_hold_name(uint8_t index) {
    static const char *names[3] = {
        "Momentary", "Latch", "Stack Latch"
    };
    return names[index % 3];
}

const char *achord_register_name(uint8_t index) {
    static const char *names[4] = {
        "8'", "16'+8'", "8'+4'", "16'+8'+4'"
    };
    return names[index % 4];
}

const char *achord_spread_name(uint8_t index) {
    static const char *names[3] = {
        "Close", "Open", "Drop-2"
    };
    return names[index % 3];
}

const char *achord_sus_name(uint8_t index) {
    static const char *names[3] = {
        "Off", "Sus2", "Sus4"
    };
    return names[index % 3];
}

const char *achord_row_name(uint8_t row) {
    static const char *names[8] = {
        "Bass Shell", "Counterbass", "Major", "Minor",
        "Dom7", "Diminished", "Maj7", "Min7"
    };
    return names[row & 7];
}

void achord_note_name(uint8_t midi_note, char *out, size_t out_size) {
    const uint8_t pc = (uint8_t)(midi_note % 12);
    const int octave = (int)(midi_note / 12) - 1;
    snprintf(out, out_size, "%s%d", k_note_names[pc], octave);
}

uint8_t achord_column_root_pc(uint8_t tonic_note, int bank_offset, uint8_t col) {
    const int tonic_pc = tonic_note % 12;
    const int fifth_steps = (int)col - 3 + bank_offset;
    return (uint8_t)wrap12(tonic_pc + (fifth_steps * 7));
}

uint8_t achord_column_root_midi(uint8_t tonic_note, int bank_offset, uint8_t col) {
    const int tonic_pc = tonic_note % 12;
    const int target_pc = achord_column_root_pc(tonic_note, bank_offset, col);
    const int delta = nearest_pc_delta(tonic_pc, target_pc);
    int midi = (int)tonic_note + delta;
    while (midi < 24) midi += 12;
    while (midi > 96) midi -= 12;
    return (uint8_t)midi;
}

void achord_root_name_for_column(uint8_t tonic_note, int bank_offset, uint8_t col,
                                 char *out, size_t out_size) {
    const uint8_t pc = achord_column_root_pc(tonic_note, bank_offset, col);
    snprintf(out, out_size, "%s", k_note_names[pc]);
}

uint8_t achord_scale_degree_interval(uint8_t scale_index, uint8_t degree) {
    const uint8_t *scale = NULL;
    uint8_t count = 0;
    if (degree < 1) degree = 1;
    get_scale(scale_index, &scale, &count);
    if (!scale || count == 0) {
        return 0;
    }
    if (degree == 1) {
        return 0;
    }
    const uint8_t index = (uint8_t)((degree - 1) % count);
    uint8_t interval = scale[index];
    if (degree > count) {
        interval = (uint8_t)(interval + 12);
    }
    return interval;
}

int achord_scale_contains_pc(uint8_t scale_index, uint8_t tonic_pc, uint8_t note_pc) {
    const uint8_t *scale = NULL;
    uint8_t count = 0;
    get_scale(scale_index, &scale, &count);
    const int rel = wrap12((int)note_pc - (int)tonic_pc);
    for (uint8_t i = 0; i < count; ++i) {
        if (scale[i] == rel) return 1;
    }
    return 0;
}

int achord_scale_prefers_minor_third(uint8_t scale_index) {
    return scale_index == ACHORD_SCALE_NAT_MINOR ||
           scale_index == ACHORD_SCALE_DORIAN ||
           scale_index == ACHORD_SCALE_HARM_MINOR ||
           scale_index == ACHORD_SCALE_BLUES;
}

int achord_scale_prefers_half_diminished(uint8_t scale_index) {
    return scale_index == ACHORD_SCALE_NAT_MINOR ||
           scale_index == ACHORD_SCALE_DORIAN ||
           scale_index == ACHORD_SCALE_MIXOLYDIAN;
}
