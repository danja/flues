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

typedef struct {
    const char* name;
    uint8_t intervals[12];
    uint8_t length;
} ScaleDef;

static const ScaleDef SCALE_DEFS[] = {
    {"Diatonic",         {0, 2, 4, 5, 7, 9, 11}, 7},
    {"Chromatic",        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, 12},
    {"Blues",            {0, 3, 5, 6, 7, 10}, 6},
    {"Minor Pentatonic", {0, 3, 5, 7, 10}, 5},
    {"Dorian",           {0, 2, 3, 5, 7, 9, 10}, 7},
    {"Phrygian",         {0, 1, 3, 5, 7, 8, 10}, 7},
    {"Harmonic Minor",   {0, 2, 3, 5, 7, 8, 11}, 7},
    {"Hijaz",            {0, 1, 4, 5, 7, 8, 11}, 7}
};

static const uint8_t SCALE_COUNT = (uint8_t)(sizeof(SCALE_DEFS) / sizeof(SCALE_DEFS[0]));

void state_init(GridSeqState* state, double sample_rate) {
    if (!state) return;

    memset(state, 0, sizeof(GridSeqState));
    state->pitch_offset = DEFAULT_PITCH_OFFSET;  // Start at C2 (MIDI note 36)
    state->scale_index = 0;
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

uint8_t state_scale_row_to_note(const GridSeqState* state, uint8_t row) {
    if (!state || SCALE_COUNT == 0) {
        return 0;
    }

    const uint8_t scale_index = state->scale_index % SCALE_COUNT;
    const ScaleDef* scale = &SCALE_DEFS[scale_index];
    const uint8_t degree = (scale->length > 0) ? (row % scale->length) : 0;
    const uint8_t octave = (scale->length > 0) ? (row / scale->length) : 0;
    int note = (int)state->pitch_offset + (int)scale->intervals[degree] + (12 * (int)octave);

    if (note < 0) {
        note = 0;
    } else if (note > 127) {
        note = 127;
    }

    return (uint8_t)note;
}

const char* state_scale_name(uint8_t index) {
    if (SCALE_COUNT == 0) {
        return "None";
    }
    return SCALE_DEFS[index % SCALE_COUNT].name;
}

uint8_t state_scale_count(void) {
    return SCALE_COUNT;
}
