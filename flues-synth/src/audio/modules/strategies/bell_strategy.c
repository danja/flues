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

    const float harmonic_spread = 6.0f + self->intensity * 14.0f;
    const float even = sinf(input * harmonic_spread + impl->bell_phase) * (0.4f + self->intensity * 0.4f);
    const float odd = sinf(input * (harmonic_spread * 0.5f + 2.0f)) * (0.2f + self->intensity * 0.3f);
    const float bright = fast_tanh((even + odd) * (1.1f + self->intensity * 0.6f));

    return fmaxf(-1.0f, fminf(1.0f, bright));
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
