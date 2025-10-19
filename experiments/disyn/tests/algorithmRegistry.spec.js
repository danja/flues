import { describe, it, expect } from 'vitest';
import { ALGORITHMS, getAlgorithmById, resolveParamValue } from '../src/audio/AlgorithmRegistry.js';

describe('AlgorithmRegistry', () => {
  it('contains expected algorithm definitions', () => {
    const ids = ALGORITHMS.map((alg) => alg.id);
    expect(ids).toContain('tanhSquare');
    expect(ids).toContain('modFm');
    expect(ids).toContain('dsfDoubleSided');
  });

  it('returns algorithms by id', () => {
    const algorithm = getAlgorithmById('tanhSquare');
    expect(algorithm.label).toBeDefined();
    expect(Array.isArray(algorithm.params)).toBe(true);
  });

  it('maps parameter values to domain units', () => {
    const { mapped } = resolveParamValue('dsfSingleSided', 'ratio', 0.5);
    expect(mapped).toBeGreaterThan(0.5);
    expect(mapped).toBeLessThan(4);
  });

  it('supports double-sided dsf parameter mapping', () => {
    const { mapped } = resolveParamValue('dsfDoubleSided', 'decay', 0.75);
    expect(mapped).toBeLessThan(1);
  });
});
