// vapor_strategy.c
// Vapor interface: Chaotic aeroacoustic turbulence
// Translated from reference/modules/interface/strategies/VaporStrategy.js

#include "interface_strategy.h"
#include "dsp_utils.h"
#include <stdlib.h>
#include <math.h>

typedef struct {
    ChaoticOscillator chaos1;
    ChaoticOscillator chaos2;
    ChaoticOscillator chaos3;
    float prev1;
    float prev2;
} VaporImpl;

static float vapor_process(InterfaceStrategy* self, float input) {
    VaporImpl* impl = (VaporImpl*)self->impl_data;

    // Map intensity to chaos parameter (3.57+ = chaotic)
    const float r = 2.5f + self->intensity * 1.5f;

    chaotic_oscillator_set_r(&impl->chaos1, r);
    chaotic_oscillator_set_r(&impl->chaos2, r + 0.1f);
    chaotic_oscillator_set_r(&impl->chaos3, r + 0.2f);

    // Generate chaotic signals
    const float c1 = chaotic_oscillator_process(&impl->chaos1, 0.3f);
    const float c2 = chaotic_oscillator_process(&impl->chaos2, 0.3f);
    const float c3 = chaotic_oscillator_process(&impl->chaos3, 0.3f);

    // Mix input with chaotic forcing
    const float chaos_amount = self->intensity * 0.6f;
    const float input_amount = 1.0f - chaos_amount * 0.5f;

    const float mixed = input * input_amount + (c1 + c2 + c3) * chaos_amount;

    // Couple with feedback from previous samples
    const float feedback = (impl->prev1 * 0.3f + impl->prev2 * 0.2f) * chaos_amount;

    const float turbulent = mixed + feedback;

    // Soft clip to prevent runaway
    const float output = soft_clip_drive(turbulent, 1.2f);

    // Store for feedback
    impl->prev2 = impl->prev1;
    impl->prev1 = output;

    return fmaxf(-1.0f, fminf(1.0f, output));
}

static void vapor_reset(InterfaceStrategy* self) {
    VaporImpl* impl = (VaporImpl*)self->impl_data;
    chaotic_oscillator_reset(&impl->chaos1);
    chaotic_oscillator_reset(&impl->chaos2);
    chaotic_oscillator_reset(&impl->chaos3);
    impl->prev1 = 0.0f;
    impl->prev2 = 0.0f;
}

static void vapor_set_intensity(InterfaceStrategy* self, float intensity) {
    self->intensity = intensity;
}

static void vapor_set_gate(InterfaceStrategy* self, bool gate) {
    if (gate) {
        vapor_reset(self);
    }
}

static void vapor_destroy(InterfaceStrategy* self) {
    if (self) {
        free(self->impl_data);
        free(self);
    }
}

static InterfaceStrategyVTable vapor_vtable = {
    .process = vapor_process,
    .reset = vapor_reset,
    .set_intensity = vapor_set_intensity,
    .set_gate = vapor_set_gate,
    .destroy = vapor_destroy
};

InterfaceStrategy* vapor_strategy_create(float sample_rate) {
    (void)sample_rate;  // Unused
    InterfaceStrategy* strategy = (InterfaceStrategy*)calloc(1, sizeof(InterfaceStrategy));
    if (!strategy) return NULL;

    VaporImpl* impl = (VaporImpl*)calloc(1, sizeof(VaporImpl));
    if (!impl) {
        free(strategy);
        return NULL;
    }

    chaotic_oscillator_init(&impl->chaos1, 3.7f);
    chaotic_oscillator_init(&impl->chaos2, 3.8f);
    chaotic_oscillator_init(&impl->chaos3, 3.9f);
    impl->prev1 = 0.0f;
    impl->prev2 = 0.0f;

    strategy->vtable = &vapor_vtable;
    strategy->impl_data = impl;
    strategy->intensity = 0.5f;

    return strategy;
}
