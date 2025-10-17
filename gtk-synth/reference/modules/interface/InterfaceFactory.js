// InterfaceFactory.js
// Factory for creating interface strategy instances

import { InterfaceType } from './InterfaceStrategy.js';

// Import all strategies - these are used dynamically so mark as side-effect free
// to prevent tree-shaking while allowing Vite to bundle them
import { PluckStrategy } from './strategies/PluckStrategy.js';
import { HitStrategy } from './strategies/HitStrategy.js';
import { ReedStrategy } from './strategies/ReedStrategy.js';
import { FluteStrategy } from './strategies/FluteStrategy.js';
import { BrassStrategy } from './strategies/BrassStrategy.js';
import { BowStrategy } from './strategies/BowStrategy.js';
import { BellStrategy } from './strategies/BellStrategy.js';
import { DrumStrategy } from './strategies/DrumStrategy.js';
import { CrystalStrategy } from './strategies/CrystalStrategy.js';
import { VaporStrategy } from './strategies/VaporStrategy.js';
import { QuantumStrategy } from './strategies/QuantumStrategy.js';
import { PlasmaStrategy } from './strategies/PlasmaStrategy.js';

// Ensure all strategies are available at runtime
// This prevents tree-shaking in production builds
const STRATEGY_REGISTRY = {
    [InterfaceType.PLUCK]: PluckStrategy,
    [InterfaceType.HIT]: HitStrategy,
    [InterfaceType.REED]: ReedStrategy,
    [InterfaceType.FLUTE]: FluteStrategy,
    [InterfaceType.BRASS]: BrassStrategy,
    [InterfaceType.BOW]: BowStrategy,
    [InterfaceType.BELL]: BellStrategy,
    [InterfaceType.DRUM]: DrumStrategy,
    [InterfaceType.CRYSTAL]: CrystalStrategy,
    [InterfaceType.VAPOR]: VaporStrategy,
    [InterfaceType.QUANTUM]: QuantumStrategy,
    [InterfaceType.PLASMA]: PlasmaStrategy
};

/**
 * Factory class for creating interface strategies
 * Encapsulates strategy instantiation logic
 */
export class InterfaceFactory {
    /**
     * Create an interface strategy based on type
     * @param {number} type - InterfaceType enum value
     * @param {number} sampleRate - Sample rate in Hz
     * @returns {InterfaceStrategy} Concrete strategy instance
     */
    static createStrategy(type, sampleRate = 44100) {
        switch (type) {
            case InterfaceType.PLUCK:
                return new PluckStrategy(sampleRate);

            case InterfaceType.HIT:
                return new HitStrategy(sampleRate);

            case InterfaceType.REED:
                return new ReedStrategy(sampleRate);

            case InterfaceType.FLUTE:
                return new FluteStrategy(sampleRate);

            case InterfaceType.BRASS:
                return new BrassStrategy(sampleRate);

            case InterfaceType.BOW:
                return new BowStrategy(sampleRate);

            case InterfaceType.BELL:
                return new BellStrategy(sampleRate);

            case InterfaceType.DRUM:
                return new DrumStrategy(sampleRate);

            case InterfaceType.CRYSTAL:
                return new CrystalStrategy(sampleRate);

            case InterfaceType.VAPOR:
                return new VaporStrategy(sampleRate);

            case InterfaceType.QUANTUM:
                return new QuantumStrategy(sampleRate);

            case InterfaceType.PLASMA:
                return new PlasmaStrategy(sampleRate);

            default:
                console.warn(`Unknown interface type: ${type}, defaulting to REED`);
                return new ReedStrategy(sampleRate);
        }
    }

    /**
     * Get list of all available interface types
     * @returns {Array<{type: number, name: string}>} Array of interface descriptors
     */
    static getAvailableTypes() {
        return [
            { type: InterfaceType.PLUCK, name: 'Pluck' },
            { type: InterfaceType.HIT, name: 'Hit' },
            { type: InterfaceType.REED, name: 'Reed' },
            { type: InterfaceType.FLUTE, name: 'Flute' },
            { type: InterfaceType.BRASS, name: 'Brass' },
            { type: InterfaceType.BOW, name: 'Bow' },
            { type: InterfaceType.BELL, name: 'Bell' },
            { type: InterfaceType.DRUM, name: 'Drum' },
            { type: InterfaceType.CRYSTAL, name: 'Crystal' },
            { type: InterfaceType.VAPOR, name: 'Vapor' },
            { type: InterfaceType.QUANTUM, name: 'Quantum' },
            { type: InterfaceType.PLASMA, name: 'Plasma' }
        ];
    }

    /**
     * Validate interface type
     * @param {number} type - Type to validate
     * @returns {boolean} True if valid
     */
    static isValidType(type) {
        return type >= InterfaceType.PLUCK && type <= InterfaceType.PLASMA;
    }
}
