// Polyphony smoke test: verifies 4-voice allocation/stealing and velocity scaling
#include "synth_engine.h"
#include "config.h"
#include "dsp_utils.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static float render_rms(SynthEngine* synth, int num_samples) {
    float* buffer = (float*)calloc(num_samples, sizeof(float));
    if (!buffer) {
        return -1.0f;
    }

    synth_engine_process(synth, buffer, num_samples);

    double sum = 0.0;
    for (int i = 0; i < num_samples; i++) {
        sum += (double)buffer[i] * (double)buffer[i];
    }

    free(buffer);
    return (float)sqrt(sum / num_samples);
}

static void drain_release(SynthEngine* synth, float seconds) {
    int samples = (int)(seconds * DEFAULT_SAMPLE_RATE);
    if (samples < 1) {
        return;
    }
    render_rms(synth, samples);
}

int main(void) {
    SynthEngine* synth = synth_engine_create(DEFAULT_SAMPLE_RATE);
    if (!synth) {
        fprintf(stderr, "Failed to create synth engine\n");
        return 1;
    }

    // --- Chord allocation and voice steal check ---
    const int chord_notes[] = {60, 64, 67, 71};
    const int chord_count = (int)(sizeof(chord_notes) / sizeof(chord_notes[0]));

    for (int i = 0; i < chord_count; i++) {
        float freq = midi_note_to_frequency(chord_notes[i]);
        synth_engine_note_on(synth, chord_notes[i], freq, 100);
    }

    int active = synth_engine_get_active_voice_count(synth);
    if (active != chord_count) {
        fprintf(stderr, "Expected %d active voices after chord, got %d\n", chord_count, active);
        synth_engine_destroy(synth);
        return 1;
    }

    float chord_rms = render_rms(synth, DEFAULT_BUFFER_SIZE * 8);
    if (chord_rms < 1e-4f) {
        fprintf(stderr, "Chord RMS too low (%.6f)\n", chord_rms);
        synth_engine_destroy(synth);
        return 1;
    }

    // Fifth note should steal the oldest (C4) but keep pool size constant
    synth_engine_note_on(synth, 72, midi_note_to_frequency(72), 110);
    active = synth_engine_get_active_voice_count(synth);
    if (active != MAX_VOICES) {
        fprintf(stderr, "Expected %d voices after 5th note, got %d\n", MAX_VOICES, active);
        synth_engine_destroy(synth);
        return 1;
    }

    // Note-off for an active voice should reduce count after release
    synth_engine_note_off(synth, 64);
    drain_release(synth, 0.5f);
    active = synth_engine_get_active_voice_count(synth);
    if (active >= MAX_VOICES) {
        fprintf(stderr, "Note-off did not free a voice (active=%d)\n", active);
        synth_engine_destroy(synth);
        return 1;
    }

    // Note-off for the oldest note should not drop count if it was stolen
    synth_engine_note_off(synth, 60);
    active = synth_engine_get_active_voice_count(synth);
    if (active < 1 || active > MAX_VOICES) {
        fprintf(stderr, "Voice count out of expected range after oldest note-off (active=%d)\n", active);
        synth_engine_destroy(synth);
        return 1;
    }

    // Clear all notes and ensure voices fully release
    const int all_notes[] = {60, 64, 67, 71, 72};
    for (int i = 0; i < (int)(sizeof(all_notes) / sizeof(all_notes[0])); i++) {
        synth_engine_note_off(synth, all_notes[i]);
    }
    drain_release(synth, 0.5f);  // let releases finish

    active = synth_engine_get_active_voice_count(synth);
    if (active != 0) {
        fprintf(stderr, "Voices did not fully release (active=%d)\n", active);
        synth_engine_destroy(synth);
        return 1;
    }

    // --- Duplicate note handling ---
    synth_engine_reset(synth);
    synth_engine_note_on(synth, 62, midi_note_to_frequency(62), 100);
    synth_engine_note_on(synth, 62, midi_note_to_frequency(62), 90);
    active = synth_engine_get_active_voice_count(synth);
    if (active < 2) {
        fprintf(stderr, "Duplicate note did not allocate two voices (active=%d)\n", active);
        synth_engine_destroy(synth);
        return 1;
    }
    synth_engine_note_off(synth, 62);
    drain_release(synth, 0.5f);
    active = synth_engine_get_active_voice_count(synth);
    if (active != 0) {
        fprintf(stderr, "Duplicate note-off did not release all voices (active=%d)\n", active);
        synth_engine_destroy(synth);
        return 1;
    }

    // --- Parameter broadcast sanity: noise level ---
    synth_engine_reset(synth);
    synth_engine_set_noise_level(synth, 0.0f);
    synth_engine_note_on(synth, 64, midi_note_to_frequency(64), 110);
    float rms_no_noise = render_rms(synth, DEFAULT_BUFFER_SIZE * 8);
    synth_engine_note_off(synth, 64);
    drain_release(synth, 0.2f);

    synth_engine_set_noise_level(synth, 0.6f);
    synth_engine_note_on(synth, 64, midi_note_to_frequency(64), 110);
    float rms_with_noise = render_rms(synth, DEFAULT_BUFFER_SIZE * 8);
    if (rms_with_noise <= rms_no_noise * 1.1f) {
        fprintf(stderr, "Noise level broadcast looks ineffective (no=%.6f, with=%.6f)\n",
                rms_no_noise, rms_with_noise);
        synth_engine_destroy(synth);
        return 1;
    }
    synth_engine_note_off(synth, 64);
    drain_release(synth, 0.2f);

    // --- Velocity scaling check ---
    synth_engine_reset(synth);

    synth_engine_note_on(synth, 60, midi_note_to_frequency(60), 30);
    float rms_soft = render_rms(synth, DEFAULT_BUFFER_SIZE * 6);
    synth_engine_note_off(synth, 60);
    render_rms(synth, (int)(DEFAULT_SAMPLE_RATE * 0.3f));  // drain release

    synth_engine_note_on(synth, 60, midi_note_to_frequency(60), 120);
    float rms_hard = render_rms(synth, DEFAULT_BUFFER_SIZE * 6);

    if (rms_soft < 0.0f || rms_hard < 0.0f) {
        fprintf(stderr, "Buffer allocation failed during RMS render\n");
        synth_engine_destroy(synth);
        return 1;
    }

    if (rms_hard <= rms_soft * 1.5f) {
        fprintf(stderr, "Velocity scaling too weak (soft=%.6f, hard=%.6f)\n", rms_soft, rms_hard);
        synth_engine_destroy(synth);
        return 1;
    }

    synth_engine_destroy(synth);
    printf("Polyphony smoke: chord RMS=%.6f, soft=%.6f, hard=%.6f (PASS)\n",
           chord_rms, rms_soft, rms_hard);
    return 0;
}
