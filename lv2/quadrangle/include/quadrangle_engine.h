#ifndef QUADRANGLE_ENGINE_H
#define QUADRANGLE_ENGINE_H

#include <stdint.h>
#include "grid_state.h"
#include "midi_comm.h"

/*
 * Quadrangle Engine
 *
 * Main coordinator for the Quadrangle performance instrument.
 * Manages four quadrants:
 * - NW: Drum sequencer (16-step, 8 voices)
 * - NE: Melodic sequencer (16-step, scale-quantized)
 * - SW: Live performance pads (16 one-shots)
 * - SE: Parameter controls (16 context-sensitive macros)
 */

// ============================================================================
// Engine Configuration
// ============================================================================

#define MAX_DRUM_VOICES 8
#define SEQ_STEPS 16
#define LIVE_PADS 16
#define PARAM_CONTROLS 16
#define MAX_MIDI_EVENTS 128
#define DEFAULT_GATE_MS 50

// ============================================================================
// Drum Voice
// ============================================================================

typedef struct {
    uint8_t active;         // Is this voice active?
    uint8_t note;           // MIDI note to trigger
    uint8_t steps[SEQ_STEPS]; // Step velocities (0 = off, 1-127 = velocity)
    uint8_t color;          // LED color for this voice
} DrumVoice;

// ============================================================================
// Melody Sequencer
// ============================================================================

typedef enum {
    SCALE_CHROMATIC,
    SCALE_MAJOR,
    SCALE_MINOR,
    SCALE_PENTATONIC,
    SCALE_BLUES,
    SCALE_DORIAN
} ScaleType;

typedef struct {
    uint8_t active;
    uint8_t notes[SEQ_STEPS];  // Scale degrees (0..)
    uint8_t velocities[SEQ_STEPS];
    uint8_t root_note;         // Root note (C4 = 60)
    int8_t root_note_shift;    // Semitone shift (-2..+2)
    uint8_t direction;         // 0 = descending, 1 = ascending
    ScaleType scale;
    uint8_t degree_bank;       // 0-3 (base degree = bank * 4)
} MelodySequencer;

// ============================================================================
// Live Pads
// ============================================================================

typedef struct {
    uint8_t note;              // MIDI note
    uint8_t velocity;
    uint8_t mode;              // 0=oneshot, 1=toggle, 2=momentary
    uint8_t state;             // Current state
} LivePad;

// ============================================================================
// Parameter Control
// ============================================================================

typedef enum {
    PARAM_FILTER_FREQ,
    PARAM_FILTER_Q,
    PARAM_DISTORTION,
    PARAM_REVERB_SIZE,
    PARAM_REVERB_MIX,
    PARAM_DELAY_TIME,
    PARAM_DELAY_FB,
    PARAM_MASTER_GAIN,
    PARAM_DRUM_TUNE,
    PARAM_ENVELOPE_ATTACK,
    PARAM_ENVELOPE_RELEASE,
    PARAM_SWING_AMOUNT,
    PARAM_TEMPO,
    PARAM_PATTERN_LENGTH,
    PARAM_CUSTOM_1,
    PARAM_CUSTOM_2
} ParameterType;

typedef struct {
    ParameterType type;
    uint8_t value;             // 0-127
    const char *name;
} ParameterControl;

// ============================================================================
// MIDI Event Queue (host-facing)
// ============================================================================

typedef struct {
    uint32_t frame;      // Sample offset within current block
    uint8_t size;        // Usually 3 bytes
    uint8_t data[3];     // MIDI message
} MidiOutEvent;

// ============================================================================
// Quadrangle Engine
// ============================================================================

typedef struct {
    // State management
    LaunchpadState grid_state;

    // Sequencer state
    DrumVoice drum_voices[MAX_DRUM_VOICES];
    uint8_t drum_beats[MAX_DRUM_VOICES];
    uint8_t drum_offsets[MAX_DRUM_VOICES];
    uint8_t drum_patterns[2][MAX_DRUM_VOICES][SEQ_STEPS];
    MelodySequencer melody;
    uint8_t melody_patterns[2][SEQ_STEPS];
    uint8_t melody_vel_patterns[2][SEQ_STEPS];
    LivePad live_pads[LIVE_PADS];
    ParameterControl params[PARAM_CONTROLS];
    uint8_t melody_program;
    uint8_t live_pad_patterns[2][LIVE_PADS];
    uint8_t param_patterns[2][PARAM_CONTROLS];

    // Playback state
    uint8_t playing;
    uint32_t sample_counter;
    uint32_t samples_per_step;
    uint16_t current_step;     // 0-15
    uint8_t selected_voice;    // Currently selected drum voice for editing
    uint8_t selected_quadrant; // Currently active quadrant

    // Sample rate
    float sample_rate;

    // MIDI output queue for current block
    MidiOutEvent midi_events[MAX_MIDI_EVENTS];
    uint16_t midi_event_count;

} QuadrangleEngine;

// ============================================================================
// Function Declarations
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Lifecycle
void quadrangle_init(QuadrangleEngine *engine, float sample_rate);
void quadrangle_reset(QuadrangleEngine *engine);

// Input handling
void quadrangle_handle_pad_press(QuadrangleEngine *engine, uint8_t row, uint8_t col, uint8_t velocity);
void quadrangle_handle_pad_release(QuadrangleEngine *engine, uint8_t row, uint8_t col);
void quadrangle_handle_side_button(QuadrangleEngine *engine, uint8_t index);
void quadrangle_handle_top_button(QuadrangleEngine *engine, uint8_t index);

// Sequencer control
void quadrangle_start(QuadrangleEngine *engine);
void quadrangle_stop(QuadrangleEngine *engine);
void quadrangle_set_tempo(QuadrangleEngine *engine, uint16_t bpm);

// Audio processing
void quadrangle_process(QuadrangleEngine *engine, uint32_t n_samples);
void quadrangle_begin_block(QuadrangleEngine *engine);
void quadrangle_refresh_grid_state(QuadrangleEngine *engine);
uint32_t quadrangle_default_gate_frames(const QuadrangleEngine *engine);

// Grid update
void quadrangle_update_leds(QuadrangleEngine *engine, MidiMessage *msg);

// Pattern management
void quadrangle_set_pattern(QuadrangleEngine *engine, uint8_t pattern);
void quadrangle_copy_pattern(QuadrangleEngine *engine, uint8_t src, uint8_t dst);
void quadrangle_clear_pattern(QuadrangleEngine *engine);

// Quadrant-specific
void quadrangle_set_drum_step(QuadrangleEngine *engine, uint8_t voice, uint8_t step, uint8_t velocity);
void quadrangle_set_melody_note(QuadrangleEngine *engine, uint8_t step, uint8_t note, uint8_t velocity);
void quadrangle_trigger_live_pad(QuadrangleEngine *engine, uint8_t pad_index, uint8_t velocity);
void quadrangle_set_parameter(QuadrangleEngine *engine, uint8_t param_index, uint8_t value);

#ifdef __cplusplus
}
#endif

#endif // QUADRANGLE_ENGINE_H
