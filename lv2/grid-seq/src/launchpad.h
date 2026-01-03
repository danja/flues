/*
 * grid-seq - Grid-based MIDI sequencer LV2 plugin
 *
 * Copyright (C) 2025 Danny
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE.
 */

#ifndef GRID_SEQ_LAUNCHPAD_H
#define GRID_SEQ_LAUNCHPAD_H

#include "grid_seq/common.h"
#include "state.h"
#include <stdbool.h>
#include <stdint.h>

// Launchpad Mini Mk3 Programmer Mode constants
#define LP_SYSEX_HEADER_SIZE 7
#define LP_SYSEX_ENTER_PROG 0x0E
#define LP_SYSEX_EXIT_PROG 0x0E

// Launchpad grid note mapping (Programmer mode)
// Grid is notes 11-88 (8x8 grid, skip rows ending in 9)
static inline uint8_t lp_grid_to_note(uint8_t x, uint8_t y) {
    return 11 + x + (y * 10);
}

static inline void lp_note_to_grid(uint8_t note, uint8_t* x, uint8_t* y) {
    uint8_t offset = note - 11;
    *x = offset % 10;
    *y = offset / 10;
}

// Top row CCs (91-98)
#define LP_TOP_CC_BASE 91

// Right column CCs (scene launch buttons)
static const uint8_t LP_SCENE_CCS[] = {89, 79, 69, 59, 49, 39, 29, 19};

// Color palette indices
#define LP_COLOR_OFF 0
#define LP_COLOR_WHITE 3
#define LP_COLOR_GREEN 21
#define LP_COLOR_GREEN_DIM 23
#define LP_COLOR_YELLOW 13
#define LP_COLOR_RED 5

typedef struct LaunchpadController LaunchpadController;

/**
 * Initialize Launchpad connection.
 *
 * @param card_num ALSA card number
 * @return Launchpad controller handle or NULL on error
 */
LaunchpadController* launchpad_init(int card_num);

/**
 * Cleanup and close Launchpad connection.
 */
void launchpad_cleanup(LaunchpadController* lp);

/**
 * Enter Programmer mode.
 */
bool launchpad_enter_programmer_mode(LaunchpadController* lp);

/**
 * Exit Programmer mode (back to standalone).
 */
bool launchpad_exit_programmer_mode(LaunchpadController* lp);

/**
 * Set LED color for a pad.
 *
 * @param note MIDI note number
 * @param color Color palette index
 */
bool launchpad_set_led(LaunchpadController* lp, uint8_t note, uint8_t color);

/**
 * Update LEDs to reflect current grid state.
 */
bool launchpad_update_grid(LaunchpadController* lp, const GridSeqState* state);

/**
 * Poll for button presses and update grid state.
 *
 * @return true if grid was modified
 */
bool launchpad_poll_input(LaunchpadController* lp, GridSeqState* state);

#endif // GRID_SEQ_LAUNCHPAD_H
