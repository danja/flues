// dsp_modules.h
// DSP module interfaces for Flues-Synth
// Based on gtk-synth/include/dsp_modules.h with additions for Disyn and Chatterbox

#ifndef FLUES_SYNTH_DSP_MODULES_H
#define FLUES_SYNTH_DSP_MODULES_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// Disyn Module (C wrapper around C++ OscillatorModule)
// ============================================================================

typedef struct DisynModule DisynModule;

DisynModule* disyn_create(float sample_rate);
void disyn_destroy(DisynModule* disyn);
float disyn_process(DisynModule* disyn, float frequency);
void disyn_set_algorithm(DisynModule* disyn, int algorithm);
void disyn_set_param1(DisynModule* disyn, float value);
void disyn_set_param2(DisynModule* disyn, float value);

// ============================================================================
// Sources Module (from PM-Synth)
// ============================================================================

typedef struct {
    float sample_rate;
    float dc_level;
    float noise_level;
    float tone_level;
    float tone_phase;
    float tone_frequency;
} SourcesModule;

SourcesModule* sources_create(float sample_rate);
void sources_destroy(SourcesModule* sources);
float sources_process(SourcesModule* sources);
void sources_set_dc_level(SourcesModule* sources, float level);
void sources_set_noise_level(SourcesModule* sources, float level);
void sources_set_tone_level(SourcesModule* sources, float level);
void sources_set_tone_frequency(SourcesModule* sources, float frequency);

// ============================================================================
// Envelope Module (from Chatterbox - Attack/Release)
// Note: Different from gtk-synth EnvelopeModule - has reset() and is_active()
// ============================================================================

typedef enum {
    ENVELOPE_IDLE = 0,
    ENVELOPE_ATTACK = 1,
    ENVELOPE_SUSTAIN = 2,
    ENVELOPE_RELEASE = 3
} EnvelopeState;

typedef struct {
    float sample_rate;
    float attack_samples;
    float release_samples;
    float current_level;
    bool gate;
    EnvelopeState state;
} EnvelopeModule;

EnvelopeModule* envelope_create(float sample_rate);
void envelope_destroy(EnvelopeModule* env);
float envelope_process(EnvelopeModule* env);
void envelope_set_gate(EnvelopeModule* env, bool gate);
void envelope_set_attack(EnvelopeModule* env, float attack);
void envelope_set_release(EnvelopeModule* env, float release);
void envelope_reset(EnvelopeModule* env);
bool envelope_is_active(EnvelopeModule* env);

// ============================================================================
// Formant Module (Single biquad bandpass filter from Chatterbox)
// ============================================================================

typedef struct {
    float sample_rate;
    float frequency;
    float bandwidth;
    float q;
    // Biquad coefficients
    float a0, a1, a2;
    float b1, b2;
    // State
    float x1, x2;
    float y1, y2;
} FormantModule;

FormantModule* formant_create(float sample_rate);
void formant_destroy(FormantModule* formant);
float formant_process(FormantModule* formant, float input);
void formant_set_frequency(FormantModule* formant, float frequency, float bandwidth);

// ============================================================================
// Formant Bank Module (4 formant cascade + nasal from Chatterbox)
// ============================================================================

typedef struct {
    float sample_rate;
    FormantModule* f1;
    FormantModule* f2;
    FormantModule* f3;
    FormantModule* f4;
    FormantModule* nasal;
    bool nasal_enabled;
    bool shout_enabled;
    float makeup_gain;
} FormantBankModule;

FormantBankModule* formant_bank_create(float sample_rate);
void formant_bank_destroy(FormantBankModule* bank);
float formant_bank_process(FormantBankModule* bank, float input);
void formant_bank_set_f1(FormantBankModule* bank, float frequency);
void formant_bank_set_f2(FormantBankModule* bank, float frequency);
void formant_bank_set_f3(FormantBankModule* bank, float frequency);
void formant_bank_set_f4(FormantBankModule* bank, float frequency);
void formant_bank_set_nasal_enabled(FormantBankModule* bank, bool enabled);
void formant_bank_set_shout_enabled(FormantBankModule* bank, bool enabled);

// ============================================================================
// Interface Module (Strategy Pattern from PM-Synth)
// ============================================================================

typedef struct InterfaceStrategy InterfaceStrategy;

