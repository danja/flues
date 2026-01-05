#ifndef FLUES_SYNTH_SYNTH_ENGINE_H
#define FLUES_SYNTH_SYNTH_ENGINE_H

#include <stdbool.h>

// Opaque synth engine structure
typedef struct SynthEngine SynthEngine;

// Create synth engine
SynthEngine* synth_engine_create(float sample_rate);

// Destroy synth engine
void synth_engine_destroy(SynthEngine* engine);

// Process one audio buffer
void synth_engine_process(SynthEngine* engine, float* output, int num_samples);

// Note control
void synth_engine_note_on(SynthEngine* engine, int midi_note, float frequency, unsigned char velocity);
void synth_engine_note_off(SynthEngine* engine, int midi_note);
void synth_engine_all_notes_off(SynthEngine* engine);
void synth_engine_reset(SynthEngine* engine);  // Reset all parameters to defaults

// =============================================================================
// PARAMETER SETTERS (Phase 1 - Essential Parameters)
// =============================================================================

// Disyn Source
void synth_engine_set_disyn_algorithm(SynthEngine* engine, int algorithm);  // 0-16 (updated)
void synth_engine_set_disyn_param1(SynthEngine* engine, float value);  // 0-1
void synth_engine_set_disyn_param2(SynthEngine* engine, float value);  // 0-1
void synth_engine_set_disyn_param3(SynthEngine* engine, float value);  // 0-1 (new for algorithms 7-16)
void synth_engine_set_disyn_level(SynthEngine* engine, float value);  // 0-1
void synth_engine_set_noise_level(SynthEngine* engine, float value);  // 0-1
void synth_engine_set_dc_level(SynthEngine* engine, float value);  // 0-1

// Trajectory Oscillator (polygon bounce)
void synth_engine_set_trajectory_sides(SynthEngine* engine, float normalized);  // 0-1 -> 3-12 sides
void synth_engine_set_trajectory_start_pos(SynthEngine* engine, float normalized);  // 0-1 -> 0-360 deg
void synth_engine_set_trajectory_start_angle(SynthEngine* engine, float normalized);  // 0-1 -> 0-360 deg
void synth_engine_set_trajectory_clip(SynthEngine* engine, float normalized);  // 0-1 -> drive 1-5

// Formants
void synth_engine_set_f1(SynthEngine* engine, float frequency);  // 200-1000 Hz
void synth_engine_set_f2(SynthEngine* engine, float frequency);  // 500-3000 Hz
void synth_engine_set_f3(SynthEngine* engine, float frequency);  // 1500-4000 Hz
void synth_engine_set_f4(SynthEngine* engine, float frequency);  // 2500-4500 Hz

// Vocal Modes
void synth_engine_set_nasal(SynthEngine* engine, bool enabled);
void synth_engine_set_sing(SynthEngine* engine, bool enabled);  // Vibrato
void synth_engine_set_shout(SynthEngine* engine, bool enabled);  // 15% boost
void synth_engine_set_fry(SynthEngine* engine, bool enabled);  // Subharmonics

// Envelope
void synth_engine_set_attack(SynthEngine* engine, float value);  // 0-1 normalized
void synth_engine_set_release(SynthEngine* engine, float value);  // 0-1 normalized

// Interface & Delay
void synth_engine_set_interface_type(SynthEngine* engine, int type);  // 0-11
void synth_engine_set_intensity(SynthEngine* engine, float value);  // 0-1
void synth_engine_set_tuning(SynthEngine* engine, float semitones);  // -12 to +12
void synth_engine_set_ratio(SynthEngine* engine, float value);  // 0.5 to 2.0

// Feedback
void synth_engine_set_delay1_feedback(SynthEngine* engine, float value);  // 0-1
void synth_engine_set_delay2_feedback(SynthEngine* engine, float value);  // 0-1
void synth_engine_set_filter_feedback(SynthEngine* engine, float value);  // 0-1

// Filter
void synth_engine_set_filter_frequency(SynthEngine* engine, float frequency);  // Hz
void synth_engine_set_filter_q(SynthEngine* engine, float q);  // 0.1 to 10
void synth_engine_set_filter_shape(SynthEngine* engine, float shape);  // 0-1

// Modulation
void synth_engine_set_lfo_frequency(SynthEngine* engine, float frequency);  // Hz
void synth_engine_set_am_fm_depth(SynthEngine* engine, float depth);  // -1 to +1

// Reverb
void synth_engine_set_reverb_size(SynthEngine* engine, float value);  // 0-1
void synth_engine_set_reverb_level(SynthEngine* engine, float value);  // 0-1

// Output
void synth_engine_set_master_gain(SynthEngine* engine, float gain);  // 0-1

// Get number of active voices (for UI display)
int synth_engine_get_active_voice_count(SynthEngine* engine);

// Debug/diagnostic toggles (bypass parts of the chain)
void synth_engine_enable_disyn(SynthEngine* engine, bool enabled);
void synth_engine_enable_trajectory(SynthEngine* engine, bool enabled);
void synth_engine_enable_noise(SynthEngine* engine, bool enabled);
void synth_engine_enable_feedback(SynthEngine* engine, bool enabled);
void synth_engine_enable_formants(SynthEngine* engine, bool enabled);
void synth_engine_enable_filter(SynthEngine* engine, bool enabled);
void synth_engine_hard_mute(SynthEngine* engine, bool enabled);
bool synth_engine_is_noise_enabled(SynthEngine* engine);
bool synth_engine_is_disyn_enabled(SynthEngine* engine);
bool synth_engine_is_trajectory_enabled(SynthEngine* engine);
bool synth_engine_is_feedback_enabled(SynthEngine* engine);
bool synth_engine_is_formants_enabled(SynthEngine* engine);
bool synth_engine_is_filter_enabled(SynthEngine* engine);
bool synth_engine_is_hard_muted(SynthEngine* engine);

#endif // FLUES_SYNTH_SYNTH_ENGINE_H
