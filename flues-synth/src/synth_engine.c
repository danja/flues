// Synth Engine - Main DSP Coordinator (Polyphonic, 4 voices by default)

#include "synth_engine.h"
#include "dsp_modules.h"
#include "dsp_utils.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>  // For debug prints
#include <stdint.h> // For sample counters

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Voice structure (Phase 2: Polyphonic)
typedef struct
{
    // Voice state
    bool active;
    int midi_note;
    uint32_t note_on_age; // Timestamp for oldest-note stealing
    uint8_t velocity;     // MIDI velocity (0-127) for dynamics
    float frequency;
    float base_frequency;
    uint64_t note_on_sample; // Sample index when note-on arrived
    bool gate_forced_off;    // True once a forced release was applied

    // DSP Modules
    DisynModule *disyn;
    SourcesModule *sources;
    EnvelopeModule *envelope;
    FormantBankModule *formant_bank;
    InterfaceModule *interface;
    DelayLinesModule *delay_lines;
    FeedbackModule *feedback;
    FilterModule *filter;
    ModulationModule *modulation;

    // Feedback state
    float prev_delay1_out;
    float prev_delay2_out;
    float prev_filter_out;

    // Dual DC blockers to prevent feedback latching
    DCBlocker dc_blocker_disyn;    // After Disyn oscillator (catches DC at source)
    DCBlocker dc_blocker_feedback; // On feedback path (prevents loop accumulation)

    // Vibrato for Sing mode
    float vibrato_phase;
    bool vibrato_enabled;
} Voice;

struct SynthEngine
{
    float sample_rate;

    // Voice pool (Phase 2: Polyphonic - 4 voices)
    Voice voices[MAX_VOICES];
    uint32_t global_note_counter; // For age-based voice stealing
    uint64_t sample_counter;      // Global sample position (for timeouts)

    // Global parameters
    float master_gain;
    float disyn_level; // Disyn tone level

    // Fallback handling when note-off is missing
    bool watchdog_enabled;
    uint64_t watchdog_samples; // Max hold before auto-release (0 = disabled)
    bool fixed_duration_enabled;
    uint64_t fixed_duration_samples; // Force release after this many samples

    // Vocal modes
    bool sing_enabled; // Vibrato on Disyn frequency
    bool fry_enabled;  // f0/2 subharmonic (TODO: needs Disyn modification)

    // Debug toggles for isolating noise sources
    bool enable_disyn;
    bool enable_noise;
    bool enable_feedback;
    bool enable_formants;
    bool enable_filter;
    bool hard_mute;
};

// Initialize voice
static void voice_init(Voice *voice, float sample_rate)
{
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

    // Initialize dual DC blockers
    dc_blocker_init(&voice->dc_blocker_disyn, DC_BLOCKER_R);
    dc_blocker_init(&voice->dc_blocker_feedback, DC_BLOCKER_R);

    voice->active = false;
    voice->midi_note = -1;
    voice->vibrato_phase = 0.0f;
    voice->note_on_sample = 0;
    voice->gate_forced_off = false;
}

