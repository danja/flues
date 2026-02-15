#ifndef ARPISO_UI_STATE_H
#define ARPISO_UI_STATE_H

#include <stdint.h>

#define ARPISO_UI_STATE_MAGIC 0x51534453u  // 'PSDS'
#define ARPISO_UI_STATE_VERSION 2
#define ARPISO_UI_DELTA_MAGIC 0x5053444Cu  // 'PSDL'
#define ARPISO_UI_DELTA_VERSION 1

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t grid[8][8];
    uint8_t side[8];
    uint8_t top[9];
    uint8_t selected_voice;
    uint8_t pattern;
    uint8_t playing;
    uint8_t current_step;
    uint8_t euclid_pulses;
    uint8_t euclid_offset;
    uint8_t reserved[1];
} ArpIsoUiState;

typedef enum {
    ARPISO_UI_DELTA_GRID = 0,
    ARPISO_UI_DELTA_SIDE = 1,
    ARPISO_UI_DELTA_TOP  = 2
} ArpIsoUiDeltaTarget;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t target;
    uint8_t index_a;
    uint8_t index_b;
    uint8_t color;
    uint8_t reserved[3];
} ArpIsoUiDelta;

#endif // ARPISO_UI_STATE_H
