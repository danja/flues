#ifndef ACHORD_CHORD_MAP_H
#define ACHORD_CHORD_MAP_H

#include <stddef.h>
#include <stdint.h>

#define ACHORD_SCALE_COUNT 7

typedef enum {
    ACHORD_SCALE_MAJOR = 0,
    ACHORD_SCALE_NAT_MINOR = 1,
    ACHORD_SCALE_DORIAN = 2,
    ACHORD_SCALE_MIXOLYDIAN = 3,
    ACHORD_SCALE_HARM_MINOR = 4,
    ACHORD_SCALE_BLUES = 5,
    ACHORD_SCALE_CHROMATIC = 6
} AchordScale;

typedef enum {
    ACHORD_REGISTER_8 = 0,
    ACHORD_REGISTER_16_8 = 1,
    ACHORD_REGISTER_8_4 = 2,
    ACHORD_REGISTER_16_8_4 = 3
} AchordRegisterMode;

typedef enum {
    ACHORD_TRIGGER_DIRECT = 0,
    ACHORD_TRIGGER_QUANTIZED = 1,
    ACHORD_TRIGGER_STRUM_DOWN = 2,
    ACHORD_TRIGGER_STRUM_UP = 3,
    ACHORD_TRIGGER_REPEAT = 4
} AchordTriggerMode;

typedef enum {
    ACHORD_HOLD_MOMENTARY = 0,
    ACHORD_HOLD_LATCH = 1,
    ACHORD_HOLD_STACK = 2
} AchordHoldMode;

typedef enum {
    ACHORD_SUS_OFF = 0,
    ACHORD_SUS_2 = 1,
    ACHORD_SUS_4 = 2
} AchordSusMode;

typedef enum {
    ACHORD_SPREAD_CLOSE = 0,
    ACHORD_SPREAD_OPEN = 1,
    ACHORD_SPREAD_DROP2 = 2
} AchordSpreadMode;

#ifdef __cplusplus
extern "C" {
#endif

const char *achord_scale_name(uint8_t index);
const char *achord_trigger_name(uint8_t index);
const char *achord_hold_name(uint8_t index);
const char *achord_register_name(uint8_t index);
const char *achord_spread_name(uint8_t index);
const char *achord_sus_name(uint8_t index);
const char *achord_row_name(uint8_t row);

void achord_note_name(uint8_t midi_note, char *out, size_t out_size);
void achord_root_name_for_column(uint8_t tonic_note, int bank_offset, uint8_t col,
                                 char *out, size_t out_size);

uint8_t achord_scale_degree_interval(uint8_t scale_index, uint8_t degree);
int achord_scale_contains_pc(uint8_t scale_index, uint8_t tonic_pc, uint8_t note_pc);
int achord_scale_prefers_minor_third(uint8_t scale_index);
int achord_scale_prefers_half_diminished(uint8_t scale_index);
uint8_t achord_column_root_midi(uint8_t tonic_note, int bank_offset, uint8_t col);
uint8_t achord_column_root_pc(uint8_t tonic_note, int bank_offset, uint8_t col);

#ifdef __cplusplus
}
#endif

#endif
