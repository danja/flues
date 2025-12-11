// midi_mapping.c
// MIDI CC to parameter mapping tables
// Clean, maintainable mapping system for all 18 programs

#include "midi_mapping.h"
#include <stddef.h>

// Hardware slider CCs (fixed by controller)
const HardwareSlider HARDWARE_SLIDERS[9] = {
    {73, "Slider 1"},
    {72, "Slider 2"},
    {28, "Slider 3"},
    {30, "Slider 4"},
    {74, "Slider 5"},
    {71, "Slider 6"},
    {1,  "Slider 7"},
    {27, "Slider 8"},
    {7,  "Slider 9"}
};

// Parameter name table
static const char* PARAMETER_NAMES[] = {
    [PARAM_DISYN_ALGORITHM]   = "Disyn Algorithm",
    [PARAM_DISYN_PARAM1]      = "Disyn Param1",
    [PARAM_DISYN_PARAM2]      = "Disyn Param2",
    [PARAM_DISYN_LEVEL]       = "Disyn Level",
    [PARAM_NOISE_LEVEL]       = "Noise Level",
    [PARAM_DC_LEVEL]          = "DC Level",
    [PARAM_INTENSITY]         = "Intensity",
    [PARAM_TUNING]            = "Tuning",
    [PARAM_RATIO]             = "Delay Ratio",
    [PARAM_DELAY1_FEEDBACK]   = "Delay1 Feedback",
    [PARAM_DELAY2_FEEDBACK]   = "Delay2 Feedback",
    [PARAM_FILTER_FEEDBACK]   = "Filter Feedback",
    [PARAM_FILTER_FREQ]       = "Filter Frequency",
    [PARAM_FILTER_Q]          = "Filter Q",
    [PARAM_FILTER_SHAPE]      = "Filter Shape",
    [PARAM_F1]                = "F1 (Jaw)",
    [PARAM_F2]                = "F2 (Tongue)",
    [PARAM_F3]                = "F3 (Lips)",
    [PARAM_F4]                = "F4 (Quality)",
    [PARAM_NASAL]             = "Nasal",
    [PARAM_SING]              = "Sing",
    [PARAM_SHOUT]             = "Shout",
    [PARAM_FRY]               = "Fry",
    [PARAM_ATTACK]            = "Attack",
    [PARAM_RELEASE]           = "Release",
    [PARAM_LFO_FREQ]          = "LFO Frequency",
    [PARAM_AM_FM_DEPTH]       = "AM↔FM Depth",
    [PARAM_INTERFACE_TYPE]    = "Interface Type",
    [PARAM_MASTER_GAIN]       = "Master Gain",
    [PARAM_NONE]              = "(unmapped)"
};

