#ifndef PADSEQ_ENGINE_H
#define PADSEQ_ENGINE_H

#include <stdint.h>
#include "grid_state.h"
#include "midi_comm.h"

/*
 * PadSeq Engine
 *
 * 8-voice drum sequencer with a 64-step grid.
 * The full 8x8 Launchpad grid maps to steps 0-63 for the selected voice.
 */

// ============================================================================
// Engine Configuration
// ============================================================================

#define MAX_DRUM_VOICES 8
#define SEQ_STEPS 64
#define MAX_MIDI_EVENTS 128
#define DEFAULT_GATE_MS 50

// ============================================================================
// Drum Voice
// ============================================================================

typedef struct {
    uint8_t active;           // Is this voice active?
    uint8_t note;             // MIDI note to trigger
    uint8_t steps[SEQ_STEPS]; // Step velocities (0 = off, 1-127 = velocity)
    uint8_t color;            // LED color for this voice
} DrumVoice;

// ============================================================================
// MIDI Event Queue (host-facing)
// ============================================================================

typedef struct {
    uint32_t frame;      // Sample offset within current block
    uint8_t size;        // Usually 3 bytes
    uint8_t data[3];     // MIDI message
} MidiOutEvent;

// ============================================================================
// PadSeq Engine
// ============================================================================

typedef struct {
    LaunchpadState grid_state;

    DrumVoice drum_voices[MAX_DRUM_VOICES];
    uint8_t drum_patterns[2][MAX_DRUM_VOICES][SEQ_STEPS];

    uint8_t playing;
    uint32_t sample_counter;
    uint32_t samples_per_step;
    uint16_t current_step;
    uint8_t selected_voice;
    uint8_t active_columns;
    uint8_t euclid_pulses[2][MAX_DRUM_VOICES];
    uint8_t euclid_offset[2][MAX_DRUM_VOICES];

    float sample_rate;

    MidiOutEvent midi_events[MAX_MIDI_EVENTS];
    uint16_t midi_event_count;
} PadSeqEngine;

// ============================================================================
// Function Declarations
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

void padseq_init(PadSeqEngine *engine, float sample_rate);
void padseq_reset(PadSeqEngine *engine);

void padseq_handle_pad_press(PadSeqEngine *engine, uint8_t row, uint8_t col, uint8_t velocity);
void padseq_handle_pad_release(PadSeqEngine *engine, uint8_t row, uint8_t col);
void padseq_handle_side_button(PadSeqEngine *engine, uint8_t index);
void padseq_handle_top_button(PadSeqEngine *engine, uint8_t index);

void padseq_start(PadSeqEngine *engine);
void padseq_stop(PadSeqEngine *engine);
void padseq_set_tempo(PadSeqEngine *engine, uint16_t bpm);

void padseq_process(PadSeqEngine *engine, uint32_t n_samples);
void padseq_begin_block(PadSeqEngine *engine);
void padseq_refresh_grid_state(PadSeqEngine *engine);
uint32_t padseq_default_gate_frames(const PadSeqEngine *engine);

void padseq_set_pattern(PadSeqEngine *engine, uint8_t pattern);
void padseq_clear_pattern(PadSeqEngine *engine);

#ifdef __cplusplus
}
#endif

#endif // PADSEQ_ENGINE_H