// Destroy voice
static void voice_destroy(Voice *voice)
{
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

// Trigger a release on a voice (used for watchdog / fixed-duration safety)
static void force_voice_off(Voice *voice)
{
    // Let the envelope release naturally
    envelope_set_gate(voice->envelope, false);
    interface_set_gate(voice->interface, 0.0f);
    voice->gate_forced_off = true;
}

// Process one sample for voice
static float voice_process_sample(Voice *voice, SynthEngine *engine)
{
    if (!voice->active)
    {
        return 0.0f;
    }

    if (engine->hard_mute)
    {
        return 0.0f;
    }

    // 1. Modulation LFO (updates internal state)
    modulation_process(voice->modulation);
    float am_scale = modulation_get_am_scale(voice->modulation);

    // 2. Apply vibrato if Sing mode enabled
    float frequency = voice->frequency;
    if (voice->vibrato_enabled)
    {
        float vibrato = sinf(voice->vibrato_phase) * 0.015f; // ±1.5%
        frequency *= (1.0f + vibrato);
        voice->vibrato_phase += 2.0f * M_PI * 5.5f / engine->sample_rate;
        if (voice->vibrato_phase >= 2.0f * M_PI)
        {
            voice->vibrato_phase -= 2.0f * M_PI;
        }
    }

    // 3. Apply forced releases if enabled (missing note-off protection)
    if (!voice->gate_forced_off)
    {
        uint64_t elapsed = engine->sample_counter - voice->note_on_sample;
        if (engine->fixed_duration_enabled && elapsed >= engine->fixed_duration_samples)
        {
            force_voice_off(voice);
        }
        else if (engine->watchdog_enabled && elapsed >= engine->watchdog_samples)
        {
            force_voice_off(voice);
        }
    }

    // 4. Disyn Oscillator + DC Blocker (catch DC at source)
    float disyn_out = engine->enable_disyn ? disyn_process(voice->disyn, frequency) : 0.0f;
    disyn_out = dc_blocker_process(&voice->dc_blocker_disyn, disyn_out);
    disyn_out *= engine->disyn_level;

    // 5. Sources (Noise + DC)
    float sources_out = engine->enable_noise ? sources_process(voice->sources) : 0.0f;

    // 6. Mix: Disyn + Noise + DC
    float excitation = disyn_out + sources_out;

    // 7. Envelope with velocity scaling
    float env = envelope_process(voice->envelope);
    float velocity_scale = voice->velocity / 127.0f; // Convert MIDI velocity (0-127) to scale (0.0-1.0)
    excitation *= env * velocity_scale;              // Apply both envelope and velocity

    // Check if envelope is done
    if (!envelope_is_active(voice->envelope))
    {
        voice->active = false;
        voice->gate_forced_off = false; // ready for reuse
        return 0.0f;
    }

    // 8. Chatterbox Formant Bank
    float formant_out = engine->enable_formants
                            ? formant_bank_process(voice->formant_bank, excitation)
                            : excitation;

    // 9. Feedback Mix + DC Blocker (prevent loop accumulation)
    float feedback_mix = engine->enable_feedback
                             ? feedback_process(voice->feedback,
                                                voice->prev_delay1_out,
                                                voice->prev_delay2_out,
                                                voice->prev_filter_out)
                             : 0.0f;
    feedback_mix = sanitize_sample(feedback_mix);
    float feedback_clean = dc_blocker_process(&voice->dc_blocker_feedback, feedback_mix);
    if (!isfinite(feedback_clean))
    {
        feedback_clean = 0.0f;
    }

    // 10. Add feedback to signal
    float interface_input = formant_out + feedback_clean;

    // 11. Interface Module (Physical Modeling)
    float interface_out = interface_process(voice->interface, interface_input);
    interface_out = sanitize_sample(interface_out);

    // 12. Dual Delay Lines
    float delay1_out, delay2_out;
    delay_lines_process(voice->delay_lines, interface_out, &delay1_out, &delay2_out);

    // 13. Filter
    float filter_out = engine->enable_filter
                           ? filter_process(voice->filter, delay2_out)
                           : delay2_out;
    filter_out = sanitize_sample(filter_out);

    // 14. Store previous outputs for next sample's feedback
    voice->prev_delay1_out = delay1_out;
    voice->prev_delay2_out = delay2_out;
    voice->prev_filter_out = filter_out;

    // 15. Apply AM modulation
    float output = filter_out * am_scale;

    // danny keyword for finding again
    output *= 0.7f;                         // Global pad to reduce accumulated level
    output = soft_clip_drive(output, 1.0f); // final guard against runaway noise

    // 16. Master gain (final DC blocker removed - was too aggressive and killed envelope attack)
    output *= engine->master_gain;

    return output;
}

// Create synth engine
SynthEngine *synth_engine_create(float sample_rate)
{
    SynthEngine *engine = (SynthEngine *)calloc(1, sizeof(SynthEngine));
    if (!engine)
        return NULL;

    engine->sample_rate = sample_rate;
    engine->global_note_counter = 0; // Initialize voice age counter
    engine->sample_counter = 0;
    engine->master_gain = 0.8f;      // Increased from 0.35 to boost output level (safe after soft clipping)
    // danny tweak
    engine->disyn_level = 0.8f; // Safe with formants (boost via CC19 to 0.5-1.0 when testing without formants)
    engine->sing_enabled = false;
    engine->fry_enabled = false;
    engine->enable_disyn = true;
    engine->enable_noise = true;
    engine->enable_feedback = true;
    engine->enable_formants = true;
    engine->enable_filter = true;
    engine->hard_mute = false;
    engine->watchdog_enabled = false;
    engine->fixed_duration_enabled = false;
    engine->watchdog_samples = 0;
    engine->fixed_duration_samples = 0;

    // Optional: force releases when note-off is missing (e.g., flaky MIDI interface)
    const char *watchdog_env = getenv("FLUES_NOTE_TIMEOUT_MS");
    if (watchdog_env)
    {
        float ms = strtof(watchdog_env, NULL);
        if (ms > 0.0f)
        {
            engine->watchdog_enabled = true;
            engine->watchdog_samples = (uint64_t)((ms / 1000.0f) * sample_rate);
            printf("MIDI: Auto-release watchdog enabled (%.1f ms)\n", ms);
        }
    }

    // Optional: fixed-duration notes (forces release after N ms regardless of note-off)
    const char *fixed_env = getenv("FLUES_FIXED_NOTE_MS");
    if (fixed_env)
    {
        float ms = strtof(fixed_env, NULL);
        if (ms > 0.0f)
        {
            engine->fixed_duration_enabled = true;
            engine->fixed_duration_samples = (uint64_t)((ms / 1000.0f) * sample_rate);
            printf("MIDI: Fixed note duration enabled (%.1f ms)\n", ms);
        }
    }

    // Initialize all voices in the voice pool
    for (int i = 0; i < MAX_VOICES; i++)
    {
        voice_init(&engine->voices[i], sample_rate);

        // Set default parameters for each voice
        Voice *voice = &engine->voices[i];

        disyn_set_algorithm(voice->disyn, 0); // Dirichlet Pulse
        disyn_set_param1(voice->disyn, 0.5f);
        disyn_set_param2(voice->disyn, 0.5f);

        sources_set_noise_level(voice->sources, 0.15f); // Increased from 0.02 to provide formant excitation
        sources_set_dc_level(voice->sources, 0.0f);

        envelope_set_attack(voice->envelope, 0.1f);  // ~10ms
        envelope_set_release(voice->envelope, 0.3f); // ~100ms

        // Set default formant frequencies (neutral vowel)
        formant_bank_set_f1(voice->formant_bank, 500.0f);
        formant_bank_set_f2(voice->formant_bank, 1500.0f);
        formant_bank_set_f3(voice->formant_bank, 2500.0f);
        formant_bank_set_f4(voice->formant_bank, 3500.0f);

        interface_set_type(voice->interface, INTERFACE_REED);
        interface_set_intensity(voice->interface, 0.5f);

        delay_lines_set_tuning(voice->delay_lines, 0.0f); // 0 semitone offset
        delay_lines_set_ratio(voice->delay_lines, 1.0f);  // same delay length

        feedback_set_delay1_level(voice->feedback, 0.2f);
        feedback_set_delay2_level(voice->feedback, 0.2f);
        feedback_set_filter_level(voice->feedback, 0.1f);

        filter_set_frequency(voice->filter, 2000.0f);
        filter_set_q(voice->filter, 1.0f);
        filter_set_shape(voice->filter, 0.0f); // Lowpass

        modulation_set_frequency(voice->modulation, 2.0f);
        modulation_set_depth(voice->modulation, 0.0f); // No modulation by default
    }

    return engine;
}

// Destroy synth engine
void synth_engine_destroy(SynthEngine *engine)
{
    if (!engine)
        return;

    // Destroy all voices in the voice pool
    for (int i = 0; i < MAX_VOICES; i++)
    {
        voice_destroy(&engine->voices[i]);
    }

    free(engine);
}

// Process audio buffer (polyphonic with energy-preserving mix)
void synth_engine_process(SynthEngine *engine, float *output, int num_samples)
{
    for (int i = 0; i < num_samples; i++, engine->sample_counter++)
    {
        float mix = 0.0f;
        int active_count = 0;

        // Sum all active voices
        for (int v = 0; v < MAX_VOICES; v++)
        {
            Voice *voice = &engine->voices[v];
            if (voice->active)
            {
                mix += voice_process_sample(voice, engine);
                active_count++;
            }
        }

        // Energy-preserving mix to prevent clipping
        // 1 voice: ×1.0, 2 voices: ×0.707, 4 voices: ×0.5
        if (active_count > 0)
        {
            float scale = 1.0f / sqrtf((float)active_count);
            output[i] = mix * scale;
        }
        else
        {
            output[i] = 0.0f;
        }
    }
}

// ============================================================================
// VOICE ALLOCATION HELPERS (Polyphonic)
// ============================================================================

// Find first inactive voice
static Voice *find_free_voice(SynthEngine *engine)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (!engine->voices[i].active)
        {
            return &engine->voices[i];
        }
    }
    return NULL; // All voices busy
}