// Program 0: Disyn Echo
static const SynthParameter PROGRAM_0_MAP[9] = {
    PARAM_DISYN_ALGORITHM,  // Slider 1 (CC 73)
    PARAM_DISYN_PARAM1,     // Slider 2 (CC 72)
    PARAM_DISYN_PARAM2,     // Slider 3 (CC 28)
    PARAM_INTERFACE_TYPE,   // Slider 4 (CC 30)
    PARAM_INTENSITY,        // Slider 5 (CC 74)
    PARAM_TUNING,           // Slider 6 (CC 71)
    PARAM_DELAY1_FEEDBACK,  // Slider 7 (CC 1)
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 1: Disyn + Delays
static const SynthParameter PROGRAM_1_MAP[9] = {
    PARAM_DELAY1_FEEDBACK,  // Slider 1 (CC 73)
    PARAM_DELAY2_FEEDBACK,  // Slider 2 (CC 72)
    PARAM_FILTER_FEEDBACK,  // Slider 3 (CC 28)
    PARAM_DISYN_LEVEL,      // Slider 4 (CC 30)
    PARAM_INTENSITY,        // Slider 5 (CC 74)
    PARAM_TUNING,           // Slider 6 (CC 71)
    PARAM_RATIO,            // Slider 7 (CC 1)
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 2: Disyn + Filter
static const SynthParameter PROGRAM_2_MAP[9] = {
    PARAM_FILTER_FREQ,      // Slider 1 (CC 73)
    PARAM_FILTER_Q,         // Slider 2 (CC 72)
    PARAM_FILTER_SHAPE,     // Slider 3 (CC 28)
    PARAM_DISYN_LEVEL,      // Slider 4 (CC 30)
    PARAM_INTENSITY,        // Slider 5 (CC 74)
    PARAM_TUNING,           // Slider 6 (CC 71)
    PARAM_RATIO,            // Slider 7 (CC 1)
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 3: Formant Voice
static const SynthParameter PROGRAM_3_MAP[9] = {
    PARAM_F1,               // Slider 1 (CC 73)
    PARAM_F2,               // Slider 2 (CC 72)
    PARAM_F3,               // Slider 3 (CC 28)
    PARAM_F4,               // Slider 4 (CC 30)
    PARAM_NOISE_LEVEL,      // Slider 5 (CC 74)
    PARAM_NASAL,            // Slider 6 (CC 71)
    PARAM_MASTER_GAIN,      // Slider 7 (CC 1)
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 4: Hybrid Speech
static const SynthParameter PROGRAM_4_MAP[9] = {
    PARAM_F1,               // Slider 1 (CC 73)
    PARAM_F2,               // Slider 2 (CC 72)
    PARAM_F3,               // Slider 3 (CC 28)
    PARAM_F4,               // Slider 4 (CC 30)
    PARAM_DISYN_LEVEL,      // Slider 5 (CC 74)
    PARAM_NOISE_LEVEL,      // Slider 6 (CC 71)
    PARAM_MASTER_GAIN,      // Slider 7 (CC 1)
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 5: Physical Model
static const SynthParameter PROGRAM_5_MAP[9] = {
    PARAM_DELAY1_FEEDBACK,  // Slider 1 (CC 73)
    PARAM_DELAY2_FEEDBACK,  // Slider 2 (CC 72)
    PARAM_FILTER_FEEDBACK,  // Slider 3 (CC 28)
    PARAM_INTERFACE_TYPE,   // Slider 4 (CC 30)
    PARAM_INTENSITY,        // Slider 5 (CC 74)
    PARAM_TUNING,           // Slider 6 (CC 71)
    PARAM_RATIO,            // Slider 7 (CC 1)
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 6: Full Hybrid (DISABLED - redirects to Program 5)
static const SynthParameter PROGRAM_6_MAP[9] = {
    PARAM_NONE, PARAM_NONE, PARAM_NONE, PARAM_NONE, PARAM_NONE,
    PARAM_NONE, PARAM_NONE, PARAM_NONE, PARAM_NONE
};

// Program 7: Disyn Direct
static const SynthParameter PROGRAM_7_MAP[9] = {
    PARAM_DISYN_ALGORITHM,  // Slider 1 (CC 73)
    PARAM_DISYN_PARAM1,     // Slider 2 (CC 72)
    PARAM_DISYN_PARAM2,     // Slider 3 (CC 28)
    PARAM_DISYN_LEVEL,      // Slider 4 (CC 30)
    PARAM_INTENSITY,        // Slider 5 (CC 74)
    PARAM_TUNING,           // Slider 6 (CC 71)
    PARAM_RATIO,            // Slider 7 (CC 1)
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 8: ModFM Formant
static const SynthParameter PROGRAM_8_MAP[9] = {
    PARAM_DISYN_PARAM1,     // Slider 1 (CC 73) → ModFM Index
    PARAM_DISYN_PARAM2,     // Slider 2 (CC 72) → ModFM Ratio
    PARAM_F1,               // Slider 3 (CC 28) → F1 Jaw
    PARAM_F2,               // Slider 4 (CC 30) → F2 Tongue
    PARAM_F3,               // Slider 5 (CC 74) → F3 Lips
    PARAM_F4,               // Slider 6 (CC 71) → F4 Quality
    PARAM_MASTER_GAIN,      // Slider 7 (CC 1) → Master Gain
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 9: DSF Inharmonic Explorer
static const SynthParameter PROGRAM_9_MAP[9] = {
    PARAM_DISYN_PARAM1,     // Slider 1 (CC 73) → DSF Decay
    PARAM_DISYN_PARAM2,     // Slider 2 (CC 72) → DSF Ratio
    PARAM_DELAY1_FEEDBACK,  // Slider 3 (CC 28) → Delay1 Feedback
    PARAM_DELAY2_FEEDBACK,  // Slider 4 (CC 30) → Delay2 Feedback
    PARAM_INTENSITY,        // Slider 5 (CC 74) → Intensity
    PARAM_TUNING,           // Slider 6 (CC 71) → Tuning
    PARAM_RATIO,            // Slider 7 (CC 1) → Delay Ratio
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 10: PAF Direct
static const SynthParameter PROGRAM_10_MAP[9] = {
    PARAM_DISYN_PARAM1,     // Slider 1 (CC 73) → PAF Formant
    PARAM_DISYN_PARAM2,     // Slider 2 (CC 72) → PAF Bandwidth
    PARAM_FILTER_FREQ,      // Slider 3 (CC 28) → Filter Frequency
    PARAM_FILTER_Q,         // Slider 4 (CC 30) → Filter Q
    PARAM_FILTER_SHAPE,     // Slider 5 (CC 74) → Filter Shape
    PARAM_TUNING,           // Slider 6 (CC 71) → Tuning
    PARAM_MASTER_GAIN,      // Slider 7 (CC 1) → Master Gain
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 11: Cascaded DSF+PAF
static const SynthParameter PROGRAM_11_MAP[9] = {
    PARAM_DISYN_PARAM1,     // Slider 1 (CC 73) → DSF Decay
    PARAM_DISYN_PARAM2,     // Slider 2 (CC 72) → DSF Ratio
    PARAM_F1,               // Slider 3 (CC 28) → F1 Low Formant
    PARAM_F2,               // Slider 4 (CC 30) → F2 High Formant
    PARAM_F3,               // Slider 5 (CC 74) → F3 Brightness
    PARAM_TUNING,           // Slider 6 (CC 71) → Tuning
    PARAM_MASTER_GAIN,      // Slider 7 (CC 1) → Master Gain
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 12: Tanh Spectral
static const SynthParameter PROGRAM_12_MAP[9] = {
    PARAM_DISYN_PARAM1,     // Slider 1 (CC 73) → Tanh Drive
    PARAM_DISYN_PARAM2,     // Slider 2 (CC 72) → Tanh Blend
    PARAM_FILTER_FREQ,      // Slider 3 (CC 28) → Filter Frequency
    PARAM_FILTER_Q,         // Slider 4 (CC 30) → Filter Q
    PARAM_FILTER_SHAPE,     // Slider 5 (CC 74) → Filter Shape
    PARAM_FILTER_FEEDBACK,  // Slider 6 (CC 71) → Filter Feedback
    PARAM_INTENSITY,        // Slider 7 (CC 1) → Intensity
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 13: Hybrid DSF→Formant
static const SynthParameter PROGRAM_13_MAP[9] = {
    PARAM_DISYN_PARAM1,     // Slider 1 (CC 73) → DSF Decay
    PARAM_DISYN_PARAM2,     // Slider 2 (CC 72) → DSF Ratio
    PARAM_F1,               // Slider 3 (CC 28) → F1 Jaw
    PARAM_F2,               // Slider 4 (CC 30) → F2 Tongue
    PARAM_F3,               // Slider 5 (CC 74) → F3 Lips
    PARAM_F4,               // Slider 6 (CC 71) → F4 Quality
    PARAM_MASTER_GAIN,      // Slider 7 (CC 1) → Master Gain
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 14: Feedback ModFM
static const SynthParameter PROGRAM_14_MAP[9] = {
    PARAM_DISYN_PARAM1,     // Slider 1 (CC 73) → ModFM Index
    PARAM_DISYN_PARAM2,     // Slider 2 (CC 72) → ModFM Ratio
    PARAM_DELAY1_FEEDBACK,  // Slider 3 (CC 28) → Delay1 Feedback
    PARAM_DELAY2_FEEDBACK,  // Slider 4 (CC 30) → Delay2 Feedback
    PARAM_FILTER_FEEDBACK,  // Slider 5 (CC 74) → Filter Feedback
    PARAM_TUNING,           // Slider 6 (CC 71) → Tuning
    PARAM_RATIO,            // Slider 7 (CC 1) → Delay Ratio
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 15: Dirichlet Explorer
static const SynthParameter PROGRAM_15_MAP[9] = {
    PARAM_DISYN_PARAM1,     // Slider 1 (CC 73) → Harmonics
    PARAM_DISYN_PARAM2,     // Slider 2 (CC 72) → Tilt
    PARAM_FILTER_FREQ,      // Slider 3 (CC 28) → Filter Frequency
    PARAM_FILTER_Q,         // Slider 4 (CC 30) → Filter Q
    PARAM_FILTER_SHAPE,     // Slider 5 (CC 74) → Filter Shape
    PARAM_TUNING,           // Slider 6 (CC 71) → Tuning
    PARAM_MASTER_GAIN,      // Slider 7 (CC 1) → Master Gain
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 16: Multi-Algorithm Demo
static const SynthParameter PROGRAM_16_MAP[9] = {
    PARAM_DISYN_ALGORITHM,  // Slider 1 (CC 73) → Algorithm Selector
    PARAM_DISYN_PARAM1,     // Slider 2 (CC 72) → Param1
    PARAM_DISYN_PARAM2,     // Slider 3 (CC 28) → Param2
    PARAM_DISYN_LEVEL,      // Slider 4 (CC 30) → Disyn Level
    PARAM_INTENSITY,        // Slider 5 (CC 74) → Intensity
    PARAM_TUNING,           // Slider 6 (CC 71) → Tuning
    PARAM_MASTER_GAIN,      // Slider 7 (CC 1) → Master Gain
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 17: Spectral Sculptor
static const SynthParameter PROGRAM_17_MAP[9] = {
    PARAM_DISYN_PARAM1,     // Slider 1 (CC 73) → DSF Decay
    PARAM_DISYN_PARAM2,     // Slider 2 (CC 72) → DSF Ratio
    PARAM_FILTER_FREQ,      // Slider 3 (CC 28) → Filter Frequency
    PARAM_FILTER_Q,         // Slider 4 (CC 30) → Filter Q
    PARAM_FILTER_SHAPE,     // Slider 5 (CC 74) → Filter Shape
    PARAM_DELAY1_FEEDBACK,  // Slider 6 (CC 71) → Delay1 Feedback
    PARAM_FILTER_FEEDBACK,  // Slider 7 (CC 1) → Filter Feedback
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Master program table (array of pointers)
static const SynthParameter* PROGRAM_MAPS[18] = {
    PROGRAM_0_MAP,
    PROGRAM_1_MAP,
    PROGRAM_2_MAP,
    PROGRAM_3_MAP,
    PROGRAM_4_MAP,
    PROGRAM_5_MAP,
    PROGRAM_6_MAP,
    PROGRAM_7_MAP,
    PROGRAM_8_MAP,
    PROGRAM_9_MAP,
    PROGRAM_10_MAP,
    PROGRAM_11_MAP,
    PROGRAM_12_MAP,
    PROGRAM_13_MAP,
    PROGRAM_14_MAP,
    PROGRAM_15_MAP,
    PROGRAM_16_MAP,
    PROGRAM_17_MAP
};

// Get parameter for program/slider combination
SynthParameter midi_get_slider_parameter(uint8_t program, uint8_t slider_index) {
    if (program >= 18 || slider_index >= 9) {
        return PARAM_NONE;
    }
    return PROGRAM_MAPS[program][slider_index];
}

// Get parameter name
const char* midi_get_parameter_name(SynthParameter param) {
    if (param >= PARAM_NONE) {
        return PARAMETER_NAMES[PARAM_NONE];
    }
    return PARAMETER_NAMES[param];
}

// Find slider index from CC number
int midi_find_slider_by_cc(uint8_t cc) {
    // Primary mapping: exact hardware CC numbers
    for (int i = 0; i < 9; i++) {
        if (HARDWARE_SLIDERS[i].cc == cc) {
            return i;
        }
    }

    // Compatibility aliases for controllers still using legacy CCs
    // (e.g., Evolution MK-449 factory assignments)
    static const struct {
        uint8_t cc;
        uint8_t slider_index;
    } SLIDER_ALIASES[] = {
        {91, 2},  // Legacy Effect 1 depth → Slider 3 (CC 28)
        {92, 1},  // Legacy Effect 2 depth → Slider 2 (CC 72)
        {93, 3},  // Legacy Effect 3 depth → Slider 4 (CC 30)
        {5,  6},  // Portamento time      → Slider 7 (CC 1)
        {84, 7},  // Portamento control   → Slider 8 (CC 27)
    };

    for (size_t i = 0; i < sizeof(SLIDER_ALIASES) / sizeof(SLIDER_ALIASES[0]); i++) {
        if (SLIDER_ALIASES[i].cc == cc) {
            return SLIDER_ALIASES[i].slider_index;
        }
    }

    return -1;  // Not found
}
