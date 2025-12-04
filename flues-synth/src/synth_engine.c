// Synth Engine - Main DSP Coordinator
// Phase 1: Single voice implementation

#include "synth_engine.h"
#include "dsp_modules.h"
#include "dsp_utils.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Single voice structure (Phase 1)
typedef struct {
    // Voice state
    bool active;
    int midi_note;
    float frequency;
    float base_frequency;

    // DSP Modules
    DisynModule* disyn;
    SourcesModule* sources;
    EnvelopeModule* envelope;
    FormantBankModule* formant_bank;
    InterfaceModule* interface;
    DelayLinesModule* delay_lines;
    FeedbackModule* feedback;
    FilterModule* filter;
    ModulationModule* modulation;

    // Feedback state
    float prev_delay1_out;
    float prev_delay2_out;
    float prev_filter_out;

    // DC blocker (on feedback path only)
    DCBlocker dc_blocker;

    // Vibrato for Sing mode
    float vibrato_phase;
    bool vibrato_enabled;
} Voice;

struct SynthEngine {
    float sample_rate;

    // Single voice (Phase 1)
    Voice voice;

    // Global parameters
    float master_gain;
    float disyn_level;  // Disyn tone level

    // Vocal modes
    bool sing_enabled;   // Vibrato on Disyn frequency
    bool fry_enabled;    // f0/2 subharmonic (TODO: needs Disyn modification)
};

// Initialize voice
static void voice_init(Voice* voice, float sample_rate) {
    memset(voice, 0, sizeof(Voice));

    voice->disyn = disyn_create(sample_rate);
    voice->sources = sources_create(sample_rate);
    voice->envelope = envelope_create(sample_rate);
    voice->formant_bank = formant_bank_create(sample_rate);
    voice->interface = interface_create(sample_rate);
    voice->delay_lines = delay_lines_create(sample_rate);
    voice->feedback = feedback_create();
    voice->filter = filter_create(sample_rate);
    voice->modulation = modulation_create(sample_rate);

    dc_blocker_init(&voice->dc_blocker, DC_BLOCKER_R);

    voice->active = false;
    voice->midi_note = -1;
    voice->vibrato_phase = 0.0f;
}

// Destroy voice
static void voice_destroy(Voice* voice) {
    disyn_destroy(voice->disyn);
    sources_destroy(voice->sources);
    envelope_destroy(voice->envelope);
    formant_bank_destroy(voice->formant_bank);
    interface_destroy(voice->interface);
    delay_lines_destroy(voice->delay_lines);
    feedback_destroy(voice->feedback);
    filter_destroy(voice->filter);
    modulation_destroy(voice->modulation);
}

// Process one sample for voice
static float voice_process_sample(Voice* voice, SynthEngine* engine) {
    if (!voice->active) {
        return 0.0f;
    }

    // 1. Modulation LFO (updates internal state)
    modulation_process(voice->modulation);
    float am_scale = modulation_get_am_scale(voice->modulation);

    // 2. Apply vibrato if Sing mode enabled
    float frequency = voice->frequency;
    if (voice->vibrato_enabled) {
        float vibrato = sinf(voice->vibrato_phase) * 0.015f;  // ±1.5%
        frequency *= (1.0f + vibrato);
        voice->vibrato_phase += 2.0f * M_PI * 5.5f / engine->sample_rate;
        if (voice->vibrato_phase >= 2.0f * M_PI) {
            voice->vibrato_phase -= 2.0f * M_PI;
        }
    }

    // 3. Disyn Oscillator
    float disyn_out = disyn_process(voice->disyn, frequency);
    disyn_out *= engine->disyn_level;

    // 4. Sources (Noise + DC)
    float sources_out = sources_process(voice->sources);

    // 5. Mix: Disyn + Noise + DC
    float excitation = disyn_out + sources_out;

    // 6. Envelope
    float env = envelope_process(voice->envelope);
    excitation *= env;

    // Check if envelope is done
    if (!envelope_is_active(voice->envelope)) {
        voice->active = false;
        return 0.0f;
    }

    // 7. Chatterbox Formant Bank
    float formant_out = formant_bank_process(voice->formant_bank, excitation);

    // 8. Feedback Mix (DC blocker applied to feedback path only)
    float fb_delay1 = dc_blocker_process(&voice->dc_blocker, voice->prev_delay1_out);
    float fb_delay2 = dc_blocker_process(&voice->dc_blocker, voice->prev_delay2_out);
    float fb_filter = dc_blocker_process(&voice->dc_blocker, voice->prev_filter_out);

    float feedback_mix = feedback_process(voice->feedback, fb_delay1, fb_delay2, fb_filter);

    // 9. Add feedback to signal
    float interface_input = formant_out + feedback_mix;

    // 10. Interface Module (Physical Modeling)
    float interface_out = interface_process(voice->interface, interface_input);

    // 11. Dual Delay Lines
    float delay1_out, delay2_out;
    delay_lines_process(voice->delay_lines, interface_out, &delay1_out, &delay2_out);

    // 12. Filter
    float filter_out = filter_process(voice->filter, delay2_out);

    // 13. Store previous outputs for next sample's feedback
    voice->prev_delay1_out = delay1_out;
    voice->prev_delay2_out = delay2_out;
    voice->prev_filter_out = filter_out;

    // 14. Apply AM modulation
    float output = filter_out * am_scale;

    // 15. Master gain
    output *= engine->master_gain;

    return output;
}