// Find oldest active voice for stealing
static Voice *find_oldest_voice(SynthEngine *engine)
{
    Voice *oldest = NULL;
    uint32_t oldest_age = UINT32_MAX;

    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (engine->voices[i].active &&
            engine->voices[i].note_on_age < oldest_age)
        {
            oldest_age = engine->voices[i].note_on_age;
            oldest = &engine->voices[i];
        }
    }
    return oldest;
}

// ============================================================================
// NOTE MANAGEMENT
// ============================================================================

// Note on
void synth_engine_note_on(SynthEngine *engine, int midi_note, float frequency, unsigned char velocity)
{
    // Find free voice or steal oldest
    Voice *voice = find_free_voice(engine);
    if (!voice)
    {
        voice = find_oldest_voice(engine);
        if (voice)
        {
            // Voice stealing - optional debug message
            // printf("Voice steal: note %d → %d\n", voice->midi_note, midi_note);
        }
    }
    if (!voice)
        return; // Should never happen with MAX_VOICES > 0

    voice->active = true;
    voice->midi_note = midi_note;
    voice->note_on_age = engine->global_note_counter++;
    voice->velocity = velocity; // Use actual MIDI velocity (0-127)
    voice->frequency = frequency;
    voice->base_frequency = frequency;
    voice->vibrato_enabled = engine->sing_enabled;
    voice->vibrato_phase = 0.0f;
    voice->note_on_sample = engine->sample_counter;
    voice->gate_forced_off = false;

    // Reset modules
    envelope_reset(voice->envelope);
    envelope_set_gate(voice->envelope, true);
    delay_lines_note_on(voice->delay_lines, frequency);
    interface_set_gate(voice->interface, 1.0f);

    // Clear feedback state and DC blockers
    voice->prev_delay1_out = 0.0f;
    voice->prev_delay2_out = 0.0f;
    voice->prev_filter_out = 0.0f;
    dc_blocker_init(&voice->dc_blocker_disyn, DC_BLOCKER_R);
    dc_blocker_init(&voice->dc_blocker_feedback, DC_BLOCKER_R);
}

