// midi_mapping.c
// MIDI CC to parameter mapping tables
// Clean, maintainable mapping system for all 8 programs

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

// Program 0: Disyn Direct
static const SynthParameter PROGRAM_0_MAP[9] = {
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

// Program 7: Disyn Echo
static const SynthParameter PROGRAM_7_MAP[9] = {
    PARAM_DISYN_ALGORITHM,  // Slider 1 (CC 73)
    PARAM_DISYN_PARAM1,     // Slider 2 (CC 72)
    PARAM_DISYN_PARAM2,     // Slider 3 (CC 28)
    PARAM_DISYN_LEVEL,      // Slider 4 (CC 30)
    PARAM_INTENSITY,        // Slider 5 (CC 74)
    PARAM_TUNING,           // Slider 6 (CC 71)
    PARAM_DELAY1_FEEDBACK,  // Slider 7 (CC 1)  ← THE FIX!
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Master program table (array of pointers)
static const SynthParameter* PROGRAM_MAPS[8] = {
    PROGRAM_0_MAP,
    PROGRAM_1_MAP,
    PROGRAM_2_MAP,
    PROGRAM_3_MAP,
    PROGRAM_4_MAP,
    PROGRAM_5_MAP,
    PROGRAM_6_MAP,
    PROGRAM_7_MAP
};

// Get parameter for program/slider combination
SynthParameter midi_get_slider_parameter(uint8_t program, uint8_t slider_index) {
    if (program >= 8 || slider_index >= 9) {
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
    for (int i = 0; i < 9; i++) {
        if (HARDWARE_SLIDERS[i].cc == cc) {
            return i;
        }
    }
    return -1;  // Not found
}
