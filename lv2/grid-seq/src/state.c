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

#include "state.h"
#include <string.h>

void state_init(GridSeqState* state, double sample_rate) {
    if (!state) return;

    memset(state, 0, sizeof(GridSeqState));
    state->pitch_offset = DEFAULT_PITCH_OFFSET;  // Start at C2 (MIDI note 36)
    state->beats_per_bar = 4.0;
    state->sample_rate = sample_rate;
    state->current_step = 0;
    state->previous_step = 0;
    state->sequence_length = DEFAULT_SEQUENCE_LENGTH;
    state->hardware_page = 0;
    state->playing = false;
    state->frame_counter = 0;

    // Default to 120 BPM, 8 steps per 2 bars
    // 2 bars at 4/4 = 8 beats, 8 steps = 1 beat per step
    state_update_tempo(state, 120.0);
}

void state_toggle_step(GridSeqState* state, uint8_t x, uint8_t y) {
    if (!state || x >= MAX_GRID_SIZE || y >= GRID_PITCH_RANGE) return;

    state->grid[x][y] = !state->grid[x][y];
}

void state_update_tempo(GridSeqState* state, double bpm) {
    if (!state || bpm <= 0.0) return;

    // 1 beat per step, calculate frames per step
    double beats_per_second = bpm / 60.0;
    double seconds_per_beat = 1.0 / beats_per_second;
    state->frames_per_step = (uint64_t)(seconds_per_beat * state->sample_rate);
}