// Note off
void synth_engine_note_off(SynthEngine *engine, int midi_note)
{
    // Find ALL voices playing this MIDI note and release them
    // (handles duplicate notes - both voices release together)
    for (int i = 0; i < MAX_VOICES; i++)
    {
        Voice *voice = &engine->voices[i];
        if (voice->active && voice->midi_note == midi_note)
        {
            // Normal note-off: let the envelope release naturally
            envelope_set_gate(voice->envelope, false);
            interface_set_gate(voice->interface, 0.0f);
            voice->gate_forced_off = false;
        }
    }
}

// All notes off
void synth_engine_all_notes_off(SynthEngine *engine)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        Voice *voice = &engine->voices[i];
        if (voice->active)
        {
            envelope_set_gate(voice->envelope, false);
            interface_set_gate(voice->interface, 0.0f);
            voice->gate_forced_off = false;
        }
    }
}

// Reset all parameters to defaults
void synth_engine_reset(SynthEngine *engine)
{
    // 1. Stop all playing notes
    synth_engine_all_notes_off(engine);

    // 2. Reset global parameters
    engine->master_gain = 0.8f;
    engine->disyn_level = 0.8f;
    engine->sing_enabled = false;
    engine->fry_enabled = false;
    engine->enable_disyn = true;
    engine->enable_noise = true;
    engine->enable_feedback = true;
    engine->enable_formants = true;
    engine->enable_filter = true;
    engine->hard_mute = false;

    // 3. Reset all voice parameters using parameter setters
    // (These now automatically broadcast to all voices thanks to polyphony)

    // Disyn source
    synth_engine_set_disyn_algorithm(engine, 0);   // Dirichlet Pulse
    synth_engine_set_disyn_param1(engine, 0.5f);
    synth_engine_set_disyn_param2(engine, 0.5f);
    synth_engine_set_noise_level(engine, 0.15f);
    synth_engine_set_dc_level(engine, 0.0f);

    // Formants (neutral vowel)
    synth_engine_set_f1(engine, 500.0f);
    synth_engine_set_f2(engine, 1500.0f);
    synth_engine_set_f3(engine, 2500.0f);
    synth_engine_set_f4(engine, 3500.0f);

    // Vocal modes
    synth_engine_set_nasal(engine, false);
    synth_engine_set_sing(engine, false);
    synth_engine_set_shout(engine, false);
    synth_engine_set_fry(engine, false);

    // Envelope
    synth_engine_set_attack(engine, 0.1f);   // ~10ms
    synth_engine_set_release(engine, 0.3f);  // ~100ms

    // Interface & Delay
    synth_engine_set_interface_type(engine, 2);  // INTERFACE_REED
    synth_engine_set_intensity(engine, 0.5f);
    synth_engine_set_tuning(engine, 0.0f);       // 0 semitone offset
    synth_engine_set_ratio(engine, 1.0f);        // Same delay length

    // Feedback
    synth_engine_set_delay1_feedback(engine, 0.2f);
    synth_engine_set_delay2_feedback(engine, 0.2f);
    synth_engine_set_filter_feedback(engine, 0.1f);

    // Filter
    synth_engine_set_filter_frequency(engine, 2000.0f);
    synth_engine_set_filter_q(engine, 1.0f);
    synth_engine_set_filter_shape(engine, 0.0f);  // Lowpass

    // Modulation
    synth_engine_set_lfo_frequency(engine, 2.0f);
    synth_engine_set_am_fm_depth(engine, 0.0f);   // No modulation

    printf("Ctl Note 42: RESET - All parameters returned to defaults\n");
}

