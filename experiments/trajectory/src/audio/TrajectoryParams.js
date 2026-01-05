const clamp = (value, min = 0, max = 1) => Math.min(Math.max(value, min), max);

export const TRAJECTORY_PARAMS = [
  {
    id: 'sides',
    label: 'Sides',
    min: 3,
    max: 12,
    default: 0.33,
    format: (value) => `${Math.round(value)}`,
  },
  {
    id: 'startPosition',
    label: 'Start Angle',
    min: 0,
    max: 360,
    default: 0.0,
    format: (value) => `${Math.round(value)}deg`,
  },
  {
    id: 'startAngle',
    label: 'Launch Angle',
    min: 0,
    max: 360,
    default: 0.125,
    format: (value) => `${Math.round(value)}deg`,
  },
];

export const getDefaultParamState = () => {
  const result = {};
  TRAJECTORY_PARAMS.forEach((param) => {
    result[param.id] = param.default ?? 0.5;
  });
  return result;
};

const getParamById = (paramId) => {
  const param = TRAJECTORY_PARAMS.find((item) => item.id === paramId);
  if (!param) {
    throw new Error(`[Trajectory] Unknown param "${paramId}"`);
  }
  return param;
};

export const resolveParamValue = (paramId, normalizedValue = 0.5) => {
  const param = getParamById(paramId);
  const normalized = clamp(normalizedValue, 0, 1);
  const mapped = param.min + (param.max - param.min) * normalized;
  const value = param.id === 'sides' ? Math.round(mapped) : mapped;

  return {
    normalized,
    mapped: value,
    formatted: param.format ? param.format(value) : `${value}`,
  };
};
