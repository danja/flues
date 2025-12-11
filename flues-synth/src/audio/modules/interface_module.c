// interface_module.c
// Interface module using Strategy pattern
// Translated from experiments/pm-synth/src/audio/modules/InterfaceModule.js

#include "dsp_modules.h"
#include "interface_strategy.h"
#include <stdlib.h>

InterfaceModule* interface_create(float sample_rate) {
    InterfaceModule* iface = (InterfaceModule*)calloc(1, sizeof(InterfaceModule));
    if (!iface) return NULL;

    iface->sample_rate = sample_rate;
    iface->current_type = 2; // Reed default
    for (int i = 0; i < 12; i++) {
        iface->cache[i] = NULL;
    }

    // Pre-create default strategy and cache it so we never free a live strategy
    iface->cache[iface->current_type] = interface_strategy_create(iface->current_type, sample_rate);
    iface->strategy = iface->cache[iface->current_type];

    return iface;
}

void interface_destroy(InterfaceModule* iface) {
    if (!iface) return;
    for (int i = 0; i < 12; i++) {
        if (iface->cache[i]) {
            interface_strategy_destroy(iface->cache[i]);
        }
    }
    free(iface);
}

float interface_process(InterfaceModule* iface, float input) {
    return interface_strategy_process(iface->strategy, input);
}

void interface_set_type(InterfaceModule* iface, int type) {
    if (type < 0 || type > 11) {
        type = 2; // Clamp to Reed if out of range
    }
    if (type != iface->current_type) {
        float old_intensity = iface->strategy ? iface->strategy->intensity : 0.5f;
        float old_gate = iface->strategy ? iface->strategy->gate : false;

        // Create once per type and cache to avoid freeing while audio thread may be reading
        if (!iface->cache[type]) {
            iface->cache[type] = interface_strategy_create(type, iface->sample_rate);
        }

        if (iface->cache[type]) {
            iface->strategy = iface->cache[type];
            iface->current_type = type;

            // Restore parameters on the newly selected strategy
            interface_strategy_set_intensity(iface->strategy, old_intensity);
            interface_strategy_set_gate(iface->strategy, old_gate);
        }
    }
}

void interface_set_intensity(InterfaceModule* iface, float intensity) {
    interface_strategy_set_intensity(iface->strategy, intensity);
}

void interface_set_gate(InterfaceModule* iface, float gate) {
    interface_strategy_set_gate(iface->strategy, gate);
}
