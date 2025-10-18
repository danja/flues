// pm_synth_engine.c
// Main physical modeling synthesizer engine implementation
// Translated from experiments/pm-synth/src/audio/PMSynthEngine.js

#include "pm_synth.h"
#include "dsp_modules.h"
#include "dsp_utils.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Engine structure holds all DSP modules
struct PMSynthEngine {
    float sample_rate;

    // DSP modules
    SourcesModule* sources;
    EnvelopeModule* envelope;
    InterfaceModule* interface;
    DelayLinesModule* delay_lines;
    FeedbackModule* feedback;
    FilterModule* filter;
    ModulationModule* modulation;
    ReverbModule* reverb;

    // DC blocker (on feedback path only)
    DCBlocker dc_blocker;

    // Current note state
    float current_frequency;
    bool note_active;
    bool is_playing;

    // Feedback memory (previous outputs)
    float prev_delay1_out;
    float prev_delay2_out;
    float prev_filter_out;
};

// Interface type names
static const char* INTERFACE_NAMES[] = {
    "Pluck", "Hit", "Reed", "Flute", "Brass", "Bow", "Bell", "Drum",
    "Crystal", "Vapor", "Quantum", "Plasma"
};

PMSynthEngine* pm_synth_create(float sample_rate) {
    PMSynthEngine* synth = (PMSynthEngine*)calloc(1, sizeof(PMSynthEngine));
    if (!synth) return NULL;

    synth->sample_rate = sample_rate;

    // Create all modules
    synth->sources = sources_create(sample_rate);
    synth->envelope = envelope_create(sample_rate);
    synth->interface = interface_create(sample_rate);
    synth->delay_lines = delay_lines_create(sample_rate);
    synth->feedback = feedback_create();
    synth->filter = filter_create(sample_rate);
    synth->modulation = modulation_create(sample_rate);
    synth->reverb = reverb_create(sample_rate);

    // Initialize DC blocker
    dc_blocker_init(&synth->dc_blocker);

    // Set default parameters (from constants.js)
    pm_synth_set_dc_level(synth, 0.0f);      // DC off by default
    pm_synth_set_noise_level(synth, 10.0f);  // 10% noise
    pm_synth_set_tone_level(synth, 0.0f);    // Tone off
    pm_synth_set_attack(synth, 10.0f);       // Fast attack
    pm_synth_set_release(synth, 50.0f);      // Medium release
    pm_synth_set_interface_type(synth, INTERFACE_REED);
    pm_synth_set_interface_intensity(synth, 50.0f);
    pm_synth_set_delay1_feedback(synth, 0.0f);  // Start with no feedback
    pm_synth_set_delay2_feedback(synth, 0.0f);  // Start with no feedback
    pm_synth_set_filter_feedback(synth, 0.0f);  // No filter feedback
    pm_synth_set_filter_frequency(synth, 70.0f);
    pm_synth_set_filter_q(synth, 20.0f);
    pm_synth_set_filter_shape(synth, 0.0f);
    pm_synth_set_lfo_frequency(synth, 30.0f);
    pm_synth_set_modulation_depth(synth, 50.0f); // Center = no modulation
    pm_synth_set_reverb_size(synth, 50.0f);
    pm_synth_set_reverb_level(synth, 30.0f);

    synth->current_frequency = 440.0f;
    synth->note_active = false;
    synth->is_playing = false;
    synth->prev_delay1_out = 0.0f;
    synth->prev_delay2_out = 0.0f;
    synth->prev_filter_out = 0.0f;

    return synth;
}

void pm_synth_destroy(PMSynthEngine* synth) {
    if (!synth) return;

    sources_destroy(synth->sources);
    envelope_destroy(synth->envelope);
    interface_destroy(synth->interface);
    delay_lines_destroy(synth->delay_lines);
    feedback_destroy(synth->feedback);
    filter_destroy(synth->filter);
    modulation_destroy(synth->modulation);
    reverb_destroy(synth->reverb);

    free(synth);
}

void pm_synth_process(PMSynthEngine* synth, float* output, int num_samples) {
    for (int i = 0; i < num_samples; i++) {
        // Early exit if not playing (like JavaScript)
        if (!synth->is_playing) {
            output[i] = 0.0f;
            continue;
        }

        // 1. Generate excitation signals (DC, noise, tone)
        float source_signal = sources_process(synth->sources);

        // 2. Apply envelope
        float enveloped_signal = envelope_process(synth->envelope, source_signal);

        // 3. Mix feedback from PREVIOUS cycle (like JavaScript)
        float feedback_signal = feedback_process(synth->feedback,
                                                synth->prev_delay1_out,
                                                synth->prev_delay2_out,
                                                synth->prev_filter_out);

        // 4. DC block the feedback (not the sources!)
        float clean_feedback = dc_blocker_process(&synth->dc_blocker, feedback_signal);

        // 5. Sum envelope and feedback, send through interface
        float interface_input = enveloped_signal + clean_feedback;
        float interface_output = interface_process(synth->interface, interface_input);

        // 6. Clamp and send to delay lines
        float clamped = hard_clip(interface_output);
        float delay1_out, delay2_out;
        delay_lines_process(synth->delay_lines, clamped, &delay1_out, &delay2_out);

        // 7. Mix delay outputs (simple average)
        float delay_mix = (delay1_out + delay2_out) * 0.5f;

        // 8. Apply filter
        float filter_out = filter_process(synth->filter, delay_mix);

        // 9. Apply modulation (AM)
        float modulated = modulation_process(synth->modulation, filter_out);

        // 10. Apply reverb
        float reverb_output = reverb_process(synth->reverb, modulated);

        // 11. Store outputs for next iteration feedback
        synth->prev_delay1_out = delay1_out;
        synth->prev_delay2_out = delay2_out;
        synth->prev_filter_out = filter_out;

        // 12. Final output with gain
        output[i] = hard_clip(reverb_output * 0.5f);

        // 13. Check if we can stop playing (voice tail finished)
        if (synth->envelope->envelope < 0.0001f &&
            fabsf(output[i]) < 0.00001f &&
            fabsf(synth->prev_delay1_out) < 0.00001f &&
            fabsf(synth->prev_delay2_out) < 0.00001f) {
            synth->is_playing = false;
        }
    }
}

