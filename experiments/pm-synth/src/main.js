// main.js
// Main application initialization and coordination

import { PMSynthProcessor } from './audio/PMSynthProcessor.js';
import { InterfaceType } from './audio/PMSynthEngine.js';
import { KnobController } from './ui/KnobController.js';
import { RotarySwitchController } from './ui/RotarySwitchController.js';
import { KeyboardController } from './ui/KeyboardController.js';
import { Visualizer } from './ui/Visualizer.js';
import {
    DEFAULT_DC_LEVEL,
    DEFAULT_NOISE_LEVEL,
    DEFAULT_TONE_LEVEL,
    DEFAULT_ATTACK,
    DEFAULT_RELEASE,
    DEFAULT_INTERFACE_TYPE,
    DEFAULT_INTERFACE_INTENSITY,
    DEFAULT_TUNING,
    DEFAULT_RATIO,
    DEFAULT_DELAY1_FEEDBACK,
    DEFAULT_DELAY2_FEEDBACK,
    DEFAULT_FILTER_FEEDBACK,
    DEFAULT_FILTER_FREQUENCY,
    DEFAULT_FILTER_Q,
    DEFAULT_FILTER_SHAPE,
    DEFAULT_LFO_FREQUENCY,
    DEFAULT_MODULATION_TYPE_LEVEL,
    INTERFACE_TYPE_NAMES
} from './constants.js';

class PMSynthApp {
    constructor() {
        this.processor = null;
        this.keyboard = null;
        this.visualizer = null;
        this.knobs = {};
        this.switches = {};
        this.isActive = false;
        this.currentNote = null;

        this.initializeUI();
    }

    initializeUI() {
        // Power button
        const powerButton = document.getElementById('power-button');
        if (powerButton) {
            powerButton.addEventListener('click', () => this.togglePower());
        }

        // Initialize Sources knobs
        this.knobs.dcLevel = new KnobController(
            document.getElementById('dc-knob'),
            document.getElementById('dc-value'),
            (value) => this.updateParameter('dcLevel', value),
            0, 100, DEFAULT_DC_LEVEL
        );

        this.knobs.noiseLevel = new KnobController(
            document.getElementById('noise-knob'),
            document.getElementById('noise-value'),
            (value) => this.updateParameter('noiseLevel', value),
            0, 100, DEFAULT_NOISE_LEVEL
        );

        this.knobs.toneLevel = new KnobController(
            document.getElementById('tone-knob'),
            document.getElementById('tone-value'),
            (value) => this.updateParameter('toneLevel', value),
            0, 100, DEFAULT_TONE_LEVEL
        );

        // Initialize Envelope knobs
        this.knobs.attack = new KnobController(
            document.getElementById('attack-knob'),
            document.getElementById('attack-value'),
            (value) => this.updateParameter('attack', value),
            0, 100, DEFAULT_ATTACK
        );

        this.knobs.release = new KnobController(
            document.getElementById('release-knob'),
            document.getElementById('release-value'),
            (value) => this.updateParameter('release', value),
            0, 100, DEFAULT_RELEASE
        );

        // Initialize Interface controls
        this.switches.interfaceType = new RotarySwitchController(
            document.getElementById('interface-type-switch'),
            document.getElementById('interface-type-label'),
            INTERFACE_TYPE_NAMES,
            (position) => this.updateParameter('interfaceType', position),
            DEFAULT_INTERFACE_TYPE
        );

        this.knobs.interfaceIntensity = new KnobController(
            document.getElementById('interface-intensity-knob'),
            document.getElementById('interface-intensity-value'),
            (value) => this.updateParameter('interfaceIntensity', value),
            0, 100, DEFAULT_INTERFACE_INTENSITY
        );

        // Initialize Delay Lines knobs
        this.knobs.tuning = new KnobController(
            document.getElementById('tuning-knob'),
            document.getElementById('tuning-value'),
            (value) => this.updateParameter('tuning', value),
            0, 100, DEFAULT_TUNING
        );

        this.knobs.ratio = new KnobController(
            document.getElementById('ratio-knob'),
            document.getElementById('ratio-value'),
            (value) => this.updateParameter('ratio', value),
            0, 100, DEFAULT_RATIO
        );

        // Initialize Feedback knobs
        this.knobs.delay1Feedback = new KnobController(
            document.getElementById('delay1-fb-knob'),
            document.getElementById('delay1-fb-value'),
            (value) => this.updateParameter('delay1Feedback', value),
            0, 100, DEFAULT_DELAY1_FEEDBACK
        );

        this.knobs.delay2Feedback = new KnobController(
            document.getElementById('delay2-fb-knob'),
            document.getElementById('delay2-fb-value'),
            (value) => this.updateParameter('delay2Feedback', value),
            0, 100, DEFAULT_DELAY2_FEEDBACK
        );

        this.knobs.filterFeedback = new KnobController(
            document.getElementById('filter-fb-knob'),
            document.getElementById('filter-fb-value'),
            (value) => this.updateParameter('filterFeedback', value),
            0, 100, DEFAULT_FILTER_FEEDBACK
        );

        // Initialize Filter knobs
        this.knobs.filterFrequency = new KnobController(
            document.getElementById('filter-freq-knob'),
            document.getElementById('filter-freq-value'),
            (value) => this.updateParameter('filterFrequency', value),
            0, 100, DEFAULT_FILTER_FREQUENCY
        );

        this.knobs.filterQ = new KnobController(
            document.getElementById('filter-q-knob'),
            document.getElementById('filter-q-value'),
            (value) => this.updateParameter('filterQ', value),
            0, 100, DEFAULT_FILTER_Q
        );

        this.knobs.filterShape = new KnobController(
            document.getElementById('filter-shape-knob'),
            document.getElementById('filter-shape-value'),
            (value) => this.updateParameter('filterShape', value),
            0, 100, DEFAULT_FILTER_SHAPE
        );

        // Initialize Modulation knobs
        this.knobs.lfoFrequency = new KnobController(
            document.getElementById('lfo-freq-knob'),
            document.getElementById('lfo-freq-value'),
            (value) => this.updateParameter('lfoFrequency', value),
            0, 100, DEFAULT_LFO_FREQUENCY
        );

        this.knobs.modulationTypeLevel = new KnobController(
            document.getElementById('mod-type-level-knob'),
            document.getElementById('mod-type-level-value'),
            (value) => this.updateParameter('modulationTypeLevel', value),
            0, 100, DEFAULT_MODULATION_TYPE_LEVEL,
            true  // Bipolar mode
        );

        // Initialize keyboard
        const keyboardElement = document.getElementById('keyboard');
        if (keyboardElement) {
            this.keyboard = new KeyboardController(
                keyboardElement,
                (note, frequency) => this.handleNoteOn(note, frequency),
                (note) => this.handleNoteOff(note)
            );
        }

        // Initialize visualizer
        const visualizerElement = document.getElementById('visualizer');
        if (visualizerElement) {
            this.visualizer = new Visualizer(visualizerElement);
        }

        // Update status display
        this.updateStatus();
    }

