#ifndef ACHORD_ENGINE_H
#define ACHORD_ENGINE_H

#include <stdint.h>
#include "grid_state.h"
#include "midi_comm.h"
#include "voicing_engine.h"

#define ACHORD_MAX_MIDI_EVENTS 256
#define ACHORD_MAX_PENDING_NOTEOFFS 256

typedef struct {
    uint32_t frame;
    uint8_t size;
    uint8_t data[3];
} MidiOutEvent;

typedef struct {
    uint8_t active;
    uint8_t note;
    uint32_t frames_left;
} AchordPendingNoteOff;

typedef struct {
    uint8_t is_down;
    uint8_t is_latched;
    uint8_t logic_active;
    uint8_t sounding;
    uint8_t velocity;
    uint32_t order;
    uint8_t current_note_count;
    uint8_t current_notes[ACHORD_MAX_CHORD_NOTES];
    uint8_t current_note_on[ACHORD_MAX_CHORD_NOTES];
    uint8_t pending_quantized;
    uint64_t pending_step;
    uint8_t strum_pending;
    uint8_t strum_note_count;
    uint8_t strum_notes[ACHORD_MAX_CHORD_NOTES];
    uint8_t strum_velocities[ACHORD_MAX_CHORD_NOTES];
    uint32_t strum_next_frame;
    uint16_t strum_spacing_frames;
    uint8_t strum_index;
    uint8_t strum_direction;
    uint64_t last_repeat_step;
} AchordPadState;

typedef struct {
    int8_t bank_offset;
    uint8_t tonic_note;
    uint8_t scale_index;
    uint8_t register_mode;
    uint8_t trigger_mode;
    uint8_t hold_mode;
    uint8_t bass_enabled;
    uint8_t add9_enabled;
    uint8_t sus_mode;
    int8_t inversion_offset;
    uint8_t spread_mode;
    uint8_t accent_enabled;
    uint8_t voice_lead_enabled;
} AchordPersistState;

typedef struct {
    LaunchpadState grid_state;
    float sample_rate;

    AchordPersistState config;

    uint16_t bpm;
    uint8_t host_time_valid;
    uint8_t host_playing;
    double host_bar;
    double host_bar_beat;
    double beats_per_bar;

    double block_abs_steps_start;
    double block_abs_steps_end;
    double block_steps_per_frame;
    uint32_t block_n_samples;

    uint8_t current_step16;
    uint8_t active_chord_count;
    uint32_t order_counter;

    AchordPadState pads[GRID_HEIGHT][GRID_WIDTH];
    uint8_t note_refcount[128];
    uint8_t last_voicing_count;
    uint8_t last_voicing_notes[ACHORD_MAX_CHORD_NOTES];

    MidiOutEvent midi_events[ACHORD_MAX_MIDI_EVENTS];
    uint16_t midi_event_count;
    AchordPendingNoteOff pending_note_offs[ACHORD_MAX_PENDING_NOTEOFFS];
} AchordEngine;

#ifdef __cplusplus
extern "C" {
#endif

void achord_init(AchordEngine *engine, float sample_rate);
void achord_reset(AchordEngine *engine);
void achord_begin_block(AchordEngine *engine,
                        uint32_t n_samples,
                        uint8_t host_time_valid,
                        uint8_t host_playing,
                        double bar,
                        double bar_beat,
                        double beats_per_bar,
                        double bpm);
void achord_process(AchordEngine *engine, uint32_t n_samples);
void achord_refresh_grid_state(AchordEngine *engine);

void achord_handle_pad_press(AchordEngine *engine,
                             uint8_t row,
                             uint8_t col,
                             uint8_t velocity,
                             uint32_t frame);
void achord_handle_pad_release(AchordEngine *engine,
                               uint8_t row,
                               uint8_t col,
                               uint32_t frame);
void achord_handle_side_button(AchordEngine *engine, uint8_t index, uint32_t frame);
void achord_handle_top_button(AchordEngine *engine, uint8_t index, uint32_t frame);

void achord_panic(AchordEngine *engine, uint32_t frame);

#ifdef __cplusplus
}
#endif

#endif
