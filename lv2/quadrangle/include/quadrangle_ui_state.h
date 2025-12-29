#ifndef QUADRANGLE_UI_STATE_H
#define QUADRANGLE_UI_STATE_H

#include <stdint.h>

#define QUADRANGLE_UI_STATE_MAGIC 0x51554453u  // 'QUDS'
#define QUADRANGLE_UI_STATE_VERSION 2
#define QUADRANGLE_UI_DELTA_MAGIC 0x5155444Cu  // 'QUDL'
#define QUADRANGLE_UI_DELTA_VERSION 1

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
    uint8_t melody_program;
    uint8_t reserved[2];
} QuadrangleUiState;

typedef enum {
    QUADRANGLE_UI_DELTA_GRID = 0,
    QUADRANGLE_UI_DELTA_SIDE = 1,
    QUADRANGLE_UI_DELTA_TOP  = 2
} QuadrangleUiDeltaTarget;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t target;
    uint8_t index_a;
    uint8_t index_b;
    uint8_t color;
    uint8_t reserved[3];
} QuadrangleUiDelta;

#endif // QUADRANGLE_UI_STATE_H