    async togglePower() {
        if (!this.isActive) {
            await this.powerOn();
        } else {
            this.powerOff();
        }
    }

    async powerOn() {
        try {
            // Initialize audio processor
            this.processor = new PMSynthProcessor();
            await this.processor.initialize();

            // Apply all current parameter values
            Object.entries(this.knobs).forEach(([key, knob]) => {
                const paramName = this.getParameterName(key);
                this.processor.setParameter(paramName, knob.value / 100);
            });

            // Apply switch positions
            if (this.switches.interfaceType) {
                this.processor.setParameter('interfaceType', this.switches.interfaceType.currentPosition);
            }

            // Start visualizer
            if (this.visualizer) {
                this.visualizer.start(() => this.processor.getAnalyserData());
            }

            this.isActive = true;

            // Update UI
            const powerButton = document.getElementById('power-button');
            if (powerButton) {
                powerButton.classList.add('active');
            }
            this.updateStatus();

            console.log('PM Synth powered on');
        } catch (error) {
            console.error('Failed to initialize audio:', error);
            alert('Failed to initialize audio. Please check browser compatibility.');
        }
    }

    powerOff() {
        if (this.processor) {
            if (this.keyboard) {
                this.keyboard.releaseAllKeys();
            }
            this.processor.shutdown();
            this.processor = null;
        }

        if (this.visualizer) {
            this.visualizer.stop();
        }

        this.isActive = false;
        this.currentNote = null;

        // Update UI
        const powerButton = document.getElementById('power-button');
        if (powerButton) {
            powerButton.classList.remove('active');
        }
        this.updateStatus();

        console.log('PM Synth powered off');
    }

    handleNoteOn(note, frequency) {
        if (!this.isActive) return;

        this.currentNote = note;
        this.processor.noteOn(frequency);
        this.updateStatus();
    }

    handleNoteOff(note) {
        if (!this.isActive) return;

        if (this.currentNote === note) {
            this.processor.noteOff();
            this.currentNote = null;
            this.updateStatus();
        }
    }

    updateParameter(param, value) {
        if (this.processor && this.processor.isActive) {
            this.processor.setParameter(param, value);
        }
    }

    getParameterName(knobKey) {
        // Map knob keys to parameter names
        const mapping = {
            dcLevel: 'dcLevel',
            noiseLevel: 'noiseLevel',
            toneLevel: 'toneLevel',
            attack: 'attack',
            release: 'release',
            interfaceIntensity: 'interfaceIntensity',
            tuning: 'tuning',
            ratio: 'ratio',
            delay1Feedback: 'delay1Feedback',
            delay2Feedback: 'delay2Feedback',
            filterFeedback: 'filterFeedback',
            filterFrequency: 'filterFrequency',
            filterQ: 'filterQ',
            filterShape: 'filterShape',
            lfoFrequency: 'lfoFrequency',
            modulationTypeLevel: 'modulationTypeLevel'
        };
        return mapping[knobKey] || knobKey;
    }

    updateStatus() {
        // Status display removed - power button shows active state via CSS
    }
}

// Initialize app when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        window.synthApp = new PMSynthApp();
    });
} else {
    window.synthApp = new PMSynthApp();
}

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
    if (window.synthApp && window.synthApp.isActive) {
        window.synthApp.powerOff();
    }
});
