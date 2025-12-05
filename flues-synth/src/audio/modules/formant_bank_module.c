// Formant Bank Module - Cascade of Four Formant Filters (F1-F4)
// Ported from lv2/chatterbox/src/modules/FormantBankModule.hpp
// Uses struct definition from dsp_modules.h

#include "dsp_modules.h"
#include <stdlib.h>

// Fixed bandwidths for each formant (Hz)
#define F1_BANDWIDTH 80.0f
#define F2_BANDWIDTH 120.0f
#define F3_BANDWIDTH 150.0f
#define F4_BANDWIDTH 200.0f
#define NASAL_BANDWIDTH 100.0f

FormantBankModule* formant_bank_create(float sample_rate) {
    FormantBankModule* bank = (FormantBankModule*)calloc(1, sizeof(FormantBankModule));
    if (!bank) return NULL;

    bank->sample_rate = sample_rate;
    bank->makeup_gain = 2.0f;  // Reduced from 3.0f to prevent Disyn-induced clipping
    bank->nasal_enabled = false;
    bank->shout_enabled = false;

    // Create four formant filters
    bank->f1 = formant_create(sample_rate);
    bank->f2 = formant_create(sample_rate);
    bank->f3 = formant_create(sample_rate);
    bank->f4 = formant_create(sample_rate);
    bank->nasal = formant_create(sample_rate);

    // Set default formant frequencies with bandwidths (neutral vowel / schwa)
    formant_set_frequency(bank->f1, 500.0f, F1_BANDWIDTH);
    formant_set_frequency(bank->f2, 1500.0f, F2_BANDWIDTH);
    formant_set_frequency(bank->f3, 2500.0f, F3_BANDWIDTH);
    formant_set_frequency(bank->f4, 3500.0f, F4_BANDWIDTH);
    formant_set_frequency(bank->nasal, 250.0f, NASAL_BANDWIDTH);  // Typical nasal formant

    return bank;
}

void formant_bank_destroy(FormantBankModule* bank) {
    if (!bank) return;

    formant_destroy(bank->f1);
    formant_destroy(bank->f2);
    formant_destroy(bank->f3);
    formant_destroy(bank->f4);
    formant_destroy(bank->nasal);
    free(bank);
}

void formant_bank_set_f1(FormantBankModule* bank, float frequency) {
    float freq = frequency;
    if (bank->shout_enabled) {
        freq *= 1.15f;  // 15% boost for shout mode
    }
    formant_set_frequency(bank->f1, freq, F1_BANDWIDTH);
}

void formant_bank_set_f2(FormantBankModule* bank, float frequency) {
    float freq = frequency;
    if (bank->shout_enabled) {
        freq *= 1.15f;
    }
    formant_set_frequency(bank->f2, freq, F2_BANDWIDTH);
}

void formant_bank_set_f3(FormantBankModule* bank, float frequency) {
    float freq = frequency;
    if (bank->shout_enabled) {
        freq *= 1.15f;
    }
    formant_set_frequency(bank->f3, freq, F3_BANDWIDTH);
}

void formant_bank_set_f4(FormantBankModule* bank, float frequency) {
    float freq = frequency;
    if (bank->shout_enabled) {
        freq *= 1.15f;
    }
    formant_set_frequency(bank->f4, freq, F4_BANDWIDTH);
}

void formant_bank_set_nasal_enabled(FormantBankModule* bank, bool enabled) {
    bank->nasal_enabled = enabled;
}

void formant_bank_set_shout_enabled(FormantBankModule* bank, bool enabled) {
    bank->shout_enabled = enabled;
    // Re-apply current frequencies with shout boost if enabled
    // (Frequencies are cached in FormantModule, so we need to trigger an update)
    // For simplicity, we'll update on next parameter change
}

float formant_bank_process(FormantBankModule* bank, float input) {
    // Series cascade: input → F1 → F2 → F3 → F4 → output
    float signal = input;
    signal = formant_process(bank->f1, signal);
    signal = formant_process(bank->f2, signal);
    signal = formant_process(bank->f3, signal);
    signal = formant_process(bank->f4, signal);

    // Parallel nasal formant (if enabled)
    if (bank->nasal_enabled) {
        float nasal_out = formant_process(bank->nasal, input);
        // Reduced from 0.3 to 0.08 - nasal was dominating cascade output
        // Nasal processes raw input while cascade attenuates heavily
        signal += nasal_out * 0.08f;
    }

    // Apply makeup gain
    return signal * bank->makeup_gain;
}

// formant_bank_reset removed - formants auto-reset on instability
