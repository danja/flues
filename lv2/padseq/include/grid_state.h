#ifndef GRID_STATE_H
#define GRID_STATE_H

#include <stdint.h>
#include "launchpad_config.h"

/*
 * Grid State Management
 *
 * This module manages the complete state of the Launchpad grid,
 * including cell states, LED colors, and button states.
 */

// ============================================================================
// Cell State Flags
// ============================================================================

typedef enum {
    CELL_EMPTY    = 0x00,
    CELL_ARMED    = 0x01,  // Step is armed/active
    CELL_PLAYING  = 0x02,  // Currently playing
    CELL_MUTED    = 0x04,  // Muted
    CELL_SELECTED = 0x08   // Selected for editing
} CellState;

// ============================================================================
// RGB Color
// ============================================================================

typedef struct {
    uint8_t r;  // 0-127
    uint8_t g;  // 0-127
    uint8_t b;  // 0-127
} RGB;

// ============================================================================
// Grid Cell
// ============================================================================

typedef struct {
    uint8_t state;        // CellState flags
    uint8_t value;        // Note/param value (0-127)
    uint8_t color;        // Palette index (0-127)
    uint8_t brightness;   // LED brightness (0-127)
    uint8_t velocity;     // MIDI velocity if note cell
} GridCell;

// ============================================================================
// Button State
// ============================================================================

typedef struct {
    uint8_t pressed;      // 0 = released, 1 = pressed
    uint8_t color;        // Palette index
    uint8_t value;        // Associated value (mode, pattern, etc.)
} ButtonState;

// ============================================================================
// Launchpad State
// ============================================================================

typedef struct {
    // 8x8 grid cells
    GridCell grid[GRID_HEIGHT][GRID_WIDTH];

    // Side buttons (right column)
    ButtonState side[GRID_HEIGHT];

    // Top row buttons (including logo)
    ButtonState top[9];

    // Global state
    uint8_t mode;           // Current performance mode
    uint8_t tempo_bpm;      // BPM (60-240)
    uint8_t swing;          // Swing amount (0-100%)
    uint32_t step_counter;  // Current step in sequencer
    uint8_t pattern;        // Current pattern (0-7)
    uint8_t playing;        // Transport state

    // Quadrant states
    uint8_t drum_pattern[64];   // 4x16 step sequencer (4 rows × 16 steps)
    uint8_t melody_pattern[64]; // 4x16 step sequencer
    uint8_t live_states[16];    // 4x4 live pad states
    uint8_t param_values[16];   // 4x4 parameter values

} LaunchpadState;

// ============================================================================
// Function Declarations
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Initialize state to defaults
void grid_state_init(LaunchpadState *state);

// Clear all cells
void grid_state_clear(LaunchpadState *state);

// Cell access
void grid_state_set_cell(LaunchpadState *state, uint8_t row, uint8_t col,
                         uint8_t cell_state, uint8_t value, uint8_t color);

void grid_state_get_cell(const LaunchpadState *state, uint8_t row, uint8_t col,
                         GridCell *cell);

// Button access
void grid_state_set_side_button(LaunchpadState *state, uint8_t index,
                                uint8_t pressed, uint8_t color);

void grid_state_set_top_button(LaunchpadState *state, uint8_t index,
                               uint8_t pressed, uint8_t color);

// LED color helpers
void grid_state_set_led(LaunchpadState *state, uint8_t row, uint8_t col,
                        uint8_t color);

uint8_t grid_state_get_led(const LaunchpadState *state, uint8_t row, uint8_t col);

// Quadrant helpers
typedef enum {
    QUADRANT_DRUMS,   // Top-left (rows 4-7, cols 0-3)
    QUADRANT_MELODY,  // Top-right (rows 4-7, cols 4-7)
    QUADRANT_LIVE,    // Bottom-left (rows 0-3, cols 0-3)
    QUADRANT_PARAMS   // Bottom-right (rows 0-3, cols 4-7)
} Quadrant;

int grid_state_get_quadrant(uint8_t row, uint8_t col, Quadrant *quad,
                            uint8_t *local_row, uint8_t *local_col);

#ifdef __cplusplus
}
#endif

#endif // GRID_STATE_H
