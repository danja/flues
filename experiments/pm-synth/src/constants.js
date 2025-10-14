// constants.js
// Shared constants for the Clarinet Synthesizer
// All default parameter values are consolidated here to ensure consistency
// between ClarinetEngine.js and clarinet-worklet.js

// ============================================================================
// Reed Parameters
// ============================================================================

// Initial breath pressure - INTERNAL value after mapping (0-1 range)
// NOTE: Due to reed physics, LOWER internal pressure = more stable/louder oscillation
// This value is used as the constructor default and will be overridden by UI
// Worklet range: 0.4-0.8, Engine range: 0.2-1.0
export const DEFAULT_BREATH_PRESSURE = 0.4;

// UI Default Values (0-100 range, used by knobs)
// These map to the internal values via inverted functions in setBreath/setParameter
// Higher UI knob value = lower internal pressure = louder sound
export const DEFAULT_BREATH_UI = 75;  // Maps to ~0.7 internal pressure

// Reed stiffness coefficient (0-1 range)
// Controls how much the reed resists the air pressure
// Higher values = stiffer reed, brighter and more aggressive tone
export const DEFAULT_REED_STIFFNESS = 0.9;

// Reed stiffness UI default (0-100 range)
// Maps directly to internal value (no inversion)
export const DEFAULT_REED_UI = 62.5;  // 50 * 1.25 = 62.5 (25% higher)

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
export const DEFAULT_DELAY_LENGTH = 1000;

// ============================================================================
// Physical Modeling Constants
// ============================================================================

// Reed reflection nonlinearity coefficients
// Used in the cubic/tanh approximation of reed behavior
export const REED_STIFFNESS_SCALE = 8;
export const REED_STIFFNESS_OFFSET = 0.8;

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

// ============================================================================
// Parameter Mapping Scaling Factors
// ============================================================================

// Breath pressure mapping (UI value 0-1 to internal pressure)
// Engine uses wider range, worklet uses narrower range for stability
export const BREATH_SCALE_ENGINE = 0.8;
export const BREATH_OFFSET_ENGINE = 0.2;
export const BREATH_SCALE_WORKLET = 0.4;
export const BREATH_OFFSET_WORKLET = 0.4;

// Noise level scaling (UI value 0-1 to noise amplitude)
export const NOISE_SCALE = 0.3;

// Attack time scaling (UI value 0-1 to seconds)
export const ATTACK_SCALE = 0.1;
export const ATTACK_OFFSET = 0.001;

// Release time scaling (UI value 0-1 to seconds)
export const RELEASE_SCALE = 0.3;
export const RELEASE_OFFSET = 0.01;

// Damping/lowpass filter scaling (UI value 0-1 to cutoff)
export const DAMPING_SCALE = 0.69;
export const DAMPING_OFFSET = 0.3;

// Brightness/highpass filter scaling (UI value 0-1 to cutoff)
// Engine and worklet have different mappings
export const BRIGHTNESS_SCALE_ENGINE = 0.05;
export const BRIGHTNESS_SCALE_WORKLET = 0.01;
export const BRIGHTNESS_OFFSET_WORKLET = 0.001;

// Vibrato amount scaling (UI value 0-1 to modulation depth)
export const VIBRATO_SCALE = 0.05;

// Flow injection mixing factor
export const FLOW_MIX_ENGINE = 0.5;
export const FLOW_MIX_WORKLET = 1.5;

// Final output scaling (worklet only, engine uses OUTPUT_SCALE)
export const WORKLET_OUTPUT_SCALE = 1.0;
