#ifndef ACHORD_UI_STATE_H
#define ACHORD_UI_STATE_H

#include <stdint.h>

#define ACHORD_UI_STATE_MAGIC 0x41434844u
#define ACHORD_UI_STATE_VERSION 1

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t grid[8][8];
    uint8_t side[8];
    uint8_t top[9];
    uint8_t tonic_note;
    int8_t bank_offset;
    uint8_t scale_index;
    uint8_t register_mode;
    uint8_t trigger_mode;
    uint8_t hold_mode;
    uint8_t bass_enabled;
    uint8_t add9_enabled;
    uint8_t sus_mode;
    int8_t inversion_offset;
    uint8_t spread_mode;
    uint8_t accent_enabled;
    uint8_t voice_lead_enabled;
    uint16_t bpm;
    uint8_t active_chord_count;
    uint8_t current_step16;
    uint8_t host_playing;
    uint8_t clock_source;
    uint8_t reserved[3];
} AchordUiState;

#endif
