/**
 * Parameter Mapping Utilities
 * Converts UI values (0-1) to MIDI CC values (0-127) with appropriate scaling
 */

/**
 * Exponential mapping: value^exp * range
 * Used for frequency parameters (formants, filter freq, LFO freq)
 * @param {number} value - Input value (0-1)
 * @param {number} min - Minimum output value
 * @param {number} max - Maximum output value
 * @returns {number} - Exponentially mapped value
 */
export function expMap(value, min, max) {
    return min * Math.pow(max / min, value);
}

/**
 * Linear interpolation
 * Used for most parameters (levels, feedback, etc.)
 * @param {number} value - Input value (0-1)
 * @param {number} min - Minimum output value
 * @param {number} max - Maximum output value
 * @returns {number} - Linearly interpolated value
 */
export function lerp(value, min, max) {
    return min + value * (max - min);
}

/**
 * Bipolar mapping: -1 to +1
 * Used for AM/FM depth
 * @param {number} value - Input value (0-1)
 * @returns {number} - Bipolar value (-1 to +1)
 */
export function bipolar(value) {
    return (value - 0.5) * 2;
}

/**
 * Discrete mapping for integer values
 * Used for algorithm selector, interface type
 * @param {number} value - Input value (0-1)
 * @param {number} max - Maximum integer value (inclusive)
 * @returns {number} - Discrete integer (0 to max)
 */
export function discrete(value, max) {
    return Math.floor(value * (max + 0.999));
}

/**
 * Convert normalized value (0-1) to MIDI CC value (0-127)
 * @param {number} value - Normalized value (0-1)
 * @returns {number} - MIDI CC value (0-127)
 */
export function toMidiCC(value) {
    return Math.max(0, Math.min(127, Math.round(value * 127)));
}

/**
 * Convert MIDI CC value (0-127) to normalized value (0-1)
 * @param {number} cc - MIDI CC value (0-127)
 * @returns {number} - Normalized value (0-1)
 */
export function fromMidiCC(cc) {
    return cc / 127;
}

/**
 * Parameter definitions for all 29 flues-synth parameters
 */
export const PARAMETERS = {
    // Standard Controls
    intensity: { cc: 1, map: 'linear', min: 0, max: 1, default: 0.5 },
    masterGain: { cc: 7, map: 'linear', min: 0, max: 1, default: 0.5 },

    // Formants
    f1: { cc: 71, map: 'exponential', min: 200, max: 1000, default: 500, unit: 'Hz' },
    f2: { cc: 10, map: 'exponential', min: 500, max: 3000, default: 1500, unit: 'Hz' },
    f3: { cc: 74, map: 'exponential', min: 1500, max: 4000, default: 2500, unit: 'Hz' },
    f4: { cc: 75, map: 'exponential', min: 2500, max: 4500, default: 3500, unit: 'Hz' },

    // Vocal Modes (Toggle: ≥64 = ON)
    nasal: { cc: 80, map: 'boolean', default: false },
    sing: { cc: 81, map: 'boolean', default: false },
    shout: { cc: 82, map: 'boolean', default: false },
    fry: { cc: 83, map: 'boolean', default: false },

    // Disyn Source
    disynAlgorithm: { cc: 16, map: 'discrete', min: 0, max: 6, default: 0 },
    disynParam1: { cc: 17, map: 'linear', min: 0, max: 1, default: 0.5 },
    disynParam2: { cc: 18, map: 'linear', min: 0, max: 1, default: 0.5 },
    disynLevel: { cc: 19, map: 'linear', min: 0, max: 1, default: 0.8 },
    noiseLevel: { cc: 20, map: 'linear', min: 0, max: 1, default: 0.15 },
    dcLevel: { cc: 21, map: 'linear', min: 0, max: 1, default: 0.0 },

    // Interface & Delay
    interfaceType: { cc: 24, map: 'discrete', min: 0, max: 11, default: 2 },
    tuning: { cc: 26, map: 'linear', min: -12, max: 12, default: 0, unit: 'st' },
    ratio: { cc: 27, map: 'exponential', min: 0.5, max: 2.0, default: 1.0 },

    // Feedback
    delay1Feedback: { cc: 28, map: 'linear', min: 0, max: 1, default: 0.2 },
    delay2Feedback: { cc: 29, map: 'linear', min: 0, max: 1, default: 0.2 },
    filterFeedback: { cc: 30, map: 'linear', min: 0, max: 1, default: 0.1 },

    // Filter
    filterFreq: { cc: 32, map: 'exponential', min: 20, max: 20000, default: 2000, unit: 'Hz' },
    filterQ: { cc: 33, map: 'exponential', min: 0.1, max: 10, default: 1.0 },
    filterShape: { cc: 34, map: 'linear', min: 0, max: 1, default: 0.0 },  // LP→BP→HP

    // Envelope
    attack: { cc: 73, map: 'exponential', min: 0.001, max: 1.0, default: 0.01, unit: 's' },
    release: { cc: 72, map: 'exponential', min: 0.01, max: 3.0, default: 0.05, unit: 's' },

    // Modulation
    lfoFreq: { cc: 36, map: 'exponential', min: 0.1, max: 20, default: 5.0, unit: 'Hz' },
    amFmDepth: { cc: 37, map: 'bipolar', min: -1, max: 1, default: 0.0 }
};

/**
 * Convert parameter value to MIDI CC
 * @param {string} paramName - Parameter name from PARAMETERS
 * @param {number} value - Normalized value (0-1) or boolean for toggles
 * @returns {number} - MIDI CC value (0-127)
 */
export function paramToMidi(paramName, value) {
    const param = PARAMETERS[paramName];
    if (!param) {
        console.warn(`Unknown parameter: ${paramName}`);
        return 0;
    }

    let normalizedValue = value;

    // Handle different mapping types
    switch (param.map) {
        case 'linear':
            // Value is already 0-1, just convert to MIDI
            normalizedValue = value;
            break;

        case 'exponential':
            // Value is 0-1, will be mapped exponentially by synth
            normalizedValue = value;
            break;

        case 'discrete':
            // Value is 0-1, convert to discrete steps
            const steps = param.max - param.min + 1;
            normalizedValue = Math.floor(value * steps) / (steps - 1);
            break;

        case 'bipolar':
            // Value is 0-1, center at 0.5
            normalizedValue = value;
            break;

        case 'boolean':
            // Boolean toggle: false = 0, true = 127
            return value ? 127 : 0;

        default:
            normalizedValue = value;
    }

    return toMidiCC(normalizedValue);
}

/**
 * Format parameter value for display
 * @param {string} paramName - Parameter name
 * @param {number} value - Normalized value (0-1)
 * @returns {string} - Formatted display string
 */
export function formatParamValue(paramName, value) {
    const param = PARAMETERS[paramName];
    if (!param) return value.toFixed(2);

    switch (param.map) {
        case 'exponential':
            const expValue = expMap(value, param.min, param.max);
            return `${expValue.toFixed(0)}${param.unit || ''}`;

        case 'linear':
            const linValue = lerp(value, param.min, param.max);
            return `${linValue.toFixed(2)}${param.unit || ''}`;

        case 'discrete':
            const discValue = discrete(value, param.max - param.min) + param.min;
            return `${discValue}`;

        case 'bipolar':
            const biValue = bipolar(value);
            return biValue.toFixed(2);

        case 'boolean':
            return value ? 'ON' : 'OFF';

        default:
            return value.toFixed(2);
    }
}
