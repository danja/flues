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

#ifndef GRID_SEQ_STATE_H
#define GRID_SEQ_STATE_H

#include "grid_seq/common.h"

typedef struct {
    bool grid[MAX_GRID_SIZE][GRID_PITCH_RANGE];  // Full MIDI range 0-127
    uint8_t pitch_offset;       // Base MIDI note for current 8-row view (0-120)
    uint8_t current_step;
    uint8_t previous_step;
    uint8_t sequence_length;    // 2-16 steps
    uint8_t hardware_page;      // 0 or 1 for Launchpad paging
    double beats_per_bar;
    double sample_rate;
    bool playing;
    bool first_run;
    uint64_t frame_counter;
    uint64_t frames_per_step;
    bool active_notes[128];  // Track which notes are currently on
} GridSeqState;

/**
 * Initialize the sequencer state.
 *
 * @param state Pointer to state structure
 * @param sample_rate Host sample rate in Hz
 */
void state_init(GridSeqState* state, double sample_rate);

/**
 * Toggle a step in the grid.
 *
 * @param state Pointer to state structure
 * @param x Grid X coordinate (0-7)
 * @param y Grid Y coordinate (0-7)
 */
void state_toggle_step(GridSeqState* state, uint8_t x, uint8_t y);

/**
 * Update timing based on BPM.
 *
 * @param state Pointer to state structure
 * @param bpm Beats per minute
 */
void state_update_tempo(GridSeqState* state, double bpm);

#endif // GRID_SEQ_STATE_H