// Get active voice count
int synth_engine_get_active_voice_count(SynthEngine *engine)
{
    int count = 0;
    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (engine->voices[i].active)
        {
            count++;
        }
    }
    return count;
}

// ============================================================================
// PARAMETER SETTERS
// ============================================================================

// Disyn Source
void synth_engine_set_disyn_algorithm(SynthEngine *engine, int algorithm)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        disyn_set_algorithm(engine->voices[i].disyn, algorithm);
    }
}

void synth_engine_set_disyn_param1(SynthEngine *engine, float value)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        disyn_set_param1(engine->voices[i].disyn, value);
    }
}

void synth_engine_set_disyn_param2(SynthEngine *engine, float value)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        disyn_set_param2(engine->voices[i].disyn, value);
    }
}

void synth_engine_set_disyn_level(SynthEngine *engine, float value)
{
    engine->disyn_level = value;
}

void synth_engine_set_noise_level(SynthEngine *engine, float value)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        sources_set_noise_level(engine->voices[i].sources, value);
    }
}

void synth_engine_set_dc_level(SynthEngine *engine, float value)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        sources_set_dc_level(engine->voices[i].sources, value);
    }
}

// Formants
void synth_engine_set_f1(SynthEngine *engine, float frequency)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        formant_bank_set_f1(engine->voices[i].formant_bank, frequency);
    }
}

