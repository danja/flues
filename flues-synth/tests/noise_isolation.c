// Noise Isolation Test
// Systematically tests all parameter combinations to isolate broadband noise source

#include "synth_engine.h"
#include "config.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_DURATION 1.0f  // 1 second per test
#define TEST_SAMPLES ((int)(DEFAULT_SAMPLE_RATE * TEST_DURATION))
#define RMS_THRESHOLD 0.001f  // Minimum RMS to consider "signal present"

typedef struct {
    float rms;
    float peak;
    float dc_offset;
    int clip_count;
} SignalStats;

static void calculate_stats(float* buffer, int num_samples, SignalStats* stats) {
    double sum_squares = 0.0;
    double sum = 0.0;
    float peak = 0.0f;
    int clip_count = 0;

    for (int i = 0; i < num_samples; i++) {
        float sample = buffer[i];
        sum += sample;
        sum_squares += (double)sample * (double)sample;

        float abs_val = fabsf(sample);
        if (abs_val > peak) {
            peak = abs_val;
        }
        if (abs_val > 0.99f) {
            clip_count++;
        }
    }

    stats->rms = sqrtf(sum_squares / num_samples);
    stats->peak = peak;
    stats->dc_offset = sum / num_samples;
    stats->clip_count = clip_count;
}

static void print_stats(const char* test_name, SignalStats* stats) {
    printf("%-50s RMS: %.6f  Peak: %.6f  DC: %.6f  Clips: %d\n",
           test_name, stats->rms, stats->peak, stats->dc_offset, stats->clip_count);

    if (stats->rms > 0.1f) {
        printf("  ⚠ HIGH RMS (>0.1) - potential noise source!\n");
    }
    if (stats->clip_count > 0) {
        printf("  ⚠ CLIPPING DETECTED (%d samples)\n", stats->clip_count);
    }
    if (fabsf(stats->dc_offset) > 0.01f) {
        printf("  ⚠ DC OFFSET DETECTED (%.6f)\n", stats->dc_offset);
    }
}

static void render_test(SynthEngine* synth, float* buffer, int note, float frequency) {
    // Trigger note
    synth_engine_note_on(synth, note, frequency);

    // Render buffer
    synth_engine_process(synth, buffer, TEST_SAMPLES);
}

