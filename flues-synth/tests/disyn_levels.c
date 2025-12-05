/**
 * Disyn Level Analysis Test
 *
 * Tests each Disyn algorithm across parameter ranges to measure output levels.
 * Helps identify problematic amplitude scaling before integrating into full synth.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "dsp_modules.h"
#include "config.h"

#define TEST_SAMPLE_RATE 48000.0f
#define TEST_DURATION_SEC 0.5f
#define TEST_SAMPLES ((int)(TEST_SAMPLE_RATE * TEST_DURATION_SEC))
#define TEST_FREQUENCY 440.0f  // A4

// Algorithm names for reporting
static const char* algorithm_names[] = {
    "Dirichlet Pulse",
    "DSF Single",
    "DSF Double",
    "Tanh Square",
    "Tanh Saw",
    "PAF",
    "Modified FM"
};

typedef struct {
    float peak_positive;
    float peak_negative;
    float peak_absolute;
    float rms;
    int clip_count;  // Samples exceeding ±1.0
} LevelStats;

/**
 * Calculate level statistics for a buffer
 */
static void calculate_stats(const float* buffer, int num_samples, LevelStats* stats) {
    memset(stats, 0, sizeof(LevelStats));

    float sum_squares = 0.0f;

    for (int i = 0; i < num_samples; i++) {
        float sample = buffer[i];

        // Track peaks
        if (sample > stats->peak_positive) {
            stats->peak_positive = sample;
        }
        if (sample < stats->peak_negative) {
            stats->peak_negative = sample;
        }

        float abs_sample = fabsf(sample);
        if (abs_sample > stats->peak_absolute) {
            stats->peak_absolute = abs_sample;
        }

        // Count clipping
        if (abs_sample > 1.0f) {
            stats->clip_count++;
        }

        // RMS accumulation
        sum_squares += sample * sample;
    }

    stats->rms = sqrtf(sum_squares / num_samples);
}

/**
 * Test a single algorithm/parameter combination
 */
static void test_algorithm_params(int algorithm, float param1, float param2) {
    // Create oscillator
    DisynModule* osc = disyn_create(TEST_SAMPLE_RATE);
    if (!osc) {
        fprintf(stderr, "Failed to create Disyn oscillator\n");
        exit(1);
    }

    // Configure algorithm and parameters
    disyn_set_algorithm(osc, algorithm);
    disyn_set_param1(osc, param1);
    disyn_set_param2(osc, param2);

    // Allocate buffer
    float* buffer = (float*)malloc(TEST_SAMPLES * sizeof(float));
    if (!buffer) {
        fprintf(stderr, "Failed to allocate buffer\n");
        exit(1);
    }

    // Generate samples (disyn_process returns one sample at a time)
    for (int i = 0; i < TEST_SAMPLES; i++) {
        buffer[i] = disyn_process(osc, TEST_FREQUENCY);
    }

    // Calculate statistics
    LevelStats stats;
    calculate_stats(buffer, TEST_SAMPLES, &stats);

    // Report
    printf("  %-16s | P1=%5.2f P2=%5.2f | Peak: %+7.3f | RMS: %7.4f | Clips: %5d",
           algorithm_names[algorithm],
           param1, param2,
           stats.peak_absolute,
           stats.rms,
           stats.clip_count);

    // Warning flags
    if (stats.peak_absolute > 2.0f) {
        printf(" *** EXCESSIVE");
    } else if (stats.peak_absolute > 1.0f) {
        printf(" ** HIGH");
    } else if (stats.rms > 0.5f) {
        printf(" * LOUD");
    }
    printf("\n");

    // Cleanup
    free(buffer);
    disyn_destroy(osc);
}

/**
 * Test all algorithms with default parameters
 */
static void test_all_defaults(void) {
    printf("\n=== DISYN ALGORITHMS - DEFAULT PARAMETERS ===\n");
    printf("  Algorithm        | Params        | Peak        | RMS         | Clips     | Notes\n");
    printf("  -----------------|---------------|-------------|-------------|-----------|-------\n");

    // Algorithm-specific default parameters (from OscillatorModule.hpp)

    // 0: Dirichlet Pulse - harmonics=16, tilt=0
    test_algorithm_params(0, 0.25f, 0.2f);  // Maps to ~16 harmonics, 0dB tilt

    // 1: DSF Single - decay=0.5, ratio=1.0
    test_algorithm_params(1, 0.5f, 0.33f);

    // 2: DSF Double - decay=0.5, ratio=1.5
    test_algorithm_params(2, 0.52f, 0.36f);

    // 3: Tanh Square - drive=1.0, trim=0.7
    test_algorithm_params(3, 0.19f, 0.5f);

    // 4: Tanh Saw - drive=1.0, blend=0.5
    test_algorithm_params(4, 0.19f, 0.5f);

    // 5: PAF - ratio=2.0, bandwidth=500Hz
    test_algorithm_params(5, 0.25f, 0.15f);

    // 6: Modified FM - index=2.0, ratio=2.0
    test_algorithm_params(6, 0.25f, 0.30f);
}

/**
 * Test algorithms at parameter extremes
 */
