// constants.js
// Shared constants for the Clarinet Synthesizer
// All default parameter values are consolidated here to ensure consistency
// between ClarinetEngine.js and clarinet-worklet.js

// ============================================================================
// Reed Parameters
// ============================================================================

// Initial breath pressure (0-1 range)
// Controls the amount of air pressure applied to the reed
// Higher values = more forceful blowing, louder and more intense tone
export const DEFAULT_BREATH_PRESSURE = 0.7;

// Reed stiffness coefficient (0-1 range)
// Controls how much the reed resists the air pressure
// Higher values = stiffer reed, brighter and more aggressive tone
export const DEFAULT_REED_STIFFNESS = 0.5;

// Breath turbulence noise level (0-1 range)
// Adds realistic noise to simulate turbulent airflow
// Higher values = more breathy, airy tone
export const DEFAULT_NOISE_LEVEL = 0.15;

// ============================================================================
// Filter Parameters
// ============================================================================

// Lowpass filter cutoff coefficient (0-1 range)
// Controls damping/absorption of high frequencies in the bore
// Lower values = more damping, darker tone
export const DEFAULT_LPF_CUTOFF = 0.7;

// Highpass filter cutoff coefficient (0-1 range)
// DC blocking filter to prevent buildup of low frequency energy
// Higher values = more bass cut, brighter tone
export const DEFAULT_HPF_CUTOFF = 0.01;

// ============================================================================
// Envelope Parameters
// ============================================================================

// Attack time in seconds
// How quickly the sound reaches full volume after note-on
export const DEFAULT_ATTACK_TIME = 0.01;

// Release time in seconds
// How quickly the sound fades after note-off
export const DEFAULT_RELEASE_TIME = 0.05;

// ============================================================================
// Vibrato Parameters
// ============================================================================

// Vibrato modulation amount (0-1 range)
// Controls the depth of pitch wobble
// Higher values = more pronounced vibrato
export const DEFAULT_VIBRATO_AMOUNT = 0;

// Vibrato rate in Hz
// Controls the speed of the vibrato oscillation
export const DEFAULT_VIBRATO_RATE = 5;

// ============================================================================
// Pitch Parameters
// ============================================================================

// Default fundamental frequency in Hz (A4)
export const DEFAULT_FREQUENCY = 440;

// ============================================================================
// Waveguide Parameters
// ============================================================================

// Initial delay line length in samples
// Will be recalculated based on frequency when notes are played
export const DEFAULT_DELAY_LENGTH = 100;

// ============================================================================
// Physical Modeling Constants
// ============================================================================

// Reed reflection nonlinearity coefficients
// Used in the cubic/tanh approximation of reed behavior
export const REED_STIFFNESS_SCALE = 5;
export const REED_STIFFNESS_OFFSET = 0.5;

// Tanh approximation threshold
// Beyond ±3, we clip to ±1 for efficiency
export const TANH_CLIP_THRESHOLD = 3;

// Tanh approximation coefficients
// Fast rational approximation: x(27+x²)/(27+9x²)
export const TANH_NUMERATOR_CONSTANT = 27;
export const TANH_DENOMINATOR_SCALE = 9;

// Saturation scaling factor
// Applied before saturation to control overall level
export const SATURATION_SCALE = 0.95;

// ============================================================================
// Output Scaling
// ============================================================================

// Final output amplitude scaling
export const OUTPUT_SCALE = 0.5;

// Initial excitation amplitude for delay line
// Small random noise injected when note starts
export const INITIAL_EXCITATION_AMPLITUDE = 0.01;