int main(void) {
    printf("=== NOISE ISOLATION TEST ===\n");
    printf("Sample Rate: %.0f Hz\n", DEFAULT_SAMPLE_RATE);
    printf("Test Duration: %.1f seconds (%d samples)\n\n", TEST_DURATION, TEST_SAMPLES);

    SynthEngine* synth = synth_engine_create(DEFAULT_SAMPLE_RATE);
    if (!synth) {
        fprintf(stderr, "Failed to create synth engine\n");
        return 1;
    }

    float* buffer = (float*)malloc(TEST_SAMPLES * sizeof(float));
    if (!buffer) {
        fprintf(stderr, "Failed to allocate buffer\n");
        synth_engine_destroy(synth);
        return 1;
    }

    SignalStats stats;
    int test_failures = 0;

    // Test 1: Baseline - All defaults (C4, 261.6 Hz)
    printf("TEST 1: Baseline Configuration (all modules enabled)\n");
    printf("--------------------------------------------------------\n");
    memset(buffer, 0, TEST_SAMPLES * sizeof(float));
    render_test(synth, buffer, 60, 261.6256f);
    calculate_stats(buffer, TEST_SAMPLES, &stats);
    print_stats("Baseline (all enabled)", &stats);
    printf("\n");

    // Test 2: Disyn only (disable noise, feedback, formants, filter)
    printf("TEST 2: Disyn Oscillator Only\n");
    printf("--------------------------------------------------------\n");

    for (int alg = 0; alg < 7; alg++) {
        synth_engine_enable_noise(synth, false);
        synth_engine_enable_feedback(synth, false);
        synth_engine_enable_formants(synth, false);
        synth_engine_enable_filter(synth, false);
        synth_engine_enable_disyn(synth, true);

        synth_engine_set_disyn_algorithm(synth, alg);
        synth_engine_set_disyn_param1(synth, 0.5f);
        synth_engine_set_disyn_param2(synth, 0.5f);
        synth_engine_set_disyn_level(synth, 0.8f);

        memset(buffer, 0, TEST_SAMPLES * sizeof(float));
        render_test(synth, buffer, 60, 261.6256f);
        calculate_stats(buffer, TEST_SAMPLES, &stats);

        char test_name[100];
        snprintf(test_name, sizeof(test_name), "  Disyn Algorithm %d (isolated)", alg);
        print_stats(test_name, &stats);

        if (stats.rms > 0.1f) {
            test_failures++;
        }
    }
    printf("\n");

    // Test 3: Noise only (disable Disyn, feedback, formants, filter)
    printf("TEST 3: Noise Source Only\n");
    printf("--------------------------------------------------------\n");
    synth_engine_enable_disyn(synth, false);
    synth_engine_enable_noise(synth, true);
    synth_engine_enable_feedback(synth, false);
    synth_engine_enable_formants(synth, false);
    synth_engine_enable_filter(synth, false);

    float noise_levels[] = {0.05f, 0.15f, 0.25f, 0.5f, 1.0f};
    for (int i = 0; i < 5; i++) {
        synth_engine_set_noise_level(synth, noise_levels[i]);

        memset(buffer, 0, TEST_SAMPLES * sizeof(float));
        render_test(synth, buffer, 60, 261.6256f);
        calculate_stats(buffer, TEST_SAMPLES, &stats);

        char test_name[100];
        snprintf(test_name, sizeof(test_name), "  Noise level %.2f (isolated)", noise_levels[i]);
        print_stats(test_name, &stats);

        if (stats.rms > noise_levels[i] * 1.5f) {  // Allow 50% headroom
            printf("  ⚠ RMS exceeds expected (%.6f > %.6f)\n", stats.rms, noise_levels[i] * 1.5f);
            test_failures++;
        }
    }
    printf("\n");

    // Test 4: Formants (with Disyn, no feedback/filter)
    printf("TEST 4: Disyn + Formants (no feedback)\n");
    printf("--------------------------------------------------------\n");
    synth_engine_enable_disyn(synth, true);
    synth_engine_enable_noise(synth, true);
    synth_engine_enable_formants(synth, true);
    synth_engine_enable_feedback(synth, false);
    synth_engine_enable_filter(synth, false);

    synth_engine_set_disyn_level(synth, 0.8f);
    synth_engine_set_noise_level(synth, 0.15f);

    // Test different formant configurations
    struct {
        float f1, f2, f3, f4;
        const char* name;
    } formant_tests[] = {
        {500.0f, 1500.0f, 2500.0f, 3500.0f, "Neutral vowel"},
        {270.0f, 2300.0f, 3000.0f, 3500.0f, "/i/ (bee)"},
        {700.0f, 1500.0f, 2500.0f, 3500.0f, "/æ/ (bat)"},
        {300.0f, 600.0f, 2500.0f, 3500.0f, "/u/ (boot)"},
    };

    for (int i = 0; i < 4; i++) {
        synth_engine_set_f1(synth, formant_tests[i].f1);
        synth_engine_set_f2(synth, formant_tests[i].f2);
        synth_engine_set_f3(synth, formant_tests[i].f3);
        synth_engine_set_f4(synth, formant_tests[i].f4);

        memset(buffer, 0, TEST_SAMPLES * sizeof(float));
        render_test(synth, buffer, 60, 261.6256f);
        calculate_stats(buffer, TEST_SAMPLES, &stats);

        char test_name[100];
        snprintf(test_name, sizeof(test_name), "  Formants: %s", formant_tests[i].name);
        print_stats(test_name, &stats);

        if (stats.rms > 0.15f) {
            test_failures++;
        }
    }
    printf("\n");

    // Test 5: Feedback loop (with delays, no filter)
    printf("TEST 5: Feedback Loop Levels\n");
    printf("--------------------------------------------------------\n");
    synth_engine_enable_disyn(synth, true);
    synth_engine_enable_noise(synth, true);
    synth_engine_enable_formants(synth, true);
    synth_engine_enable_feedback(synth, true);
    synth_engine_enable_filter(synth, false);

    synth_engine_set_f1(synth, 500.0f);
    synth_engine_set_f2(synth, 1500.0f);
    synth_engine_set_f3(synth, 2500.0f);
    synth_engine_set_f4(synth, 3500.0f);

    float feedback_levels[] = {0.0f, 0.1f, 0.2f, 0.4f, 0.6f};
    for (int i = 0; i < 5; i++) {
        synth_engine_set_delay1_feedback(synth, feedback_levels[i]);
        synth_engine_set_delay2_feedback(synth, feedback_levels[i]);
        synth_engine_set_filter_feedback(synth, feedback_levels[i] * 0.5f);

        memset(buffer, 0, TEST_SAMPLES * sizeof(float));
        render_test(synth, buffer, 60, 261.6256f);
        calculate_stats(buffer, TEST_SAMPLES, &stats);

        char test_name[100];
        snprintf(test_name, sizeof(test_name), "  Feedback level %.2f", feedback_levels[i]);
        print_stats(test_name, &stats);

        if (stats.rms > 0.3f || stats.clip_count > 0) {
            test_failures++;
        }
    }
    printf("\n");

    // Test 6: Filter sweep
    printf("TEST 6: Filter Configurations\n");
    printf("--------------------------------------------------------\n");
    synth_engine_enable_filter(synth, true);
    synth_engine_enable_feedback(synth, true);
    synth_engine_set_delay1_feedback(synth, 0.2f);
    synth_engine_set_delay2_feedback(synth, 0.2f);
    synth_engine_set_filter_feedback(synth, 0.1f);

    struct {
        float freq;
        float q;
        float shape;
        const char* name;
    } filter_tests[] = {
        {500.0f, 1.0f, 0.0f, "LP 500Hz Q=1"},
        {2000.0f, 1.0f, 0.0f, "LP 2kHz Q=1"},
        {2000.0f, 5.0f, 0.0f, "LP 2kHz Q=5"},
        {2000.0f, 1.0f, 0.5f, "BP 2kHz Q=1"},
        {2000.0f, 5.0f, 0.5f, "BP 2kHz Q=5"},
        {2000.0f, 1.0f, 1.0f, "HP 2kHz Q=1"},
    };

    for (int i = 0; i < 6; i++) {
        synth_engine_set_filter_frequency(synth, filter_tests[i].freq);
        synth_engine_set_filter_q(synth, filter_tests[i].q);
        synth_engine_set_filter_shape(synth, filter_tests[i].shape);

        memset(buffer, 0, TEST_SAMPLES * sizeof(float));
        render_test(synth, buffer, 60, 261.6256f);
        calculate_stats(buffer, TEST_SAMPLES, &stats);

        char test_name[100];
        snprintf(test_name, sizeof(test_name), "  Filter: %s", filter_tests[i].name);
        print_stats(test_name, &stats);

        if (stats.rms > 0.3f) {
            test_failures++;
        }
    }
    printf("\n");

    // Test 7: Interface types
    printf("TEST 7: Interface Types\n");
    printf("--------------------------------------------------------\n");
    synth_engine_set_filter_frequency(synth, 2000.0f);
    synth_engine_set_filter_q(synth, 1.0f);
    synth_engine_set_filter_shape(synth, 0.0f);

    const char* interface_names[] = {
        "Pluck", "Hit", "Reed", "Flute", "Brass", "Bow",
        "Bell", "Drum", "Crystal", "Vapor", "Quantum", "Plasma"
    };

    for (int i = 0; i < 12; i++) {
        synth_engine_set_interface_type(synth, i);
        synth_engine_set_intensity(synth, 0.5f);

        memset(buffer, 0, TEST_SAMPLES * sizeof(float));
        render_test(synth, buffer, 60, 261.6256f);
        calculate_stats(buffer, TEST_SAMPLES, &stats);

        char test_name[100];
        snprintf(test_name, sizeof(test_name), "  Interface: %s", interface_names[i]);
        print_stats(test_name, &stats);

        if (stats.rms > 0.3f) {
            test_failures++;
        }
    }
    printf("\n");

    // Test 8: Vocal modes
    printf("TEST 8: Vocal Modes\n");
    printf("--------------------------------------------------------\n");
    synth_engine_set_interface_type(synth, 2);  // Reed

    struct {
        bool nasal, sing, shout;
        const char* name;
    } vocal_tests[] = {
        {false, false, false, "None"},
        {true, false, false, "Nasal"},
        {false, true, false, "Sing (vibrato)"},
        {false, false, true, "Shout"},
        {true, true, false, "Nasal + Sing"},
        {true, false, true, "Nasal + Shout"},
        {true, true, true, "All modes"},
    };

    for (int i = 0; i < 7; i++) {
        synth_engine_set_nasal(synth, vocal_tests[i].nasal);
        synth_engine_set_sing(synth, vocal_tests[i].sing);
        synth_engine_set_shout(synth, vocal_tests[i].shout);

        memset(buffer, 0, TEST_SAMPLES * sizeof(float));
        render_test(synth, buffer, 60, 261.6256f);
        calculate_stats(buffer, TEST_SAMPLES, &stats);

        char test_name[100];
        snprintf(test_name, sizeof(test_name), "  Vocal modes: %s", vocal_tests[i].name);
        print_stats(test_name, &stats);

        if (stats.rms > 0.35f) {  // Shout mode can boost
            test_failures++;
        }
    }
    printf("\n");

    // Test 9: Different note pitches
    printf("TEST 9: Different Pitches\n");
    printf("--------------------------------------------------------\n");
    synth_engine_set_nasal(synth, false);
    synth_engine_set_sing(synth, false);
    synth_engine_set_shout(synth, false);

    struct {
        int note;
        float freq;
        const char* name;
    } pitch_tests[] = {
        {36, 65.41f, "C2 (65 Hz)"},
        {48, 130.81f, "C3 (131 Hz)"},
        {60, 261.63f, "C4 (262 Hz)"},
        {72, 523.25f, "C5 (523 Hz)"},
        {84, 1046.50f, "C6 (1047 Hz)"},
    };

    for (int i = 0; i < 5; i++) {
        memset(buffer, 0, TEST_SAMPLES * sizeof(float));
        render_test(synth, buffer, pitch_tests[i].note, pitch_tests[i].freq);
        calculate_stats(buffer, TEST_SAMPLES, &stats);

        char test_name[100];
        snprintf(test_name, sizeof(test_name), "  Pitch: %s", pitch_tests[i].name);
        print_stats(test_name, &stats);

        if (stats.rms > 0.3f) {
            test_failures++;
        }
    }
    printf("\n");

    // Summary
    printf("========================================\n");
    printf("SUMMARY\n");
    printf("========================================\n");
    if (test_failures == 0) {
        printf("✓ All tests passed - no excessive noise detected\n");
    } else {
        printf("⚠ %d test(s) showed excessive RMS or clipping\n", test_failures);
        printf("Review warnings above to identify noise sources\n");
    }
    printf("\n");

    free(buffer);
    synth_engine_destroy(synth);

    return (test_failures > 0) ? 1 : 0;
}
