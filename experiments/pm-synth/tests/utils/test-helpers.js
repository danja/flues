// test-helpers.js
// Common test patterns and helper functions

import { TestSignals } from './signal-generators.js';
import { SignalAnalysis } from './signal-analyzers.js';

/**
 * Test helper functions for systematic module validation
 */
export const TestHelpers = {
    /**
     * Process a test signal through a module
     * @param {object} module - Module instance with process() method
     * @param {Float32Array} inputSignal - Input signal
     * @param {function} processFunc - Custom process function (module, sample, index) => output
     * @returns {Float32Array} Output signal
     */
    processSignal(module, inputSignal, processFunc = null) {
        const output = new Float32Array(inputSignal.length);

        for (let i = 0; i < inputSignal.length; i++) {
            if (processFunc) {
                output[i] = processFunc(module, inputSignal[i], i);
            } else {
                output[i] = module.process(inputSignal[i]);
            }
        }

        return output;
    },

    /**
     * Test a module with multiple input signals
     * @param {object} module - Module instance
     * @param {Array} testCases - Array of {name, signal, processFunc, validators}
     * @param {object} expect - Vitest expect function
     * @returns {Array} Results for each test case
     */
    runSignalTests(module, testCases, expect) {
        const results = [];

        for (const testCase of testCases) {
            module.reset();

            const output = this.processSignal(
                module,
                testCase.signal,
                testCase.processFunc
            );

            // Run validators
            const validationResults = {};
            for (const [name, validator] of Object.entries(testCase.validators || {})) {
                try {
                    validator(output, expect);
                    validationResults[name] = { passed: true };
                } catch (error) {
                    validationResults[name] = { passed: false, error: error.message };
                }
            }

            results.push({
                name: testCase.name,
                output: output,
                validations: validationResults
            });
        }

        return results;
    },

    /**
     * Sweep a parameter across its full range and validate output
     * @param {object} module - Module instance
     * @param {string} paramName - Parameter setter name (e.g., 'setFrequency')
     * @param {Array} values - Array of parameter values to test
     * @param {Float32Array} inputSignal - Test signal
     * @param {function} processFunc - Custom process function
     * @param {function} validator - Function to validate each output (output, paramValue, expect)
     * @returns {Array} Results for each parameter value
     */
    parameterSweep(module, paramName, values, inputSignal, processFunc, validator) {
        const results = [];

        for (const value of values) {
            module.reset();
            module[paramName](value);

            const output = this.processSignal(module, inputSignal, processFunc);

            const result = {
                paramValue: value,
                output: output,
                valid: true,
                error: null
            };

            try {
                if (validator) {
                    validator(output, value);
                }
            } catch (error) {
                result.valid = false;
                result.error = error.message;
            }

            results.push(result);
        }

        return results;
    },

    /**
     * Stress test: run module for extended duration with random parameter changes
     * @param {object} module - Module instance
     * @param {number} duration - Test duration in samples
     * @param {Array} parameters - Array of {setter, min, max, changeRate}
     * @param {Float32Array} inputSignal - Input signal (will loop if needed)
     * @param {function} processFunc - Custom process function
     * @returns {{output: Float32Array, stable: boolean, issues: Array}}
     */
    stressTest(module, duration, parameters, inputSignal, processFunc = null) {
        module.reset();
        const output = new Float32Array(duration);
        const issues = [];
        let lastParamChange = 0;

        for (let i = 0; i < duration; i++) {
            // Randomly change parameters
            if (parameters && i - lastParamChange > 100) {
                for (const param of parameters) {
                    if (Math.random() < param.changeRate) {
                        const value = param.min + Math.random() * (param.max - param.min);
                        module[param.setter](value);
                        lastParamChange = i;
                    }
                }
            }

            // Process sample
            const inputSample = inputSignal[i % inputSignal.length];
            const outputSample = processFunc
                ? processFunc(module, inputSample, i)
                : module.process(inputSample);

            output[i] = outputSample;

            // Check for issues
            if (!isFinite(outputSample)) {
                issues.push({ index: i, type: 'non-finite', value: outputSample });
            } else if (Math.abs(outputSample) > 100) {
                issues.push({ index: i, type: 'excessive', value: outputSample });
            }
        }

        return {
            output: output,
            stable: issues.length === 0,
            issues: issues
        };
    },

    /**
     * Verify module reset clears all state
     * @param {object} module - Module instance
     * @param {Float32Array} inputSignal - Signal to fill state
     * @param {function} processFunc - Custom process function
     * @param {function} stateChecker - Function to check state (module, expect)
     * @returns {boolean} True if reset successful
     */
    verifyReset(module, inputSignal, processFunc, stateChecker) {
        // Fill module with state
        this.processSignal(module, inputSignal, processFunc);

        // Reset
        module.reset();

        // Check state
        try {
            stateChecker(module);
            return true;
        } catch (error) {
            return false;
        }
    },

    /**
     * Common validators for typical audio module behavior
     */
    validators: {
        /**
         * Validate output stays within bounds
         * @param {number} min - Minimum acceptable value
         * @param {number} max - Maximum acceptable value
         * @returns {function} Validator function
         */
        boundsCheck(min, max) {
            return (output, expect) => {
                const peak = SignalAnalysis.peakLevel(output);
                expect(peak).toBeLessThanOrEqual(max);
                expect(peak).toBeGreaterThanOrEqual(min);
            };
        },

        /**
         * Validate all samples are finite
         * @returns {function} Validator function
         */
        finiteCheck() {
            return (output, expect) => {
                expect(SignalAnalysis.hasValidOutput(output)).toBe(true);
            };
        },

        /**
         * Validate signal doesn't prematurely die
         * @param {number} threshold - Silence threshold
         * @param {number} minDuration - Minimum silent duration before failing
         * @returns {function} Validator function
         */
        noPrematureSilence(threshold = 0.001, minDuration = 100) {
            return (output, expect) => {
                const result = SignalAnalysis.detectPrematureSilence(output, threshold, minDuration);
                expect(result.premature).toBe(false);
            };
        },

        /**
         * Validate minimum energy level is maintained
         * @param {number} minRMS - Minimum RMS level
         * @returns {function} Validator function
         */
        minEnergy(minRMS) {
            return (output, expect) => {
                const rms = SignalAnalysis.rmsLevel(output);
                expect(rms).toBeGreaterThanOrEqual(minRMS);
            };
        },

        /**
         * Validate signal has expected DC offset
         * @param {number} expectedMean - Expected mean value
         * @param {number} tolerance - Acceptable deviation
         * @returns {function} Validator function
         */
        dcOffset(expectedMean, tolerance = 0.01) {
            return (output, expect) => {
                const mean = SignalAnalysis.mean(output);
                expect(mean).toBeCloseTo(expectedMean, -Math.log10(tolerance));
            };
        },

        /**
         * Validate signal has activity (not all zeros)
         * @param {number} minRMS - Minimum RMS to be considered active
         * @returns {function} Validator function
         */
        hasActivity(minRMS = 0.001) {
            return (output, expect) => {
                const rms = SignalAnalysis.rmsLevel(output);
                expect(rms).toBeGreaterThan(minRMS);
            };
        },

        /**
         * Validate no discontinuities (clicks)
         * @param {number} maxDiff - Maximum allowed sample-to-sample difference
         * @returns {function} Validator function
         */
        smooth(maxDiff = 0.5) {
            return (output, expect) => {
                const result = SignalAnalysis.detectDiscontinuities(output, maxDiff);
                expect(result.count).toBe(0);
            };
        }
    },

    /**
     * Create a standard test suite for a module
     * @param {string} moduleName - Module name for describe block
     * @param {function} moduleFactory - Function that returns new module instance
     * @param {object} config - Test configuration
     * @returns {function} Function to call in test file
     */
    createStandardSuite(moduleName, moduleFactory, config) {
        return (describe, it, expect, beforeEach) => {
            describe(`${moduleName} - Signal Processing Tests`, () => {
                let module;

                beforeEach(() => {
                    module = moduleFactory();
                });

                it('should handle impulse response', () => {
                    const input = TestSignals.impulse(1000);
                    const output = this.processSignal(module, input, config.processFunc);
                    expect(SignalAnalysis.hasValidOutput(output)).toBe(true);
                });

                it('should handle DC input', () => {
                    const input = TestSignals.step(0.5, 1000);
                    const output = this.processSignal(module, input, config.processFunc);
                    expect(SignalAnalysis.hasValidOutput(output)).toBe(true);
                });

                it('should handle white noise', () => {
                    const input = TestSignals.whiteNoise(1000);
                    const output = this.processSignal(module, input, config.processFunc);
                    expect(SignalAnalysis.hasValidOutput(output)).toBe(true);
                });

                if (config.additionalTests) {
                    config.additionalTests(module, expect);
                }
            });
        };
    }
};
