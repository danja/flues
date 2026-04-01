#ifndef GRID_STATE_H
#define GRID_STATE_H

#include <stdint.h>
#include "launchpad_config.h"

typedef enum {
    CELL_EMPTY    = 0x00,
    CELL_ARMED    = 0x01,
    CELL_PLAYING  = 0x02,
    CELL_MUTED    = 0x04,
    CELL_SELECTED = 0x08
} CellState;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} RGB;

typedef struct {
    uint8_t state;
    uint8_t value;
    uint8_t color;
    uint8_t brightness;
    uint8_t velocity;
} GridCell;

typedef struct {
    uint8_t pressed;
    uint8_t color;
    uint8_t value;
} ButtonState;

typedef struct {
    GridCell grid[GRID_HEIGHT][GRID_WIDTH];
    ButtonState side[GRID_HEIGHT];
    ButtonState top[9];
    uint8_t mode;
    uint8_t tempo_bpm;
    uint8_t swing;
    uint32_t step_counter;
    uint8_t pattern;
    uint8_t playing;
    uint8_t drum_pattern[64];
    uint8_t melody_pattern[64];
    uint8_t live_states[16];
    uint8_t param_values[16];
} LaunchpadState;

#ifdef __cplusplus
extern "C" {
#endif

void grid_state_init(LaunchpadState *state);
void grid_state_clear(LaunchpadState *state);
void grid_state_set_cell(LaunchpadState *state, uint8_t row, uint8_t col,
                         uint8_t cell_state, uint8_t value, uint8_t color);
void grid_state_get_cell(const LaunchpadState *state, uint8_t row, uint8_t col,
                         GridCell *cell);
void grid_state_set_side_button(LaunchpadState *state, uint8_t index,
                                uint8_t pressed, uint8_t color);
void grid_state_set_top_button(LaunchpadState *state, uint8_t index,
                               uint8_t pressed, uint8_t color);
void grid_state_set_led(LaunchpadState *state, uint8_t row, uint8_t col,
                        uint8_t color);
uint8_t grid_state_get_led(const LaunchpadState *state, uint8_t row, uint8_t col);

#ifdef __cplusplus
}
#endif

#endif
