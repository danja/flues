// delay_lines_module.c
// Dual delay lines with pitch tuning
// Translated from experiments/pm-synth/src/audio/modules/DelayLinesModule.js

#include "pm_synth.h"
#include "dsp_modules.h"
#include "dsp_utils.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

DelayLinesModule* delay_lines_create(float sample_rate) {
    DelayLinesModule* delays = (DelayLinesModule*)calloc(1, sizeof(DelayLinesModule));
    if (!delays) return NULL;

    delays->sample_rate = sample_rate;
    delays->buffer_size = MAX_DELAY_LENGTH;

    delays->buffer1 = (float*)calloc(delays->buffer_size, sizeof(float));
    delays->buffer2 = (float*)calloc(delays->buffer_size, sizeof(float));

    if (!delays->buffer1 || !delays->buffer2) {
        free(delays->buffer1);
        free(delays->buffer2);
        free(delays);
        return NULL;
    }

    delays->write_pos1 = 0.0f;
    delays->write_pos2 = 0.0f;
    delays->base_delay_samples = 100.0f;
    delays->tuning_offset = 0.0f;
    delays->ratio = 1.0f;

    return delays;
}

void delay_lines_destroy(DelayLinesModule* delays) {
    if (!delays) return;
    free(delays->buffer1);
    free(delays->buffer2);
    free(delays);
}

void delay_lines_process(DelayLinesModule* delays, float input, float* out1, float* out2) {
    // Calculate actual delay lengths
    float delay1_samples = delays->base_delay_samples + delays->tuning_offset;
    float delay2_samples = delay1_samples * delays->ratio;

    // Clamp to valid range
    if (delay1_samples < 1.0f) delay1_samples = 1.0f;
    if (delay2_samples < 1.0f) delay2_samples = 1.0f;
    if (delay1_samples >= delays->buffer_size) delay1_samples = delays->buffer_size - 1;
    if (delay2_samples >= delays->buffer_size) delay2_samples = delays->buffer_size - 1;

    // Write input to both delay lines
    int write_idx1 = (int)delays->write_pos1 % delays->buffer_size;
    int write_idx2 = (int)delays->write_pos2 % delays->buffer_size;
    delays->buffer1[write_idx1] = input;
    delays->buffer2[write_idx2] = input;

    // Read from delay lines with interpolation
    float read_pos1 = delays->write_pos1 - delay1_samples;
    float read_pos2 = delays->write_pos2 - delay2_samples;
    while (read_pos1 < 0) read_pos1 += delays->buffer_size;
    while (read_pos2 < 0) read_pos2 += delays->buffer_size;

    *out1 = delay_read_interpolated(delays->buffer1, delays->buffer_size, read_pos1);
    *out2 = delay_read_interpolated(delays->buffer2, delays->buffer_size, read_pos2);

    // Advance write positions
    delays->write_pos1 = (delays->write_pos1 + 1.0f);
    delays->write_pos2 = (delays->write_pos2 + 1.0f);
    if (delays->write_pos1 >= delays->buffer_size) delays->write_pos1 = 0.0f;
    if (delays->write_pos2 >= delays->buffer_size) delays->write_pos2 = 0.0f;
}

void delay_lines_set_frequency(DelayLinesModule* delays, float frequency) {
    if (frequency > 0.0f) {
        delays->base_delay_samples = delays->sample_rate / frequency;
    }
}

void delay_lines_set_tuning(DelayLinesModule* delays, float tuning) {
    // Map 0-1 to -12 to +12 semitones
    float semitones = (tuning - 0.5f) * 24.0f;
    float ratio = powf(2.0f, semitones / 12.0f);
    delays->tuning_offset = delays->base_delay_samples * (1.0f - ratio);
}

void delay_lines_set_ratio(DelayLinesModule* delays, float ratio) {
    // Map 0-1 to 0.5 to 2.0
    delays->ratio = 0.5f + ratio * 1.5f;
}
