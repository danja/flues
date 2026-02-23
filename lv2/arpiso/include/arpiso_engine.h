#ifndef ARPISO_ENGINE_H
#define ARPISO_ENGINE_H

#include <stdint.h>
#include "grid_state.h"
#include "midi_comm.h"

/*
 * ArpIso Engine
 *
 * Euclidean gravity arpeggiator for Launchpad Mini MK3.
 * 8x8 grid is performance area; top row + right column are controls.
 */

#define MAX_WELLS 5
#define MAX_PLAYHEADS 5
#define MAX_MIDI_EVENTS 128
#define MAX_PENDING_NOTEOFFS 64
#define STEP_COUNT 64

typedef struct {
    uint32_t frame;
    uint8_t size;
    uint8_t data[3];
} MidiOutEvent;

typedef struct {
    uint8_t active;
    uint8_t note;
    uint8_t status;
    uint32_t frames_left;
} PendingNoteOff;

typedef struct {
    uint8_t active;
    uint8_t row;
    uint8_t col;
    uint8_t velocity;
    uint8_t note;
    uint8_t pulses;
    uint8_t offset;
} ArpIsoWell;

typedef struct {
    uint8_t active;
    uint8_t source_well;
    uint8_t target_well;
    uint32_t phase_samples;
    uint32_t travel_samples;
    uint8_t euclid_step;
    uint8_t latched_note;
} ArpIsoPlayhead;

typedef struct {
    uint8_t density_bias;
    uint8_t phase_bias;
    uint8_t gravity_strength;
    uint8_t travel_scale;
    uint8_t gate_percent;
    uint8_t gate_step_index;
    uint8_t velocity_curve;
    uint8_t humanize;
    uint8_t root_note;
    uint8_t scale_index;
    uint8_t motion_mode;
    uint8_t clock_division_index;
    uint8_t cycle_length_index;
} ArpIsoPatternState;

typedef struct {
    LaunchpadState grid_state;

    ArpIsoWell wells[MAX_WELLS];
    ArpIsoPlayhead playheads[MAX_PLAYHEADS];
    uint8_t held_count;
    uint8_t rejected_press_flash;

    uint8_t playing;
    uint16_t current_step;
    uint32_t sample_counter;
    uint32_t samples_per_tick;

    float sample_rate;

    uint8_t density_bias;
    uint8_t phase_bias;
    uint8_t gravity_strength;
    uint8_t travel_scale;
    uint8_t gate_percent;
    uint8_t gate_step_index;
    uint8_t velocity_curve;
    uint8_t humanize;
    uint8_t root_note;
    uint8_t scale_index;
    uint8_t motion_mode;
    uint8_t clock_division_index;
    uint8_t cycle_length_index;
    uint8_t pattern_slot;
    uint8_t gm_drum_mode;
    uint8_t hold_latch_mode;

    ArpIsoPatternState patterns[2];

    MidiOutEvent midi_events[MAX_MIDI_EVENTS];
    uint16_t midi_event_count;
    PendingNoteOff pending_note_offs[MAX_PENDING_NOTEOFFS];
} ArpIsoEngine;

// ============================================================================
// Function Declarations
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

void arpiso_init(ArpIsoEngine *engine, float sample_rate);
void arpiso_reset(ArpIsoEngine *engine);

void arpiso_handle_pad_press(ArpIsoEngine *engine, uint8_t row, uint8_t col, uint8_t velocity);
void arpiso_handle_pad_release(ArpIsoEngine *engine, uint8_t row, uint8_t col);
void arpiso_handle_side_button(ArpIsoEngine *engine, uint8_t index);
void arpiso_handle_top_button(ArpIsoEngine *engine, uint8_t index);

void arpiso_start(ArpIsoEngine *engine);
void arpiso_stop(ArpIsoEngine *engine);
void arpiso_set_tempo(ArpIsoEngine *engine, uint16_t bpm);

void arpiso_process(ArpIsoEngine *engine, uint32_t n_samples);
void arpiso_begin_block(ArpIsoEngine *engine);
void arpiso_refresh_grid_state(ArpIsoEngine *engine);
uint32_t arpiso_default_gate_frames(const ArpIsoEngine *engine);

void arpiso_set_pattern(ArpIsoEngine *engine, uint8_t pattern);
void arpiso_clear_pattern(ArpIsoEngine *engine);

#ifdef __cplusplus
}
#endif

#endif // ARPISO_ENGINE_H