typedef struct {
    float sample_rate;
    int current_type;
    InterfaceStrategy* strategy;
} InterfaceModule;

typedef enum {
    INTERFACE_PLUCK = 0,
    INTERFACE_HIT = 1,
    INTERFACE_REED = 2,
    INTERFACE_FLUTE = 3,
    INTERFACE_BRASS = 4,
    INTERFACE_BOW = 5,
    INTERFACE_BELL = 6,
    INTERFACE_DRUM = 7,
    INTERFACE_CRYSTAL = 8,
    INTERFACE_VAPOR = 9,
    INTERFACE_QUANTUM = 10,
    INTERFACE_PLASMA = 11
} InterfaceType;

InterfaceModule* interface_create(float sample_rate);
void interface_destroy(InterfaceModule* iface);
float interface_process(InterfaceModule* iface, float input);
void interface_set_type(InterfaceModule* iface, int type);
void interface_set_intensity(InterfaceModule* iface, float intensity);
void interface_set_gate(InterfaceModule* iface, float gate);

// ============================================================================
// Delay Lines Module (from PM-Synth)
// ============================================================================

typedef struct {
    float sample_rate;
    float* buffer1;
    float* buffer2;
    int buffer_size;
    float write_pos1;
    float write_pos2;
    float base_delay_samples;
    float tuning_offset;
    float ratio;
} DelayLinesModule;

DelayLinesModule* delay_lines_create(float sample_rate);
void delay_lines_destroy(DelayLinesModule* delays);
void delay_lines_process(DelayLinesModule* delays, float input, float* out1, float* out2);
void delay_lines_set_frequency(DelayLinesModule* delays, float frequency);
void delay_lines_set_tuning(DelayLinesModule* delays, float semitone_offset);
void delay_lines_set_ratio(DelayLinesModule* delays, float ratio);
void delay_lines_clear(DelayLinesModule* delays);

// Alias for note_on compatibility
static inline void delay_lines_note_on(DelayLinesModule* delays, float frequency) {
    delay_lines_set_frequency(delays, frequency);
}

// ============================================================================
// Feedback Module (from PM-Synth)
// ============================================================================

typedef struct {
    float delay1_amount;
    float delay2_amount;
    float filter_amount;
} FeedbackModule;

FeedbackModule* feedback_create(void);
void feedback_destroy(FeedbackModule* fb);
float feedback_process(FeedbackModule* fb, float delay1, float delay2, float filter);
void feedback_set_delay1(FeedbackModule* fb, float amount);
void feedback_set_delay2(FeedbackModule* fb, float amount);
void feedback_set_filter(FeedbackModule* fb, float amount);

// Aliases for consistency with synth_engine usage
static inline void feedback_set_delay1_level(FeedbackModule* fb, float level) {
    feedback_set_delay1(fb, level);
}
static inline void feedback_set_delay2_level(FeedbackModule* fb, float level) {
    feedback_set_delay2(fb, level);
}
static inline void feedback_set_filter_level(FeedbackModule* fb, float level) {
    feedback_set_filter(fb, level);
}

// ============================================================================
// Filter Module (State Variable Filter from PM-Synth)
// ============================================================================

typedef struct {
    float sample_rate;
    float frequency;
    float q;
    float shape;
    float low;
    float band;
    float high;
} FilterModule;

FilterModule* filter_create(float sample_rate);
void filter_destroy(FilterModule* filter);
float filter_process(FilterModule* filter, float input);
void filter_set_frequency(FilterModule* filter, float frequency);
void filter_set_q(FilterModule* filter, float q);
void filter_set_shape(FilterModule* filter, float shape);
void filter_reset(FilterModule* filter);

// ============================================================================
// Modulation Module (LFO from PM-Synth)
// ============================================================================

typedef struct {
    float sample_rate;
    float frequency;
    float depth;
    float phase;
} ModulationModule;

ModulationModule* modulation_create(float sample_rate);
void modulation_destroy(ModulationModule* mod);
void modulation_process(ModulationModule* mod);
float modulation_get_am_scale(ModulationModule* mod);
void modulation_set_frequency(ModulationModule* mod, float frequency);
void modulation_set_depth(ModulationModule* mod, float depth);

#endif // FLUES_SYNTH_DSP_MODULES_H