void pm_synth_note_on(PMSynthEngine* synth, float frequency) {
    synth->current_frequency = frequency;
    synth->note_active = true;
    synth->is_playing = true;

    // Reset all state (like JavaScript noteOn)
    dc_blocker_init(&synth->dc_blocker);
    synth->prev_delay1_out = 0.0f;
    synth->prev_delay2_out = 0.0f;
    synth->prev_filter_out = 0.0f;

    // Set frequency for tone oscillator and delay lines
    sources_set_tone_frequency(synth->sources, frequency);
    delay_lines_set_frequency(synth->delay_lines, frequency);

    // Set gates
    envelope_set_gate(synth->envelope, true);
    interface_set_gate(synth->interface, true);
}

void pm_synth_note_off(PMSynthEngine* synth) {
    synth->note_active = false;
    envelope_set_gate(synth->envelope, false);
    interface_set_gate(synth->interface, false);
}

// Parameter setters (0-100 range, normalized internally)
void pm_synth_set_dc_level(PMSynthEngine* synth, float value) {
    sources_set_dc_level(synth->sources, value / 100.0f);
}

void pm_synth_set_noise_level(PMSynthEngine* synth, float value) {
    sources_set_noise_level(synth->sources, value / 100.0f);
}

void pm_synth_set_tone_level(PMSynthEngine* synth, float value) {
    sources_set_tone_level(synth->sources, value / 100.0f);
}

void pm_synth_set_attack(PMSynthEngine* synth, float value) {
    envelope_set_attack(synth->envelope, value / 100.0f);
}

void pm_synth_set_release(PMSynthEngine* synth, float value) {
    envelope_set_release(synth->envelope, value / 100.0f);
}

void pm_synth_set_interface_type(PMSynthEngine* synth, InterfaceType type) {
    interface_set_type(synth->interface, type);
}

void pm_synth_set_interface_intensity(PMSynthEngine* synth, float value) {
    interface_set_intensity(synth->interface, value / 100.0f);
}

void pm_synth_set_tuning(PMSynthEngine* synth, float value) {
    delay_lines_set_tuning(synth->delay_lines, value / 100.0f);
}

void pm_synth_set_ratio(PMSynthEngine* synth, float value) {
    delay_lines_set_ratio(synth->delay_lines, value / 100.0f);
}

void pm_synth_set_delay1_feedback(PMSynthEngine* synth, float value) {
    // Map 0-100 to 0-0.99 (like JavaScript)
    feedback_set_delay1(synth->feedback, (value / 100.0f) * 0.99f);
}

void pm_synth_set_delay2_feedback(PMSynthEngine* synth, float value) {
    // Map 0-100 to 0-0.99 (like JavaScript)
    feedback_set_delay2(synth->feedback, (value / 100.0f) * 0.99f);
}

void pm_synth_set_filter_feedback(PMSynthEngine* synth, float value) {
    // Map 0-100 to 0-0.99 (like JavaScript)
    feedback_set_filter(synth->feedback, (value / 100.0f) * 0.99f);
}

void pm_synth_set_filter_frequency(PMSynthEngine* synth, float value) {
    // Map 0-100 to 20Hz-20kHz exponentially
    float normalized = value / 100.0f;
    float freq = 20.0f * powf(1000.0f, normalized);
    filter_set_frequency(synth->filter, freq);
}

void pm_synth_set_filter_q(PMSynthEngine* synth, float value) {
    // Map 0-100 to 0.5-20
    float q = 0.5f + (value / 100.0f) * 19.5f;
    filter_set_q(synth->filter, q);
}

void pm_synth_set_filter_shape(PMSynthEngine* synth, float value) {
    filter_set_shape(synth->filter, value / 100.0f);
}

void pm_synth_set_lfo_frequency(PMSynthEngine* synth, float value) {
    // Map 0-100 to 0.1Hz-20Hz
    float freq = 0.1f + (value / 100.0f) * 19.9f;
    modulation_set_frequency(synth->modulation, freq);
}

void pm_synth_set_modulation_depth(PMSynthEngine* synth, float value) {
    // Map 0-100 to -1 to +1 (bipolar AM<->FM)
    float depth = (value / 50.0f) - 1.0f;
    modulation_set_depth(synth->modulation, depth);
}

void pm_synth_set_reverb_size(PMSynthEngine* synth, float value) {
    reverb_set_size(synth->reverb, value / 100.0f);
}

void pm_synth_set_reverb_level(PMSynthEngine* synth, float value) {
    reverb_set_level(synth->reverb, value / 100.0f);
}

// Utility functions
float pm_synth_midi_to_frequency(int midi_note) {
    return 440.0f * powf(2.0f, (midi_note - 69) / 12.0f);
}

const char* pm_synth_interface_name(InterfaceType type) {
    if (type >= 0 && type <= 11) {
        return INTERFACE_NAMES[type];
    }
    return "Unknown";
}
