#include "../include/grid_state.h"
#include <string.h>

// ============================================================================
// Initialization
// ============================================================================

void grid_state_init(LaunchpadState *state) {
    memset(state, 0, sizeof(LaunchpadState));

    // Set default tempo and swing
    state->tempo_bpm = 120;
    state->swing = 50;  // 50% = no swing
    state->mode = 0;
    state->pattern = 0;
    state->playing = 0;
    state->step_counter = 0;

    // Initialize all grid cells to off
    grid_state_clear(state);
}

void grid_state_clear(LaunchpadState *state) {
    // Clear all grid cells
    for (uint8_t r = 0; r < GRID_HEIGHT; r++) {
        for (uint8_t c = 0; c < GRID_WIDTH; c++) {
            state->grid[r][c].state = CELL_EMPTY;
            state->grid[r][c].value = 0;
            state->grid[r][c].color = COLOR_OFF;
            state->grid[r][c].brightness = 0;
            state->grid[r][c].velocity = 96;  // Default velocity
        }
    }

    // Clear all buttons
    for (uint8_t i = 0; i < GRID_HEIGHT; i++) {
        state->side[i].pressed = 0;
        state->side[i].color = COLOR_OFF;
        state->side[i].value = 0;
    }

    for (uint8_t i = 0; i < 9; i++) {
        state->top[i].pressed = 0;
        state->top[i].color = COLOR_OFF;
        state->top[i].value = 0;
    }

    // Clear pattern data
    memset(state->drum_pattern, 0, sizeof(state->drum_pattern));
    memset(state->melody_pattern, 0, sizeof(state->melody_pattern));
    memset(state->live_states, 0, sizeof(state->live_states));
    memset(state->param_values, 64, sizeof(state->param_values));  // Center values
}

// ============================================================================
// Cell Access
// ============================================================================

void grid_state_set_cell(LaunchpadState *state, uint8_t row, uint8_t col,
                         uint8_t cell_state, uint8_t value, uint8_t color) {
    if (row >= GRID_HEIGHT || col >= GRID_WIDTH) return;

    state->grid[row][col].state = cell_state;
    state->grid[row][col].value = value;
    state->grid[row][col].color = color;
}

void grid_state_get_cell(const LaunchpadState *state, uint8_t row, uint8_t col,
                         GridCell *cell) {
    if (row >= GRID_HEIGHT || col >= GRID_WIDTH) {
        memset(cell, 0, sizeof(GridCell));
        return;
    }

    *cell = state->grid[row][col];
}

// ============================================================================
// Button Access
// ============================================================================

void grid_state_set_side_button(LaunchpadState *state, uint8_t index,
                                uint8_t pressed, uint8_t color) {
    if (index >= GRID_HEIGHT) return;

    state->side[index].pressed = pressed;
    state->side[index].color = color;
}

void grid_state_set_top_button(LaunchpadState *state, uint8_t index,
                               uint8_t pressed, uint8_t color) {
    if (index >= 9) return;

    state->top[index].pressed = pressed;
    state->top[index].color = color;
}

// ============================================================================
// LED Color Helpers
// ============================================================================

void grid_state_set_led(LaunchpadState *state, uint8_t row, uint8_t col,
                        uint8_t color) {
    if (row >= GRID_HEIGHT || col >= GRID_WIDTH) return;

    state->grid[row][col].color = color;
}

uint8_t grid_state_get_led(const LaunchpadState *state, uint8_t row, uint8_t col) {
    if (row >= GRID_HEIGHT || col >= GRID_WIDTH) return COLOR_OFF;

    return state->grid[row][col].color;
}

// ============================================================================
// Quadrant Helpers
// ============================================================================

int grid_state_get_quadrant(uint8_t row, uint8_t col, Quadrant *quad,
                            uint8_t *local_row, uint8_t *local_col) {
    if (row >= GRID_HEIGHT || col >= GRID_WIDTH) return 0;

    // Determine quadrant based on position
    if (row >= 4) {
        // Top half
        if (col < 4) {
            *quad = QUADRANT_DRUMS;
            *local_row = row - 4;
            *local_col = col;
        } else {
            *quad = QUADRANT_MELODY;
            *local_row = row - 4;
            *local_col = col - 4;
        }
    } else {
        // Bottom half
        if (col < 4) {
            *quad = QUADRANT_LIVE;
            *local_row = row;
            *local_col = col;
        } else {
            *quad = QUADRANT_PARAMS;
            *local_row = row;
            *local_col = col - 4;
        }
    }

    return 1;
}