// Create synth engine
SynthEngine* synth_engine_create(float sample_rate) {
    SynthEngine* engine = (SynthEngine*)calloc(1, sizeof(SynthEngine));
    if (!engine) return NULL;

    engine->sample_rate = sample_rate;
    engine->master_gain = 0.5f;
    engine->disyn_level = 0.5f;
    engine->sing_enabled = false;
    engine->fry_enabled = false;

    // Initialize single voice
    voice_init(&engine->voice, sample_rate);

    // Set default parameters
    disyn_set_algorithm(engine->voice.disyn, 0);  // Dirichlet Pulse
    disyn_set_param1(engine->voice.disyn, 0.5f);
    disyn_set_param2(engine->voice.disyn, 0.5f);

    sources_set_noise_level(engine->voice.sources, 0.1f);
    sources_set_dc_level(engine->voice.sources, 0.0f);

    envelope_set_attack(engine->voice.envelope, 0.1f);  // ~10ms
    envelope_set_release(engine->voice.envelope, 0.3f);  // ~100ms

    // Set default formant frequencies (neutral vowel)
    formant_bank_set_f1(engine->voice.formant_bank, 500.0f);
    formant_bank_set_f2(engine->voice.formant_bank, 1500.0f);
    formant_bank_set_f3(engine->voice.formant_bank, 2500.0f);
    formant_bank_set_f4(engine->voice.formant_bank, 3500.0f);

    interface_set_type(engine->voice.interface, INTERFACE_REED);
    interface_set_intensity(engine->voice.interface, 0.5f);

    delay_lines_set_tuning(engine->voice.delay_lines, 0.0f);
    delay_lines_set_ratio(engine->voice.delay_lines, 1.0f);

    feedback_set_delay1_level(engine->voice.feedback, 0.3f);
    feedback_set_delay2_level(engine->voice.feedback, 0.3f);
    feedback_set_filter_level(engine->voice.feedback, 0.2f);

    filter_set_frequency(engine->voice.filter, 2000.0f);
    filter_set_q(engine->voice.filter, 1.0f);
    filter_set_shape(engine->voice.filter, 0.0f);  // Lowpass

    modulation_set_frequency(engine->voice.modulation, 2.0f);
    modulation_set_depth(engine->voice.modulation, 0.0f);  // No modulation by default

    return engine;
}

// Destroy synth engine
void synth_engine_destroy(SynthEngine* engine) {
    if (!engine) return;

    voice_destroy(&engine->voice);
    free(engine);
}

// Process audio buffer
void synth_engine_process(SynthEngine* engine, float* output, int num_samples) {
    for (int i = 0; i < num_samples; i++) {
        output[i] = voice_process_sample(&engine->voice, engine);
    }
}

// Note on
void synth_engine_note_on(SynthEngine* engine, int midi_note, float frequency) {
    Voice* voice = &engine->voice;

    voice->active = true;
    voice->midi_note = midi_note;
    voice->frequency = frequency;
    voice->base_frequency = frequency;
    voice->vibrato_enabled = engine->sing_enabled;
    voice->vibrato_phase = 0.0f;

    // Reset modules
    envelope_set_gate(voice->envelope, true);
    envelope_reset(voice->envelope);
    delay_lines_note_on(voice->delay_lines, frequency);
    interface_set_gate(voice->interface, 1.0f);

    // Clear feedback state
    voice->prev_delay1_out = 0.0f;
    voice->prev_delay2_out = 0.0f;
    voice->prev_filter_out = 0.0f;
    dc_blocker_init(&voice->dc_blocker, DC_BLOCKER_R);
}

// Note off
void synth_engine_note_off(SynthEngine* engine, int midi_note) {
    Voice* voice = &engine->voice;

    if (voice->midi_note == midi_note && voice->active) {
        envelope_set_gate(voice->envelope, false);
        interface_set_gate(voice->interface, 0.0f);
    }
}

// All notes off
void synth_engine_all_notes_off(SynthEngine* engine) {
    engine->voice.active = false;
    envelope_set_gate(engine->voice.envelope, false);
}