void synth_engine_set_f2(SynthEngine *engine, float frequency)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        formant_bank_set_f2(engine->voices[i].formant_bank, frequency);
    }
}

void synth_engine_set_f3(SynthEngine *engine, float frequency)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        formant_bank_set_f3(engine->voices[i].formant_bank, frequency);
    }
}

void synth_engine_set_f4(SynthEngine *engine, float frequency)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        formant_bank_set_f4(engine->voices[i].formant_bank, frequency);
    }
}

// Vocal Modes
void synth_engine_set_nasal(SynthEngine *engine, bool enabled)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        formant_bank_set_nasal_enabled(engine->voices[i].formant_bank, enabled);
    }
}

void synth_engine_set_sing(SynthEngine *engine, bool enabled)
{
    engine->sing_enabled = enabled; // Global parameter
}

void synth_engine_set_shout(SynthEngine *engine, bool enabled)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        formant_bank_set_shout_enabled(engine->voices[i].formant_bank, enabled);
    }
}

void synth_engine_set_fry(SynthEngine *engine, bool enabled)
{
    engine->fry_enabled = enabled;
    // TODO: Modify Disyn to add f0/2 subharmonic
}

// Envelope
void synth_engine_set_attack(SynthEngine *engine, float value)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        envelope_set_attack(engine->voices[i].envelope, value);
    }
}

void synth_engine_set_release(SynthEngine *engine, float value)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        envelope_set_release(engine->voices[i].envelope, value);
    }
}

// Interface & Delay
void synth_engine_set_interface_type(SynthEngine *engine, int type)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        interface_set_type(engine->voices[i].interface, (InterfaceType)type);
    }
}

void synth_engine_set_intensity(SynthEngine *engine, float value)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        interface_set_intensity(engine->voices[i].interface, value);
    }
}

void synth_engine_set_tuning(SynthEngine *engine, float semitones)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        delay_lines_set_tuning(engine->voices[i].delay_lines, semitones);
    }
}

void synth_engine_set_ratio(SynthEngine *engine, float value)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        delay_lines_set_ratio(engine->voices[i].delay_lines, value);
    }
}

// Feedback
void synth_engine_set_delay1_feedback(SynthEngine *engine, float value)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        feedback_set_delay1_level(engine->voices[i].feedback, value);
    }
}

void synth_engine_set_delay2_feedback(SynthEngine *engine, float value)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        feedback_set_delay2_level(engine->voices[i].feedback, value);
    }
}

void synth_engine_set_filter_feedback(SynthEngine *engine, float value)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        feedback_set_filter_level(engine->voices[i].feedback, value);
    }
}

// Filter
void synth_engine_set_filter_frequency(SynthEngine *engine, float frequency)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        filter_set_frequency(engine->voices[i].filter, frequency);
    }
}

void synth_engine_set_filter_q(SynthEngine *engine, float q)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        filter_set_q(engine->voices[i].filter, q);
    }
}

void synth_engine_set_filter_shape(SynthEngine *engine, float shape)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        filter_set_shape(engine->voices[i].filter, shape);
    }
}

