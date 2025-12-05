// bell_strategy.c
// Bell interface: Metallic waveshaping with evolving phase
// Translated from reference/modules/interface/strategies/BellStrategy.js

#include "interface_strategy.h"
#include "dsp_utils.h"
#include <stdlib.h>
#include <math.h>

typedef struct {
    float bell_phase;
} BellImpl;

static float bell_process(InterfaceStrategy* self, float input) {
    BellImpl* impl = (BellImpl*)self->impl_data;

    impl->bell_phase += 0.1f + self->intensity * 0.25f;
    if (impl->bell_phase > M_PI * 2.0f) {
        impl->bell_phase -= M_PI * 2.0f;
    }

    // Reduced harmonic spread to prevent excessive output (was 6-20, now 3-10)
    const float harmonic_spread = 3.0f + self->intensity * 7.0f;
    // Reduced amplitudes (even was 0.4-0.8, odd was 0.2-0.5)
    const float even = sinf(input * harmonic_spread + impl->bell_phase) * (0.25f + self->intensity * 0.3f);
    const float odd = sinf(input * (harmonic_spread * 0.5f + 2.0f)) * (0.15f + self->intensity * 0.2f);
    // Reduced drive (was 1.1-1.7, now 0.8-1.2)
    const float bright = fast_tanh((even + odd) * (0.8f + self->intensity * 0.4f));

    return fmaxf(-1.0f, fminf(1.0f, bright * 0.6f));  // Scale to match other interfaces
}

static void bell_reset(InterfaceStrategy* self) {
    BellImpl* impl = (BellImpl*)self->impl_data;
    impl->bell_phase = 0.0f;
}

static void bell_set_intensity(InterfaceStrategy* self, float intensity) {
    self->intensity = intensity;
}

static void bell_set_gate(InterfaceStrategy* self, bool gate) {
    if (gate) {
        bell_reset(self);
    }
}

static void bell_destroy(InterfaceStrategy* self) {
    if (self) {
        free(self->impl_data);
        free(self);
    }
}

static InterfaceStrategyVTable bell_vtable = {
    .process = bell_process,
    .reset = bell_reset,
    .set_intensity = bell_set_intensity,
    .set_gate = bell_set_gate,
    .destroy = bell_destroy
};

InterfaceStrategy* bell_strategy_create(float sample_rate) {
    (void)sample_rate;  // Unused
    InterfaceStrategy* strategy = (InterfaceStrategy*)calloc(1, sizeof(InterfaceStrategy));
    if (!strategy) return NULL;

    BellImpl* impl = (BellImpl*)calloc(1, sizeof(BellImpl));
    if (!impl) {
        free(strategy);
        return NULL;
    }

    impl->bell_phase = 0.0f;

    strategy->vtable = &bell_vtable;
    strategy->impl_data = impl;
    strategy->intensity = 0.5f;

    return strategy;
}
