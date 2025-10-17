// pm_synth.h
// Main physical modeling synthesizer header
// Based on experiments/pm-synth JavaScript implementation

#ifndef PM_SYNTH_H
#define PM_SYNTH_H

#include <stdint.h>
#include <stdbool.h>

// Audio configuration
#define DEFAULT_SAMPLE_RATE 44100
#define DEFAULT_BUFFER_SIZE 256
#define MAX_DELAY_LENGTH 8192

// Interface types (0-11)
typedef enum {
    INTERFACE_PLUCK = 0,
    INTERFACE_HIT = 1,
    INTERFACE_REED = 2,
    INTERFACE_FLUTE = 3,
    INTERFACE_BRASS = 4,
    INTERFACE_BOW = 5,
    INTERFACE_BELL = 6,
    INTERFACE_DRUM = 7,
    INTERFACE_CRYSTAL = 8,
    INTERFACE_VAPOR = 9,
    INTERFACE_QUANTUM = 10,
    INTERFACE_PLASMA = 11
} InterfaceType;

// Forward declarations
typedef struct PMSynthEngine PMSynthEngine;

// Main synthesizer engine
PMSynthEngine* pm_synth_create(float sample_rate);
void pm_synth_destroy(PMSynthEngine* synth);
void pm_synth_process(PMSynthEngine* synth, float* output, int num_samples);
void pm_synth_note_on(PMSynthEngine* synth, float frequency);
void pm_synth_note_off(PMSynthEngine* synth);

// Parameter setters (0-100 range, normalized internally)
void pm_synth_set_dc_level(PMSynthEngine* synth, float value);
void pm_synth_set_noise_level(PMSynthEngine* synth, float value);
void pm_synth_set_tone_level(PMSynthEngine* synth, float value);
void pm_synth_set_attack(PMSynthEngine* synth, float value);
void pm_synth_set_release(PMSynthEngine* synth, float value);
void pm_synth_set_interface_type(PMSynthEngine* synth, InterfaceType type);
void pm_synth_set_interface_intensity(PMSynthEngine* synth, float value);
void pm_synth_set_tuning(PMSynthEngine* synth, float value);
void pm_synth_set_ratio(PMSynthEngine* synth, float value);
void pm_synth_set_delay1_feedback(PMSynthEngine* synth, float value);
void pm_synth_set_delay2_feedback(PMSynthEngine* synth, float value);
void pm_synth_set_filter_feedback(PMSynthEngine* synth, float value);
void pm_synth_set_filter_frequency(PMSynthEngine* synth, float value);
void pm_synth_set_filter_q(PMSynthEngine* synth, float value);
void pm_synth_set_filter_shape(PMSynthEngine* synth, float value);
void pm_synth_set_lfo_frequency(PMSynthEngine* synth, float value);
void pm_synth_set_modulation_depth(PMSynthEngine* synth, float value);
void pm_synth_set_reverb_size(PMSynthEngine* synth, float value);
void pm_synth_set_reverb_level(PMSynthEngine* synth, float value);

// Utility functions
float pm_synth_midi_to_frequency(int midi_note);
const char* pm_synth_interface_name(InterfaceType type);

#endif // PM_SYNTH_H