// Modulation
void synth_engine_set_lfo_frequency(SynthEngine *engine, float frequency)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        modulation_set_frequency(engine->voices[i].modulation, frequency);
    }
}

void synth_engine_set_am_fm_depth(SynthEngine *engine, float depth)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        modulation_set_depth(engine->voices[i].modulation, depth);
    }
}

// Reverb (not implemented)
void synth_engine_set_reverb_size(SynthEngine *engine, float value)
{
    (void)engine;
    (void)value;
    // Reverb cancelled; stub kept for API compatibility
}

void synth_engine_set_reverb_level(SynthEngine *engine, float value)
{
    (void)engine;
    (void)value;
    // Reverb cancelled; stub kept for API compatibility
}

// Output
void synth_engine_set_master_gain(SynthEngine *engine, float gain)
{
    engine->master_gain = gain;
}

// Debug/diagnostic toggles
void synth_engine_enable_disyn(SynthEngine *engine, bool enabled)
{
    engine->enable_disyn = enabled;
}

void synth_engine_enable_noise(SynthEngine *engine, bool enabled)
{
    engine->enable_noise = enabled;
}

void synth_engine_enable_feedback(SynthEngine *engine, bool enabled)
{
    engine->enable_feedback = enabled;
    if (!enabled)
    {
        // Clear feedback state for all voices
        for (int i = 0; i < MAX_VOICES; i++)
        {
            engine->voices[i].prev_delay1_out = 0.0f;
            engine->voices[i].prev_delay2_out = 0.0f;
            engine->voices[i].prev_filter_out = 0.0f;
            delay_lines_clear(engine->voices[i].delay_lines);
            // Clear DC blockers to reset any accumulated DC offset
            dc_blocker_init(&engine->voices[i].dc_blocker_disyn, DC_BLOCKER_R);
            dc_blocker_init(&engine->voices[i].dc_blocker_feedback, DC_BLOCKER_R);
        }
    }
}

void synth_engine_enable_formants(SynthEngine *engine, bool enabled)
{
    engine->enable_formants = enabled;
}

void synth_engine_enable_filter(SynthEngine *engine, bool enabled)
{
    engine->enable_filter = enabled;
    if (!enabled)
    {
        // Reset filter state for all voices
        for (int i = 0; i < MAX_VOICES; i++)
        {
            filter_reset(engine->voices[i].filter);
            engine->voices[i].prev_filter_out = 0.0f;
        }
    }
}

void synth_engine_hard_mute(SynthEngine *engine, bool enabled)
{
    engine->hard_mute = enabled;
    if (enabled)
    {
        // Clear all voice state when muting
        for (int i = 0; i < MAX_VOICES; i++)
        {
            engine->voices[i].prev_delay1_out = 0.0f;
            engine->voices[i].prev_delay2_out = 0.0f;
            engine->voices[i].prev_filter_out = 0.0f;
            delay_lines_clear(engine->voices[i].delay_lines);
            // Clear DC blockers to reset any accumulated DC offset
            dc_blocker_init(&engine->voices[i].dc_blocker_disyn, DC_BLOCKER_R);
            dc_blocker_init(&engine->voices[i].dc_blocker_feedback, DC_BLOCKER_R);
            filter_reset(engine->voices[i].filter);
        }
    }
}

bool synth_engine_is_noise_enabled(SynthEngine *engine)
{
    return engine->enable_noise;
}

bool synth_engine_is_disyn_enabled(SynthEngine *engine)
{
    return engine->enable_disyn;
}

bool synth_engine_is_feedback_enabled(SynthEngine *engine)
{
    return engine->enable_feedback;
}

bool synth_engine_is_formants_enabled(SynthEngine *engine)
{
    return engine->enable_formants;
}

bool synth_engine_is_filter_enabled(SynthEngine *engine)
{
    return engine->enable_filter;
}

bool synth_engine_is_hard_muted(SynthEngine *engine)
{
    return engine->hard_mute;
}
