# Guide: Adding a New Interface Strategy

This guide demonstrates how to add a new interface type to the PM Synthesizer using the strategy pattern architecture.

## Example: Adding a "Membrane" Interface

Let's implement a simple membrane resonator as a new interface type.

## Step 1: Define the Interface Type

Edit `src/audio/modules/interface/InterfaceStrategy.js`:

```javascript
export const InterfaceType = {
    PLUCK: 0,
    HIT: 1,
    REED: 2,
    FLUTE: 3,
    BRASS: 4,
    BOW: 5,
    BELL: 6,
    DRUM: 7,
    MEMBRANE: 8  // ← Add new type
};
```

## Step 2: Create the Strategy Class

Create `src/audio/modules/interface/strategies/MembraneStrategy.js`:

```javascript
// MembraneStrategy.js
// Membrane interface: Nonlinear resonator with pitch glides

import { InterfaceStrategy } from '../InterfaceStrategy.js';
import { EnergyAccumulator } from '../utils/EnergyTracker.js';
import { cubicWaveshaper } from '../utils/NonlinearityLib.js';

export class MembraneStrategy extends InterfaceStrategy {
    constructor(sampleRate) {
        super(sampleRate);

        // Energy tracker for tension modulation
        this.energyTracker = new EnergyAccumulator(0.99);
    }

    /**
     * Process one sample through membrane interface
     * @param {number} input - Input sample
     * @returns {number} Processed output
     */
    process(input) {
        // Track energy for nonlinear effects
        const energy = this.energyTracker.process(input);

        // Nonlinearity strength controlled by intensity
        const alpha = this.intensity * 0.1;

        // Apply cubic nonlinearity scaled by energy
        const nonlinear = cubicWaveshaper(input, alpha * (1 + energy));

        return Math.max(-1, Math.min(1, nonlinear));
    }

    /**
     * Reset state
     */
    reset() {
        this.energyTracker.reset();
    }

    /**
     * Called on note-on trigger
     */
    onNoteOn() {
        this.reset();
    }
}
```

## Step 3: Register in Factory

Edit `src/audio/modules/interface/InterfaceFactory.js`:

```javascript
import { MembraneStrategy } from './strategies/MembraneStrategy.js';

export class InterfaceFactory {
    static createStrategy(type, sampleRate = 44100) {
        switch (type) {
            // ... existing cases ...

            case InterfaceType.MEMBRANE:
                return new MembraneStrategy(sampleRate);

            default:
                console.warn(`Unknown interface type: ${type}, defaulting to REED`);
                return new ReedStrategy(sampleRate);
        }
    }

    static getAvailableTypes() {
        return [
            // ... existing types ...
            { type: InterfaceType.MEMBRANE, name: 'Membrane' }
        ];
    }

    static isValidType(type) {
        return type >= InterfaceType.PLUCK && type <= InterfaceType.MEMBRANE;  // Update max
    }
}
```

## Step 4: (Optional) Add Unit Tests

Create `tests/unit/modules/interface/strategies/MembraneStrategy.test.js`:

```javascript
import { describe, it, expect, beforeEach } from 'vitest';
import { MembraneStrategy } from '../../../../../src/audio/modules/interface/strategies/MembraneStrategy.js';

describe('MembraneStrategy', () => {
    let strategy;

    beforeEach(() => {
        strategy = new MembraneStrategy(44100);
    });

    it('should initialize with default intensity', () => {
        expect(strategy.intensity).toBe(0.5);
    });

    it('should apply nonlinear shaping', () => {
        strategy.setIntensity(0.8);

        const input = 0.5;
        const output = strategy.process(input);

        expect(output).not.toBe(input);  // Should be shaped
        expect(Math.abs(output)).toBeLessThanOrEqual(1.0);  // Should be bounded
    });

    it('should track energy over time', () => {
        strategy.setIntensity(1.0);

        // Feed strong signal
        for (let i = 0; i < 100; i++) {
            strategy.process(0.8);
        }

        const energizedOutput = strategy.process(0.5);

        // Reset and compare
        strategy.reset();
        const resetOutput = strategy.process(0.5);

        expect(Math.abs(energizedOutput)).not.toBeCloseTo(Math.abs(resetOutput), 3);
    });

    it('should reset state properly', () => {
        strategy.process(1.0);
        strategy.reset();

        // State should be cleared
        expect(strategy.energyTracker.getEnergy()).toBe(0);
    });
});
```

## Step 5: Use the New Interface

The new interface is now available via the standard API:

