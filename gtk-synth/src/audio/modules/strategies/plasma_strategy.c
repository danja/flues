// plasma_strategy.c
// Plasma interface: Electromagnetic waveguide with nonlinear dispersion
// Translated from reference/modules/interface/strategies/PlasmaStrategy.js

#include "interface_strategy.h"
#include "dsp_utils.h"
#include <stdlib.h>
#include <math.h>

typedef struct {
    AmplitudeTracker amp_tracker;
    float phase;
    float x1;
    float y1;
} PlasmaImpl;

static float plasma_process(InterfaceStrategy* self, float input) {
    PlasmaImpl* impl = (PlasmaImpl*)self->impl_data;

    // Track amplitude for nonlinear effects
    const float amplitude = amplitude_tracker_process(&impl->amp_tracker, input);

    // Self-focusing: high amplitude -> faster propagation
    const float beta = self->intensity * 0.3f;
    const float phase_mod = 1.0f + beta * amplitude;

    impl->phase += 0.1f * phase_mod;
    if (impl->phase > M_PI * 2.0f) {
        impl->phase -= M_PI * 2.0f;
    }

    // Amplitude-to-frequency conversion
    const float freq_mod = sinf(impl->phase) * amplitude * self->intensity * 0.5f;

    // Dispersive allpass filter with amplitude-dependent coefficient
    const float allpass_coeff = 0.3f + amplitude * self->intensity * 0.4f;
    const float dispersed = allpass_coeff * input + impl->x1 - allpass_coeff * impl->y1;

    impl->x1 = input;
    impl->y1 = dispersed;

    // Add frequency modulation component
    float output = dispersed + freq_mod;

    // Nonlinear harmonic generation at high intensities
    if (self->intensity > 0.5f) {
        const float nonlinear = cubic_waveshaper(output, (self->intensity - 0.5f) * 0.4f);
        return fmaxf(-1.0f, fminf(1.0f, nonlinear));
    }

    return fmaxf(-1.0f, fminf(1.0f, output));
}

static void plasma_reset(InterfaceStrategy* self) {
    PlasmaImpl* impl = (PlasmaImpl*)self->impl_data;
    amplitude_tracker_reset(&impl->amp_tracker);
    impl->phase = 0.0f;
    impl->x1 = 0.0f;
    impl->y1 = 0.0f;
}

static void plasma_set_intensity(InterfaceStrategy* self, float intensity) {
    self->intensity = intensity;
}

static void plasma_set_gate(InterfaceStrategy* self, bool gate) {
    if (gate) {
        plasma_reset(self);
    }
}

static void plasma_destroy(InterfaceStrategy* self) {
    if (self) {
        free(self->impl_data);
        free(self);
    }
}

static InterfaceStrategyVTable plasma_vtable = {
    .process = plasma_process,
    .reset = plasma_reset,
    .set_intensity = plasma_set_intensity,
    .set_gate = plasma_set_gate,
    .destroy = plasma_destroy
};

InterfaceStrategy* plasma_strategy_create(float sample_rate) {
    (void)sample_rate;  // Unused
    InterfaceStrategy* strategy = (InterfaceStrategy*)calloc(1, sizeof(InterfaceStrategy));
    if (!strategy) return NULL;

    PlasmaImpl* impl = (PlasmaImpl*)calloc(1, sizeof(PlasmaImpl));
    if (!impl) {
        free(strategy);
        return NULL;
    }

    amplitude_tracker_init(&impl->amp_tracker, 0.001f, 44100.0f);
    impl->phase = 0.0f;
    impl->x1 = 0.0f;
    impl->y1 = 0.0f;

    strategy->vtable = &plasma_vtable;
    strategy->impl_data = impl;
    strategy->intensity = 0.5f;

    return strategy;
}
