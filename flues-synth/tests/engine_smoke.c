// Simple smoke test: render a buffer with default settings and confirm energy > 0
#include "synth_engine.h"
#include "config.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    SynthEngine* synth = synth_engine_create(DEFAULT_SAMPLE_RATE);
    if (!synth) {
        fprintf(stderr, "Failed to create synth engine\n");
        return 1;
    }

    // Trigger a middle C note
    const int note = 60;
    const float freq = 261.6256f;
    synth_engine_note_on(synth, note, freq);

    const int frames = DEFAULT_BUFFER_SIZE * 4;  // a few periods to build up
    float* buffer = (float*)calloc(frames, sizeof(float));
    if (!buffer) {
        fprintf(stderr, "Failed to allocate buffer\n");
        synth_engine_destroy(synth);
        return 1;
    }

    synth_engine_process(synth, buffer, frames);

    double sum = 0.0;
    for (int i = 0; i < frames; i++) {
        sum += (double)buffer[i] * (double)buffer[i];
    }
    const double rms = sqrt(sum / frames);

    free(buffer);
    synth_engine_destroy(synth);

    printf("Engine smoke RMS: %.6f\n", rms);
    if (rms < 1e-4) {
        fprintf(stderr, "RMS too low; signal path may be silent\n");
        return 1;
    }

    return 0;
}