```javascript
import { InterfaceModule, InterfaceType } from './modules/InterfaceModule.js';

const interface = new InterfaceModule(44100);

// Switch to membrane interface
interface.setType(InterfaceType.MEMBRANE);
interface.setIntensity(0.7);

// Process audio
const output = interface.process(input);
```

## Best Practices

### 1. **Keep Strategies Small**
- Target <100 lines per strategy
- Extract complex logic to utility functions
- Single responsibility: process one type of interface

### 2. **Use Existing Utilities**
- Check `utils/` folder before implementing new functions
- Reuse `NonlinearityLib` for waveshaping
- Use `EnergyTracker` for amplitude-dependent effects
- Use `DelayUtils` for fractional delay needs

### 3. **Follow Naming Conventions**
- Class: `[Type]Strategy` (e.g., `MembraneStrategy`)
- File: Same as class name (e.g., `MembraneStrategy.js`)
- Type constant: ALL_CAPS (e.g., `MEMBRANE`)

### 4. **Implement Required Methods**
- `process(input)` - Core DSP processing (required)
- `reset()` - Clear internal state (required)
- `onNoteOn()` - Triggered on gate rise (optional)

### 5. **Parameter Mapping**
Map intensity parameter to physical quantities:
- 0.0 = Soft/gentle/linear behavior
- 0.5 = Normal playing conditions
- 1.0 = Extreme/hard/nonlinear behavior

### 6. **Maintain Output Bounds**
Always clamp output to prevent instability:
```javascript
return Math.max(-1, Math.min(1, output));
```

### 7. **Document Physical Model**
Include header comment explaining:
- Physical system being modeled
- Key parameters and their effects
- References (if based on research)

## Advanced Example: Multi-State Strategy

For more complex interfaces with multiple state variables:

```javascript
export class ComplexStrategy extends InterfaceStrategy {
    constructor(sampleRate) {
        super(sampleRate);

        // Multiple state trackers
        this.energyTracker = new EnergyAccumulator(0.98);
        this.peakFollower = new PeakEnvelopeFollower(0.001, 0.1, sampleRate);
        this.smoother = new LeakyIntegrator(0.01, sampleRate);

        // Custom state
        this.phase = 0;
        this.previousSample = 0;
    }

    process(input) {
        // Track multiple aspects
        const energy = this.energyTracker.process(input);
        const envelope = this.peakFollower.process(input);
        const smoothed = this.smoother.process(input);

        // Complex interaction based on intensity
        const alpha = this.intensity;
        const beta = 1 - this.intensity;

        const output = alpha * this.nonlinearPath(input, energy) +
                      beta * this.linearPath(smoothed, envelope);

        this.previousSample = output;
        return Math.max(-1, Math.min(1, output));
    }

    nonlinearPath(input, energy) {
        // Implement nonlinear processing
        return cubicWaveshaper(input, 0.3 * energy);
    }

    linearPath(smoothed, envelope) {
        // Implement linear processing
        return smoothed * envelope;
    }

    reset() {
        this.energyTracker.reset();
        this.peakFollower.reset();
        this.smoother.reset();
        this.phase = 0;
        this.previousSample = 0;
    }
}
```

## Checklist

When adding a new interface strategy:

- [ ] Add constant to `InterfaceType` enum
- [ ] Create strategy class extending `InterfaceStrategy`
- [ ] Implement required `process()` method
- [ ] Implement required `reset()` method
- [ ] Add case to `InterfaceFactory.createStrategy()`
- [ ] Add entry to `InterfaceFactory.getAvailableTypes()`
- [ ] Update `InterfaceFactory.isValidType()` max bound
- [ ] Write unit tests
- [ ] Test integration with synth engine
- [ ] Update documentation if needed

## Tips

- **Start simple**: Begin with stateless processing, add state incrementally
- **Test incrementally**: Verify each component works before adding complexity
- **Profile performance**: Use Chrome DevTools if processing is expensive
- **Study existing strategies**: See how similar interfaces are implemented
- **Consult research doc**: `interface-algorithms-research.md` has detailed algorithms

## Troubleshooting

**Strategy not being created:**
- Check that `InterfaceType` constant is defined
- Verify factory has case for your type
- Ensure import path is correct

**Output sounds wrong:**
- Add console.log to verify `process()` is being called
- Check that intensity parameter is being used correctly
- Verify state is being reset on note-on

**Tests failing:**
- Ensure `reset()` clears all state completely
- Check that output is always bounded [-1, 1]
- Verify intensity clamping works [0, 1]

**Build errors:**
- Run `npm run build` to check for import issues
- Verify all utility imports are correct
- Check for typos in class/file names