// Get active voice count
int synth_engine_get_active_voice_count(SynthEngine* engine) {
    return engine->voice.active ? 1 : 0;
}

// ============================================================================
// PARAMETER SETTERS
// ============================================================================

// Disyn Source
void synth_engine_set_disyn_algorithm(SynthEngine* engine, int algorithm) {
    disyn_set_algorithm(engine->voice.disyn, algorithm);
}

void synth_engine_set_disyn_param1(SynthEngine* engine, float value) {
    disyn_set_param1(engine->voice.disyn, value);
}

void synth_engine_set_disyn_param2(SynthEngine* engine, float value) {
    disyn_set_param2(engine->voice.disyn, value);
}

void synth_engine_set_disyn_level(SynthEngine* engine, float value) {
    engine->disyn_level = value;
}

void synth_engine_set_noise_level(SynthEngine* engine, float value) {
    sources_set_noise_level(engine->voice.sources, value);
}

void synth_engine_set_dc_level(SynthEngine* engine, float value) {
    sources_set_dc_level(engine->voice.sources, value);
}

// Formants
void synth_engine_set_f1(SynthEngine* engine, float frequency) {
    formant_bank_set_f1(engine->voice.formant_bank, frequency);
}

void synth_engine_set_f2(SynthEngine* engine, float frequency) {
    formant_bank_set_f2(engine->voice.formant_bank, frequency);
}

void synth_engine_set_f3(SynthEngine* engine, float frequency) {
    formant_bank_set_f3(engine->voice.formant_bank, frequency);
}

void synth_engine_set_f4(SynthEngine* engine, float frequency) {
    formant_bank_set_f4(engine->voice.formant_bank, frequency);
}

// Vocal Modes
void synth_engine_set_nasal(SynthEngine* engine, bool enabled) {
    formant_bank_set_nasal_enabled(engine->voice.formant_bank, enabled);
}

void synth_engine_set_sing(SynthEngine* engine, bool enabled) {
    engine->sing_enabled = enabled;
}

void synth_engine_set_shout(SynthEngine* engine, bool enabled) {
    formant_bank_set_shout_enabled(engine->voice.formant_bank, enabled);
}

void synth_engine_set_fry(SynthEngine* engine, bool enabled) {
    engine->fry_enabled = enabled;
    // TODO: Modify Disyn to add f0/2 subharmonic
}

// Envelope
void synth_engine_set_attack(SynthEngine* engine, float value) {
    envelope_set_attack(engine->voice.envelope, value);
}

void synth_engine_set_release(SynthEngine* engine, float value) {
    envelope_set_release(engine->voice.envelope, value);
}

// Interface & Delay
void synth_engine_set_interface_type(SynthEngine* engine, int type) {
    interface_set_type(engine->voice.interface, (InterfaceType)type);
}

void synth_engine_set_intensity(SynthEngine* engine, float value) {
    interface_set_intensity(engine->voice.interface, value);
}

void synth_engine_set_tuning(SynthEngine* engine, float semitones) {
    delay_lines_set_tuning(engine->voice.delay_lines, semitones);
}

void synth_engine_set_ratio(SynthEngine* engine, float value) {
    delay_lines_set_ratio(engine->voice.delay_lines, value);
}

// Feedback
void synth_engine_set_delay1_feedback(SynthEngine* engine, float value) {
    feedback_set_delay1_level(engine->voice.feedback, value);
}

void synth_engine_set_delay2_feedback(SynthEngine* engine, float value) {
    feedback_set_delay2_level(engine->voice.feedback, value);
}

void synth_engine_set_filter_feedback(SynthEngine* engine, float value) {
    feedback_set_filter_level(engine->voice.feedback, value);
}

// Filter
void synth_engine_set_filter_frequency(SynthEngine* engine, float frequency) {
    filter_set_frequency(engine->voice.filter, frequency);
}

void synth_engine_set_filter_q(SynthEngine* engine, float q) {
    filter_set_q(engine->voice.filter, q);
}

void synth_engine_set_filter_shape(SynthEngine* engine, float shape) {
    filter_set_shape(engine->voice.filter, shape);
}

// Modulation
void synth_engine_set_lfo_frequency(SynthEngine* engine, float frequency) {
    modulation_set_frequency(engine->voice.modulation, frequency);
}

void synth_engine_set_am_fm_depth(SynthEngine* engine, float depth) {
    modulation_set_depth(engine->voice.modulation, depth);
}

// Reverb (skipped in Phase 1)
void synth_engine_set_reverb_size(SynthEngine* engine, float value) {
    // No-op
}

void synth_engine_set_reverb_level(SynthEngine* engine, float value) {
    // No-op
}

// Output
void synth_engine_set_master_gain(SynthEngine* engine, float gain) {
    engine->master_gain = gain;
}
