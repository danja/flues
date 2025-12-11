// midi_mapping.h
// Clean MIDI CC to parameter mapping system
// Replaces complex CC remapping with direct parameter tables

#ifndef FLUES_SYNTH_MIDI_MAPPING_H
#define FLUES_SYNTH_MIDI_MAPPING_H

#include <stdint.h>
#include <stdbool.h>

// Synth parameter identifiers (clearer than CC numbers)
typedef enum {
    PARAM_DISYN_ALGORITHM = 0,
    PARAM_DISYN_PARAM1,
    PARAM_DISYN_PARAM2,
    PARAM_DISYN_LEVEL,
    PARAM_NOISE_LEVEL,
    PARAM_DC_LEVEL,
    PARAM_INTENSITY,
    PARAM_TUNING,
    PARAM_RATIO,
    PARAM_DELAY1_FEEDBACK,
    PARAM_DELAY2_FEEDBACK,
    PARAM_FILTER_FEEDBACK,
    PARAM_FILTER_FREQ,
    PARAM_FILTER_Q,
    PARAM_FILTER_SHAPE,
    PARAM_F1,
    PARAM_F2,
    PARAM_F3,
    PARAM_F4,
    PARAM_NASAL,
    PARAM_SING,
    PARAM_SHOUT,
    PARAM_FRY,
    PARAM_ATTACK,
    PARAM_RELEASE,
    PARAM_LFO_FREQ,
    PARAM_AM_FM_DEPTH,
    PARAM_INTERFACE_TYPE,
    PARAM_MASTER_GAIN,
    PARAM_NONE  // No parameter mapped
} SynthParameter;

// Hardware slider definition
typedef struct {
    uint8_t cc;              // CC number sent by hardware slider
    const char* label;       // Human-readable label
} HardwareSlider;

// 9 hardware sliders (fixed CC assignments)
extern const HardwareSlider HARDWARE_SLIDERS[9];

// Get parameter for a given program and slider index (0-8)
SynthParameter midi_get_slider_parameter(uint8_t program, uint8_t slider_index);

// Get parameter name for display
const char* midi_get_parameter_name(SynthParameter param);

// Find slider index from CC number (returns 0-8 or -1 if not found)
int midi_find_slider_by_cc(uint8_t cc);

#endif // FLUES_SYNTH_MIDI_MAPPING_H
