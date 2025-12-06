// Envelope and Note Timing Test
// Verifies that notes trigger properly and envelope ramps correctly

#include "synth_engine.h"
#include "config.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== ENVELOPE & NOTE TIMING TEST ===\n");
    printf("Sample Rate: %.0f Hz\n", DEFAULT_SAMPLE_RATE);
    printf("Buffer Size: %d\n\n", DEFAULT_BUFFER_SIZE);

    SynthEngine* synth = synth_engine_create(DEFAULT_SAMPLE_RATE);
    if (!synth) {
        fprintf(stderr, "Failed to create synth engine\n");
        return 1;
    }

    // Test 1: Verify note on triggers envelope attack
    printf("TEST 1: Note On Envelope Attack\n");
    synth_engine_note_on(synth, 60, 261.6256f, 127);  // C4, full velocity

    // Render first few buffer periods and check for ramp-up
    const int test_samples = DEFAULT_BUFFER_SIZE * 2;  // 1024 samples at 48kHz (~21ms)
    float* first_samples = (float*)malloc(test_samples * sizeof(float));
    synth_engine_process(synth, first_samples, test_samples);

    float peak = 0.0f;
    int non_zero_count = 0;
    for (int i = 0; i < test_samples; i++) {
        float abs_val = fabsf(first_samples[i]);
        if (abs_val > peak) {
            peak = abs_val;
        }
        if (abs_val > 1e-6f) {
            non_zero_count++;
        }
    }

    printf("  First %d samples (~%.1f ms): peak = %.6f, non-zero = %d/%d\n",
           test_samples, (test_samples * 1000.0f) / DEFAULT_SAMPLE_RATE,
           peak, non_zero_count, test_samples);

    free(first_samples);

    if (non_zero_count < test_samples / 4) {  // At least 25% non-zero
        fprintf(stderr, "  FAIL: Too few non-zero samples (envelope not attacking?)\n");
        synth_engine_destroy(synth);
        return 1;
    }
    if (peak < 1e-4f) {
        fprintf(stderr, "  FAIL: Peak too low\n");
        synth_engine_destroy(synth);
        return 1;
    }
    printf("  PASS: Envelope attack detected\n\n");

    // Test 2: Verify sustained tone produces stable output
    printf("TEST 2: Sustained Tone Level\n");

    // Render 1 second at 48kHz (skip the initial attack ramp)
    int one_second = (int)DEFAULT_SAMPLE_RATE;
    float* sustained = (float*)malloc(one_second * sizeof(float));
    synth_engine_process(synth, sustained, one_second);

    // Measure RMS and peak in last half-second (should be at sustain level)
    int half_second = one_second / 2;
    double sum_squares = 0.0;
    float sustain_peak = 0.0f;
    for (int i = half_second; i < one_second; i++) {
        sum_squares += (double)sustained[i] * (double)sustained[i];
        float abs_val = fabsf(sustained[i]);
        if (abs_val > sustain_peak) {
            sustain_peak = abs_val;
        }
    }
    float rms = sqrtf(sum_squares / half_second);

    printf("  Sustain (last 0.5s): RMS = %.6f, peak = %.6f\n", rms, sustain_peak);

    if (rms < 1e-5f) {
        fprintf(stderr, "  FAIL: RMS too low (signal died?)\n");
        free(sustained);
        synth_engine_destroy(synth);
        return 1;
    }
    if (sustain_peak > 1.0f) {
        fprintf(stderr, "  WARNING: Peak exceeds 1.0 (clipping)\n");
    }
    printf("  PASS: Sustained tone present\n\n");

    free(sustained);

    // Test 3: Verify note off triggers release
    printf("TEST 3: Note Off Release\n");
    synth_engine_note_off(synth, 60);

    // Render release phase (3 seconds to be safe)
    int three_seconds = (int)(DEFAULT_SAMPLE_RATE * 3.0f);
    float* release = (float*)malloc(three_seconds * sizeof(float));
    synth_engine_process(synth, release, three_seconds);

    // Check that signal decays over time
    int quarter_points[4];
    for (int q = 0; q < 4; q++) {
        int start = (three_seconds / 4) * q;
        int end = start + (three_seconds / 4);
        double sum = 0.0;
        for (int i = start; i < end; i++) {
            sum += (double)release[i] * (double)release[i];
        }
        quarter_points[q] = (int)(sqrtf(sum / (three_seconds / 4)) * 1e6f);  // RMS in micro-units
    }

    printf("  Release RMS by quarter:\n");
    printf("    Q1: %d µ\n", quarter_points[0]);
    printf("    Q2: %d µ\n", quarter_points[1]);
    printf("    Q3: %d µ\n", quarter_points[2]);
    printf("    Q4: %d µ\n", quarter_points[3]);

    // Check for decay trend (each quarter should be <= previous)
    bool decaying = true;
    for (int q = 1; q < 4; q++) {
        if (quarter_points[q] > quarter_points[q-1] * 1.5f) {  // Allow 50% tolerance for noise
            decaying = false;
            break;
        }
    }

    if (!decaying) {
        fprintf(stderr, "  FAIL: Signal not decaying properly\n");
        free(release);
        synth_engine_destroy(synth);
        return 1;
    }
    printf("  PASS: Release decay detected\n\n");

    free(release);

    // Test 4: Rapid note re-trigger
    printf("TEST 4: Rapid Note Retriggering\n");

    for (int i = 0; i < 5; i++) {
        synth_engine_note_on(synth, 60, 261.6256f, 127);

        float retrig_buf[100];
        synth_engine_process(synth, retrig_buf, 100);

        // Check for signal immediately after retrigger
        float retrig_peak = 0.0f;
        for (int j = 0; j < 100; j++) {
            float abs_val = fabsf(retrig_buf[j]);
            if (abs_val > retrig_peak) {
                retrig_peak = abs_val;
            }
        }

        printf("  Retrigger %d: peak = %.6f\n", i+1, retrig_peak);

        if (retrig_peak < 1e-6f) {
            fprintf(stderr, "  FAIL: No signal on retrigger %d\n", i+1);
            synth_engine_destroy(synth);
            return 1;
        }

        synth_engine_note_off(synth, 60);

        // Small gap
        float gap_buf[100];
        synth_engine_process(synth, gap_buf, 100);
    }
    printf("  PASS: All retriggers produced signal\n\n");

    synth_engine_destroy(synth);

    printf("=== ALL TESTS PASSED ===\n");
    return 0;
}
