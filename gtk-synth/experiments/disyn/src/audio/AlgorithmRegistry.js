const clamp01 = (value) => Math.min(Math.max(value, 0), 1);

const expoMap = (value, min, max) => {
  const clamped = clamp01(value);
  return min * Math.pow(max / min, clamped);
};

export const ALGORITHMS = [
  {
    id: 'bandLimitedPulse',
    label: 'Dirichlet Pulse',
    description: 'Band-limited pulse derived from the Dirichlet kernel with harmonic roll-off.',
    params: [
      {
        id: 'harmonics',
        label: 'Harmonics',
        unit: 'count',
        default: 0.6,
        mapValue: (v) => Math.max(1, Math.round(1 + clamp01(v) * 63)),
        format: (v) => `${v}`,
      },
      {
        id: 'tilt',
        label: 'Tilt',
        unit: 'dB/oct',
        default: 0.35,
        mapValue: (v) => -3 + clamp01(v) * 18,
        format: (v) => `${v.toFixed(1)}`,
      },
    ],
  },
  {
    id: 'dsfSingleSided',
    label: 'Single-Sided DSF',
    description: 'Moorer discrete summation formula with variable inharmonic ratio.',
    params: [
      {
        id: 'decay',
        label: 'Decay',
        unit: 'a',
        default: 0.45,
        mapValue: (v) => clamp01(v) * 0.98,
        format: (v) => v.toFixed(2),
      },
      {
        id: 'ratio',
        label: 'Ratio',
        unit: 'θ/ω',
        default: 0.5,
        mapValue: (v) => expoMap(v, 0.5, 4),
        format: (v) => v.toFixed(2),
      },
    ],
  },
  {
    id: 'tanhSquare',
    label: 'Tanh Square',
    description: 'Hyperbolic tangent waveshaping of a sinusoidal carrier.',
    params: [
      {
        id: 'drive',
        label: 'Drive',
        unit: 'index',
        default: 0.55,
        mapValue: (v) => expoMap(v, 0.05, 5),
        format: (v) => v.toFixed(2),
      },
      {
        id: 'trim',
        label: 'Trim',
        unit: 'gain',
        default: 0.5,
        mapValue: (v) => expoMap(v, 0.2, 1.2),
        format: (v) => v.toFixed(2),
      },
    ],
  },
  {
    id: 'tanhSaw',
    label: 'Tanh Saw',
    description: 'Square-to-saw transformation using heterodyned cosine blend.',
    params: [
      {
        id: 'drive',
        label: 'Drive',
        unit: 'index',
        default: 0.6,
        mapValue: (v) => expoMap(v, 0.05, 4.5),
        format: (v) => v.toFixed(2),
      },
      {
        id: 'blend',
        label: 'Blend',
        unit: '%',
        default: 0.4,
        mapValue: (v) => clamp01(v),
        format: (v) => `${Math.round(v * 100)}`,
      },
    ],
  },
  {
    id: 'paf',
    label: 'Phase-Aligned Formant',
    description: 'PAF oscillator with configurable formant position and bandwidth.',
    params: [
      {
        id: 'formant',
        label: 'Formant',
        unit: '×f0',
        default: 0.5,
        mapValue: (v) => expoMap(v, 0.5, 6),
        format: (v) => v.toFixed(2),
      },
      {
        id: 'bandwidth',
        label: 'Bandwidth',
        unit: 'Hz',
        default: 0.3,
        mapValue: (v) => expoMap(v, 50, 3000),
        format: (v) => Math.round(v),
      },
    ],
  },
  {
    id: 'modFm',
    label: 'Modified FM',
    description: 'Modified FM with exponential modulator and smooth spectra.',
    params: [
      {
        id: 'index',
        label: 'Index',
        unit: 'k',
        default: 0.5,
        mapValue: (v) => expoMap(v, 0.01, 8),
        format: (v) => v.toFixed(2),
      },
      {
        id: 'ratio',
        label: 'Ratio',
        unit: 'c:m',
        default: 0.4,
        mapValue: (v) => expoMap(v, 0.25, 6),
        format: (v) => v.toFixed(2),
      },
    ],
  },
];

export const DEFAULT_ALGORITHM_ID = 'tanhSquare';

export function getAlgorithmById(id) {
  return ALGORITHMS.find((alg) => alg.id === id) ?? ALGORITHMS[0];
}

export function getDefaultParamState(algorithmId = DEFAULT_ALGORITHM_ID) {
  const algorithm = getAlgorithmById(algorithmId);
  const params = {};

  algorithm.params.forEach((param) => {
    params[param.id] = clamp01(param.default);
  });

  return params;
}

export function resolveParamValue(algorithmId, paramId, normalizedValue) {
  const algorithm = getAlgorithmById(algorithmId);
  const definition = algorithm.params.find((param) => param.id === paramId);
  if (!definition) {
    throw new Error(`[AlgorithmRegistry] Unknown param ${paramId} for ${algorithmId}`);
  }
  const clamped = clamp01(normalizedValue);
  const mapped = definition.mapValue(clamped);
  return {
    normalized: clamped,
    mapped,
    formatted: definition.format(mapped),
  };
}

