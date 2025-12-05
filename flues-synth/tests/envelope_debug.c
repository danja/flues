// Debug test to trace through envelope and signal generation step-by-step

#include "synth_engine.h"
#include "dsp_modules.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== ENVELOPE DEBUG TEST ===\n\n");

    SynthEngine* synth = synth_engine_create(DEFAULT_SAMPLE_RATE);
    if (!synth) {
        fprintf(stderr, "Failed to create synth engine\n");
        return 1;
    }

    printf("Step 1: Synth created\n");
    printf("  Sample rate: %.0f Hz\n", DEFAULT_SAMPLE_RATE);
    printf("  Disyn enabled: %d\n", synth_engine_is_disyn_enabled(synth));
    printf("  Noise enabled: %d\n", synth_engine_is_noise_enabled(synth));
    printf("  Formants enabled: %d\n", synth_engine_is_formants_enabled(synth));
    printf("  Filter enabled: %d\n", synth_engine_is_filter_enabled(synth));
    printf("  Feedback enabled: %d\n", synth_engine_is_feedback_enabled(synth));
    printf("\n");

    printf("Step 2: Triggering Note On (C4 = 261.6256 Hz)\n");
    synth_engine_note_on(synth, 60, 261.6256f);
    printf("  Note triggered\n\n");

    printf("Step 3: Processing first 10 samples\n");
    float buffer[10];
    synth_engine_process(synth, buffer, 10);

    for (int i = 0; i < 10; i++) {
        printf("  Sample %2d: %+.8f\n", i, buffer[i]);
    }
    printf("\n");

    // Check if all zeros
    int zero_count = 0;
    for (int i = 0; i < 10; i++) {
        if (buffer[i] == 0.0f) {
            zero_count++;
        }
    }

    if (zero_count == 10) {
        fprintf(stderr, "ERROR: All samples are zero!\n");
        fprintf(stderr, "\nPossible causes:\n");
        fprintf(stderr, "  - Voice not activated\n");
        fprintf(stderr, "  - Envelope not transitioning to ATTACK\n");
        fprintf(stderr, "  - Disyn not generating signal\n");
        fprintf(stderr, "  - DC blockers killing signal\n");
        fprintf(stderr, "  - Master gain or levels too low\n");

        synth_engine_destroy(synth);
        return 1;
    }

    printf("Step 4: Processing more samples to check ramp\n");
    float buffer2[100];
    synth_engine_process(synth, buffer2, 100);

    float min = buffer2[0];
    float max = buffer2[0];
    for (int i = 1; i < 100; i++) {
        if (buffer2[i] < min) min = buffer2[i];
        if (buffer2[i] > max) max = buffer2[i];
    }

    printf("  Samples 11-110: min = %+.8f, max = %+.8f\n", min, max);
    printf("\n");

    if (max - min < 1e-6f) {
        fprintf(stderr, "ERROR: No signal variation (flat line)\n");
        synth_engine_destroy(synth);
        return 1;
    }

    printf("SUCCESS: Signal is being generated\n");
    synth_engine_destroy(synth);
    return 0;
}
