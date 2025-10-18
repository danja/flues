// quantum_strategy.c
// Quantum interface: Amplitude-quantized resonator with zipper artifacts
// Translated from reference/modules/interface/strategies/QuantumStrategy.js

#include "interface_strategy.h"
#include "dsp_utils.h"
#include <stdlib.h>
#include <math.h>

// QuantumStrategy is stateless, so no impl_data needed

static float quantum_process(InterfaceStrategy* self, float input) {
    // Map intensity to bit depth (0.0 = 8-bit, 1.0 = 3-bit)
    const int bit_depth = 8 - (int)floorf(self->intensity * 5.0f);
    const float levels = powf(2.0f, (float)bit_depth);

    // Quantize amplitude
    const float quantized = roundf(input * levels) / levels;

    // Add slight nonlinearity at quantization boundaries
    // Creates interesting harmonic distortion
    const float near_boundary = fabsf(input * levels - roundf(input * levels));
    const float boundary_noise = (near_boundary > 0.45f) ?
                                 (white_noise() * 0.01f * self->intensity) : 0.0f;

    const float output = quantized + boundary_noise;

    return fmaxf(-1.0f, fminf(1.0f, output));
}

static void quantum_reset(InterfaceStrategy* self) {
    // Stateless, nothing to reset
    (void)self;
}

static void quantum_set_intensity(InterfaceStrategy* self, float intensity) {
    self->intensity = intensity;
}

static void quantum_set_gate(InterfaceStrategy* self, bool gate) {
    // No gate behavior for quantum
    (void)self;
    (void)gate;
}

static void quantum_destroy(InterfaceStrategy* self) {
    if (self) {
        free(self);
    }
}

static InterfaceStrategyVTable quantum_vtable = {
    .process = quantum_process,
    .reset = quantum_reset,
    .set_intensity = quantum_set_intensity,
    .set_gate = quantum_set_gate,
    .destroy = quantum_destroy
};

InterfaceStrategy* quantum_strategy_create(float sample_rate) {
    (void)sample_rate;  // Unused
    InterfaceStrategy* strategy = (InterfaceStrategy*)calloc(1, sizeof(InterfaceStrategy));
    if (!strategy) return NULL;

    strategy->vtable = &quantum_vtable;
    strategy->impl_data = NULL;  // Stateless
    strategy->intensity = 0.5f;

    return strategy;
}
