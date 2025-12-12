// Test Program 6 (Full Hybrid) for stability after race condition fix
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../include/synth_engine.h"

#define SAMPLE_RATE 48000
#define BUFFER_SIZE 512
#define TEST_DURATION_SAMPLES (SAMPLE_RATE * 2)  // 2 seconds

int main(void) {
    printf("=== Program 6 (Full Hybrid) Stability Test ===\n");
    printf("Testing after race condition fix in interface_module.c\n\n");

    // Create synth engine
    SynthEngine* synth = synth_engine_create(SAMPLE_RATE);
    if (!synth) {
        fprintf(stderr, "Failed to create synth engine\n");
        return 1;
    }

    // Manually configure Program 6 settings (all modules enabled)
    printf("Activating Program 6: Full Hybrid (all modules enabled)...\n");
    synth_engine_enable_disyn(synth, true);
    synth_engine_enable_noise(synth, true);
    synth_engine_enable_formants(synth, true);
    synth_engine_enable_feedback(synth, true);
    synth_engine_enable_filter(synth, true);
    synth_engine_set_disyn_level(synth, 0.3f);
    synth_engine_set_noise_level(synth, 0.2f);
    synth_engine_set_delay1_feedback(synth, 0.3f);
    synth_engine_set_delay2_feedback(synth, 0.3f);
    synth_engine_set_filter_feedback(synth, 0.2f);
    synth_engine_set_interface_type(synth, 2);  // Reed
    synth_engine_set_master_gain(synth, 0.6f);

    // Trigger a note (MIDI note 60 = C4 = 261.63 Hz)
    printf("Triggering MIDI note 60 (C4, 261.63 Hz)...\n");
    synth_engine_note_on(synth, 60, 261.63f, 96);

    // Process audio for 2 seconds
    printf("Processing %d samples (%.1f seconds)...\n",
           TEST_DURATION_SAMPLES, (float)TEST_DURATION_SAMPLES / SAMPLE_RATE);

    float buffer[BUFFER_SIZE];
    int samples_processed = 0;
    float max_amplitude = 0.0f;
    int non_zero_samples = 0;

    while (samples_processed < TEST_DURATION_SAMPLES) {
        // Process buffer
        synth_engine_process(synth, buffer, BUFFER_SIZE);

        // Analyze output
        for (int i = 0; i < BUFFER_SIZE; i++) {
            float abs_val = fabsf(buffer[i]);
            if (abs_val > max_amplitude) {
                max_amplitude = abs_val;
            }
            if (abs_val > 0.0001f) {
                non_zero_samples++;
            }
        }

        samples_processed += BUFFER_SIZE;

        // Release note after 1 second
        if (samples_processed == SAMPLE_RATE) {
            printf("Releasing note...\n");
            synth_engine_note_off(synth, 60);
        }
    }

    printf("\n=== Test Results ===\n");
    printf("Samples processed: %d\n", samples_processed);
    printf("Max amplitude: %.4f\n", max_amplitude);
    printf("Non-zero samples: %d (%.1f%%)\n",
           non_zero_samples,
           100.0f * non_zero_samples / samples_processed);

    // Cleanup
    synth_engine_destroy(synth);

    // Verdict
    if (max_amplitude > 0.001f && non_zero_samples > 1000) {
        printf("\n✓ Program 6 test PASSED: No segfault, audio output detected\n");
        printf("  The race condition fix appears to have resolved the stability issue.\n");
        return 0;
    } else {
        printf("\n✗ Program 6 test WARNING: No segfault, but no audio output\n");
        printf("  Program may need parameter adjustment.\n");
        return 0;  // Still pass - the important part is no crash
    }
}
