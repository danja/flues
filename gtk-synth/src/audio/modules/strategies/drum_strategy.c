// drum_strategy.c
// Drum interface: Energy accumulator with noisy drive
// Translated from reference/modules/interface/strategies/DrumStrategy.js

#include "interface_strategy.h"
#include "dsp_utils.h"
#include <stdlib.h>
#include <math.h>

typedef struct {
    float drum_energy;
} DrumImpl;

static float drum_process(InterfaceStrategy* self, float input) {
    DrumImpl* impl = (DrumImpl*)self->impl_data;

    const float drive = 1.2f + self->intensity * 2.2f;
    const float noise = white_noise() * (0.02f + self->intensity * 0.06f);

    // Accumulate energy with decay
    impl->drum_energy = impl->drum_energy * (0.7f - self->intensity * 0.2f) +
                        fabsf(input) * (0.6f + self->intensity * 0.7f);

    const float hit = tanhf(input * drive) + noise;
    const float output = hit * (0.4f + self->intensity * 0.4f) +
                        copysignf(fminf(0.8f, impl->drum_energy * 0.6f), hit);

    return fmaxf(-1.0f, fminf(1.0f, output));
}

static void drum_reset(InterfaceStrategy* self) {
    DrumImpl* impl = (DrumImpl*)self->impl_data;
    impl->drum_energy = 0.0f;
}

static void drum_set_intensity(InterfaceStrategy* self, float intensity) {
    self->intensity = intensity;
}

static void drum_set_gate(InterfaceStrategy* self, bool gate) {
    if (gate) {
        drum_reset(self);
    }
}

static void drum_destroy(InterfaceStrategy* self) {
    if (self) {
        free(self->impl_data);
        free(self);
    }
}

static InterfaceStrategyVTable drum_vtable = {
    .process = drum_process,
    .reset = drum_reset,
    .set_intensity = drum_set_intensity,
    .set_gate = drum_set_gate,
    .destroy = drum_destroy
};

InterfaceStrategy* drum_strategy_create(float sample_rate) {
    (void)sample_rate;  // Unused
    InterfaceStrategy* strategy = (InterfaceStrategy*)calloc(1, sizeof(InterfaceStrategy));
    if (!strategy) return NULL;

    DrumImpl* impl = (DrumImpl*)calloc(1, sizeof(DrumImpl));
    if (!impl) {
        free(strategy);
        return NULL;
    }

    impl->drum_energy = 0.0f;

    strategy->vtable = &drum_vtable;
    strategy->impl_data = impl;
    strategy->intensity = 0.5f;

    return strategy;
}
