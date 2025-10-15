// constants.js
// Shared constants for the PM Synth

// ============================================================================
// Default Parameter Values (UI range 0-100, normalized 0-1 internally)
// ============================================================================

// Sources
export const DEFAULT_DC_LEVEL = 50;      // 50%
export const DEFAULT_NOISE_LEVEL = 15;   // 15%
export const DEFAULT_TONE_LEVEL = 0;     // 0% (off by default)

// Envelope
export const DEFAULT_ATTACK = 10;        // Fast attack
export const DEFAULT_RELEASE = 50;       // Medium release

// Interface
export const DEFAULT_INTERFACE_TYPE = 2;      // Reed (InterfaceType.REED)
export const DEFAULT_INTERFACE_INTENSITY = 50; // 50%

// Delay Lines
export const DEFAULT_TUNING = 50;        // Center (0 semitones)
export const DEFAULT_RATIO = 50;         // Center (1.0 ratio)

// Feedback
export const DEFAULT_DELAY1_FEEDBACK = 95;  // High feedback
export const DEFAULT_DELAY2_FEEDBACK = 95;  // High feedback
export const DEFAULT_FILTER_FEEDBACK = 0;   // No filter feedback

// Filter
export const DEFAULT_FILTER_FREQUENCY = 70;  // ~7kHz
export const DEFAULT_FILTER_Q = 20;          // Low Q
export const DEFAULT_FILTER_SHAPE = 0;       // Lowpass

// Modulation
export const DEFAULT_LFO_FREQUENCY = 30;         // ~5Hz
export const DEFAULT_MODULATION_TYPE_LEVEL = 50; // Center (no modulation)

// ============================================================================
// Interface Type Names
// ============================================================================

export const INTERFACE_TYPE_NAMES = [
    'Pluck',
    'Hit',
    'Reed',
    'Flute',
    'Brass'
];

// ============================================================================
// Audio Parameters
// ============================================================================

export const DEFAULT_FREQUENCY = 440;  // A4
export const DEFAULT_SAMPLE_RATE = 44100;