static void test_extremes(void) {
    printf("\n=== DISYN ALGORITHMS - PARAMETER EXTREMES ===\n");
    printf("  Algorithm        | Params        | Peak        | RMS         | Clips     | Notes\n");
    printf("  -----------------|---------------|-------------|-------------|-----------|-------\n");

    // Test each algorithm at min/max param values
    for (int alg = 0; alg < 7; alg++) {
        // Min params
        test_algorithm_params(alg, 0.0f, 0.0f);

        // Max params
        test_algorithm_params(alg, 1.0f, 1.0f);

        // High param1, low param2
        test_algorithm_params(alg, 1.0f, 0.0f);

        // Low param1, high param2
        test_algorithm_params(alg, 0.0f, 1.0f);
    }
}

/**
 * Test frequency dependency
 */
static void test_frequencies(void) {
    printf("\n=== FREQUENCY DEPENDENCY (Algorithm 0 - Dirichlet) ===\n");
    printf("  Frequency  | Peak        | RMS         | Clips\n");
    printf("  -----------|-------------|-------------|-------\n");

    float frequencies[] = {55.0f, 110.0f, 220.0f, 440.0f, 880.0f, 1760.0f, 3520.0f};
    int num_freqs = sizeof(frequencies) / sizeof(frequencies[0]);

    for (int i = 0; i < num_freqs; i++) {
        DisynModule* osc = disyn_create(TEST_SAMPLE_RATE);
        disyn_set_algorithm(osc, 0);  // Dirichlet
        disyn_set_param1(osc, 0.5f);  // ~32 harmonics
        disyn_set_param2(osc, 0.2f);  // 0dB tilt

        float* buffer = (float*)malloc(TEST_SAMPLES * sizeof(float));
        for (int j = 0; j < TEST_SAMPLES; j++) {
            buffer[j] = disyn_process(osc, frequencies[i]);
        }

        LevelStats stats;
        calculate_stats(buffer, TEST_SAMPLES, &stats);

        printf("  %7.1f Hz | %+7.3f     | %7.4f     | %5d\n",
               frequencies[i], stats.peak_absolute, stats.rms, stats.clip_count);

        free(buffer);
        disyn_destroy(osc);
    }
}

/**
 * Test with synth-realistic scaling
 */
static void test_with_scaling(void) {
    printf("\n=== ALGORITHMS WITH SYNTH SCALING (disyn_level=0.5, global_pad=0.5) ===\n");
    printf("  Algorithm        | Params        | Peak        | RMS         | Effective Peak\n");
    printf("  -----------------|---------------|-------------|-------------|---------------\n");

    const float DISYN_LEVEL = 0.5f;
    const float GLOBAL_PAD = 0.5f;
    const float COMBINED_SCALE = DISYN_LEVEL * GLOBAL_PAD;  // 0.25

    // Test defaults with scaling applied
    struct {
        int alg;
        float p1;
        float p2;
    } configs[] = {
        {0, 0.25f, 0.2f},  // Dirichlet
        {1, 0.5f, 0.33f},  // DSF Single
        {2, 0.52f, 0.36f}, // DSF Double
        {3, 0.19f, 0.5f},  // Tanh Square
        {4, 0.19f, 0.5f},  // Tanh Saw
        {5, 0.25f, 0.15f}, // PAF
        {6, 0.25f, 0.30f}  // Modified FM
    };

    for (int i = 0; i < 7; i++) {
        DisynModule* osc = disyn_create(TEST_SAMPLE_RATE);
        disyn_set_algorithm(osc, configs[i].alg);
        disyn_set_param1(osc, configs[i].p1);
        disyn_set_param2(osc, configs[i].p2);

        float* buffer = (float*)malloc(TEST_SAMPLES * sizeof(float));
        for (int j = 0; j < TEST_SAMPLES; j++) {
            buffer[j] = disyn_process(osc, TEST_FREQUENCY);
        }

        LevelStats stats;
        calculate_stats(buffer, TEST_SAMPLES, &stats);

        float effective_peak = stats.peak_absolute * COMBINED_SCALE;

        printf("  %-16s | P1=%5.2f P2=%5.2f | %+7.3f     | %7.4f     | %+7.3f",
               algorithm_names[configs[i].alg],
               configs[i].p1, configs[i].p2,
               stats.peak_absolute, stats.rms, effective_peak);

        if (effective_peak > 1.0f) {
            printf(" *** STILL HIGH");
        }
        printf("\n");

        free(buffer);
        disyn_destroy(osc);
    }

    printf("\nNote: Effective peak = raw peak × disyn_level × global_pad\n");
    printf("      Does not include formant makeup (3.0×) or feedback gain\n");
}

int main(void) {
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  DISYN WAVEFORM LEVEL ANALYSIS\n");
    printf("  Sample Rate: %.0f Hz\n", TEST_SAMPLE_RATE);
    printf("  Test Duration: %.2f seconds (%d samples)\n", TEST_DURATION_SEC, TEST_SAMPLES);
    printf("  Test Frequency: %.1f Hz\n", TEST_FREQUENCY);
    printf("═══════════════════════════════════════════════════════════════\n");

    test_all_defaults();
    test_extremes();
    test_frequencies();
    test_with_scaling();

    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("  Test complete. Review levels marked with *, **, or ***\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    return 0;
}
