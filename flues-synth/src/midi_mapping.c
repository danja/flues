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
    [PARAM_DISYN_PARAM3]      = "Disyn Param3",
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

// Program 6: Full Hybrid - All modules enabled
static const SynthParameter PROGRAM_6_MAP[9] = {
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

// Program 18: Hybrid Formant Engine (Algorithm 7)
static const SynthParameter PROGRAM_18_MAP[9] = {
    PARAM_DISYN_PARAM1,     // Slider 1 (CC 73) → ModFM Index
    PARAM_DISYN_PARAM2,     // Slider 2 (CC 72) → PAF Bandwidth
    PARAM_DISYN_PARAM3,     // Slider 3 (CC 28) → Formant Spacing
    PARAM_DISYN_LEVEL,      // Slider 4 (CC 30) → Disyn Level
    PARAM_INTENSITY,        // Slider 5 (CC 74) → Intensity
    PARAM_TUNING,           // Slider 6 (CC 71) → Tuning
    PARAM_MASTER_GAIN,      // Slider 7 (CC 1) → Master Gain
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 19: Cascaded Spectral Sculptor (Algorithm 8)
static const SynthParameter PROGRAM_19_MAP[9] = {
    PARAM_DISYN_PARAM1,     // Slider 1 (CC 73) → DSF Decay
    PARAM_DISYN_PARAM2,     // Slider 2 (CC 72) → Asymmetric FM Ratio
    PARAM_DISYN_PARAM3,     // Slider 3 (CC 28) → Tanh Drive
    PARAM_FILTER_FREQ,      // Slider 4 (CC 30) → Filter Frequency
    PARAM_FILTER_Q,         // Slider 5 (CC 74) → Filter Q
    PARAM_INTENSITY,        // Slider 6 (CC 71) → Intensity
    PARAM_MASTER_GAIN,      // Slider 7 (CC 1) → Master Gain
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 20: Parallel Distortion Bank (Algorithm 9)
static const SynthParameter PROGRAM_20_MAP[9] = {
    PARAM_DISYN_PARAM1,     // Slider 1 (CC 73) → ModFM Index (shared)
    PARAM_DISYN_PARAM2,     // Slider 2 (CC 72) → PAF Bandwidth (shared)
    PARAM_DISYN_PARAM3,     // Slider 3 (CC 28) → Mix Balance
    PARAM_INTENSITY,        // Slider 4 (CC 30) → Intensity
    PARAM_TUNING,           // Slider 5 (CC 74) → Tuning
    PARAM_FILTER_FREQ,      // Slider 6 (CC 71) → Filter Frequency
    PARAM_MASTER_GAIN,      // Slider 7 (CC 1) → Master Gain
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 21: Feedback Distortion Network (Algorithm 10)
static const SynthParameter PROGRAM_21_MAP[9] = {
    PARAM_DISYN_PARAM1,     // Slider 1 (CC 73) → ModFM Index
    PARAM_DISYN_PARAM2,     // Slider 2 (CC 72) → Feedback Gain
    PARAM_DISYN_PARAM3,     // Slider 3 (CC 28) → Feedback Lowpass
    PARAM_INTENSITY,        // Slider 4 (CC 30) → Intensity
    PARAM_TUNING,           // Slider 5 (CC 74) → Tuning
    PARAM_FILTER_FREQ,      // Slider 6 (CC 71) → Filter Frequency
    PARAM_MASTER_GAIN,      // Slider 7 (CC 1) → Master Gain
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 22: Morphing Spectral Engine (Algorithm 11)
static const SynthParameter PROGRAM_22_MAP[9] = {
    PARAM_DISYN_PARAM1,     // Slider 1 (CC 73) → Morph Position (0=DSF, 0.5=ModFM, 1=PAF)
    PARAM_DISYN_PARAM2,     // Slider 2 (CC 72) → Shared Character
    PARAM_DISYN_PARAM3,     // Slider 3 (CC 28) → Crossfade Curve
    PARAM_INTENSITY,        // Slider 4 (CC 30) → Intensity
    PARAM_TUNING,           // Slider 5 (CC 74) → Tuning
    PARAM_FILTER_FREQ,      // Slider 6 (CC 71) → Filter Frequency
    PARAM_MASTER_GAIN,      // Slider 7 (CC 1) → Master Gain
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 23: Inharmonic Resonator (Algorithm 12)
static const SynthParameter PROGRAM_23_MAP[9] = {
    PARAM_DISYN_PARAM1,     // Slider 1 (CC 73) → DSF Inharmonicity
    PARAM_DISYN_PARAM2,     // Slider 2 (CC 72) → PAF Frequency Shift
    PARAM_DISYN_PARAM3,     // Slider 3 (CC 28) → PAF Formant Frequency
    PARAM_TUNING,           // Slider 4 (CC 30) → Tuning
    PARAM_INTENSITY,        // Slider 5 (CC 74) → Intensity
    PARAM_DELAY1_FEEDBACK,  // Slider 6 (CC 71) → Delay1 Feedback
    PARAM_MASTER_GAIN,      // Slider 7 (CC 1) → Master Gain
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 24: Adaptive Filter Emulation (Algorithm 13)
static const SynthParameter PROGRAM_24_MAP[9] = {
    PARAM_DISYN_PARAM1,     // Slider 1 (CC 73) → Filter Cutoff (DSF N)
    PARAM_DISYN_PARAM2,     // Slider 2 (CC 72) → Filter Resonance (DSF a)
    PARAM_DISYN_PARAM3,     // Slider 3 (CC 28) → Filter Character (ModFM k)
    PARAM_FILTER_FREQ,      // Slider 4 (CC 30) → SVF Frequency
    PARAM_FILTER_Q,         // Slider 5 (CC 74) → SVF Q
    PARAM_LFO_FREQ,         // Slider 6 (CC 71) → LFO Frequency
    PARAM_MASTER_GAIN,      // Slider 7 (CC 1) → Master Gain
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 25: Multi-Stage Waveshaping (Algorithm 14)
static const SynthParameter PROGRAM_25_MAP[9] = {
    PARAM_DISYN_PARAM1,     // Slider 1 (CC 73) → Tanh Drive
    PARAM_DISYN_PARAM2,     // Slider 2 (CC 72) → Exponential Depth
    PARAM_DISYN_PARAM3,     // Slider 3 (CC 28) → Ring Mod Carrier
    PARAM_FILTER_FREQ,      // Slider 4 (CC 30) → Filter Frequency
    PARAM_FILTER_Q,         // Slider 5 (CC 74) → Filter Q
    PARAM_INTENSITY,        // Slider 6 (CC 71) → Intensity
    PARAM_MASTER_GAIN,      // Slider 7 (CC 1) → Master Gain
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 26: Frequency-Dependent Asymmetry (Algorithm 15)
static const SynthParameter PROGRAM_26_MAP[9] = {
    PARAM_DISYN_PARAM1,     // Slider 1 (CC 73) → Low-Band r (downward shift)
    PARAM_DISYN_PARAM2,     // Slider 2 (CC 72) → High-Band r (upward shift)
    PARAM_DISYN_PARAM3,     // Slider 3 (CC 28) → Crossover Frequency
    PARAM_FILTER_FREQ,      // Slider 4 (CC 30) → Filter Frequency
    PARAM_FILTER_Q,         // Slider 5 (CC 74) → Filter Q
    PARAM_LFO_FREQ,         // Slider 6 (CC 71) → LFO Frequency
    PARAM_MASTER_GAIN,      // Slider 7 (CC 1) → Master Gain
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 27: Cross-Algorithm Modulation (Algorithm 16)
static const SynthParameter PROGRAM_27_MAP[9] = {
    PARAM_DISYN_PARAM1,     // Slider 1 (CC 73) → DSF→ModFM Mod Depth
    PARAM_DISYN_PARAM2,     // Slider 2 (CC 72) → PAF→AsymFM Mod Depth
    PARAM_DISYN_PARAM3,     // Slider 3 (CC 28) → ModFM→DSF Mod Depth
    PARAM_INTENSITY,        // Slider 4 (CC 30) → Intensity
    PARAM_TUNING,           // Slider 5 (CC 74) → Tuning
    PARAM_FILTER_FREQ,      // Slider 6 (CC 71) → Filter Frequency
    PARAM_MASTER_GAIN,      // Slider 7 (CC 1) → Master Gain
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 28: Vocal Morph Matrix
static const SynthParameter PROGRAM_28_MAP[9] = {
    PARAM_DISYN_PARAM1,     // Slider 1 (CC 73) → ModFM Index
    PARAM_DISYN_PARAM2,     // Slider 2 (CC 72) → ModFM Ratio
    PARAM_F1,               // Slider 3 (CC 28) → F1 Jaw
    PARAM_F2,               // Slider 4 (CC 30) → F2 Tongue
    PARAM_NASAL,            // Slider 5 (CC 74) → Nasal
    PARAM_SING,             // Slider 6 (CC 71) → Sing
    PARAM_SHOUT,            // Slider 7 (CC 1) → Shout
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Program 29: Taylor Series Approximation
static const SynthParameter PROGRAM_29_MAP[9] = {
    PARAM_DISYN_PARAM1,     // Slider 1 (CC 73) → First Terms (1-10)
    PARAM_DISYN_PARAM2,     // Slider 2 (CC 72) → Second Terms (1-10)
    PARAM_DISYN_PARAM3,     // Slider 3 (CC 28) → Blend (0-1)
    PARAM_MASTER_GAIN,      // Slider 4 (CC 30) → Master Gain
    PARAM_NONE,             // Slider 5 (CC 74) → (unused)
    PARAM_NONE,             // Slider 6 (CC 71) → (unused)
    PARAM_NONE,             // Slider 7 (CC 1) → (unused)
    PARAM_ATTACK,           // Slider 8 (CC 27)
    PARAM_RELEASE           // Slider 9 (CC 7)
};

// Master program table (array of pointers)
static const SynthParameter* PROGRAM_MAPS[30] = {
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
    PROGRAM_17_MAP,
    PROGRAM_18_MAP,
    PROGRAM_19_MAP,
    PROGRAM_20_MAP,
    PROGRAM_21_MAP,
    PROGRAM_22_MAP,
    PROGRAM_23_MAP,
    PROGRAM_24_MAP,
    PROGRAM_25_MAP,
    PROGRAM_26_MAP,
    PROGRAM_27_MAP,
    PROGRAM_28_MAP,
    PROGRAM_29_MAP
};

// Get parameter for program/slider combination
SynthParameter midi_get_slider_parameter(uint8_t program, uint8_t slider_index) {
    if (program >= 30 || slider_index >= 9) {
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
