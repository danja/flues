#ifndef PADSEQ_UI_STATE_H
#define PADSEQ_UI_STATE_H

#include <stdint.h>

#define PADSEQ_UI_STATE_MAGIC 0x51534453u  // 'PSDS'
#define PADSEQ_UI_STATE_VERSION 2
#define PADSEQ_UI_DELTA_MAGIC 0x5053444Cu  // 'PSDL'
#define PADSEQ_UI_DELTA_VERSION 1

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
} PadSeqUiState;

typedef enum {
    PADSEQ_UI_DELTA_GRID = 0,
    PADSEQ_UI_DELTA_SIDE = 1,
    PADSEQ_UI_DELTA_TOP  = 2
} PadSeqUiDeltaTarget;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t target;
    uint8_t index_a;
    uint8_t index_b;
    uint8_t color;
    uint8_t reserved[3];
} PadSeqUiDelta;

#endif // PADSEQ_UI_STATE_H
