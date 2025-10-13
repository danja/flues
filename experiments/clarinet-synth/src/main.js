// main.js
// Main application initialization and coordination

import { ClarinetProcessor } from './audio/ClarinetProcessor.js';
import { KnobController } from './ui/KnobController.js';
import { KeyboardController } from './ui/KeyboardController.js';
import { Visualizer } from './ui/Visualizer.js';

class ClarinetSynthApp {
    constructor() {
        this.processor = null;
        this.keyboard = null;
        this.visualizer = null;
        this.knobs = {};
        this.isActive = false;
        this.currentNote = null;

        this.initializeUI();
    }

    initializeUI() {
        // Power button
        const powerButton = document.getElementById('power-button');
        powerButton.addEventListener('click', () => this.togglePower());

        // Initialize knobs
        this.knobs.breath = new KnobController(
            document.getElementById('breath-knob'),
            document.getElementById('breath-value'),
            (value) => this.updateParameter('breath', value),
            0, 100, 70
        );

        this.knobs.reed = new KnobController(
            document.getElementById('reed-knob'),
            document.getElementById('reed-value'),
            (value) => this.updateParameter('reed', value),
            0, 100, 50
        );

        this.knobs.noise = new KnobController(
            document.getElementById('noise-knob'),
            document.getElementById('noise-value'),
            (value) => this.updateParameter('noise', value),
            0, 100, 15
        );

        this.knobs.attack = new KnobController(
            document.getElementById('attack-knob'),
            document.getElementById('attack-value'),
            (value) => this.updateParameter('attack', value),
            0, 100, 10
        );

        this.knobs.damping = new KnobController(
            document.getElementById('damping-knob'),
            document.getElementById('damping-value'),
            (value) => this.updateParameter('damping', value),
            0, 100, 20
        );

        this.knobs.brightness = new KnobController(
            document.getElementById('brightness-knob'),
            document.getElementById('brightness-value'),
            (value) => this.updateParameter('brightness', value),
            0, 100, 70
        );

        this.knobs.vibrato = new KnobController(
            document.getElementById('vibrato-knob'),
            document.getElementById('vibrato-value'),
            (value) => this.updateParameter('vibrato', value),
            0, 100, 0
        );

        this.knobs.release = new KnobController(
            document.getElementById('release-knob'),
            document.getElementById('release-value'),
            (value) => this.updateParameter('release', value),
            0, 100, 50
        );

        // Initialize keyboard
        this.keyboard = new KeyboardController(
            document.getElementById('keyboard'),
            (note, frequency) => this.handleNoteOn(note, frequency),
            (note) => this.handleNoteOff(note)
        );

        // Initialize visualizer
        this.visualizer = new Visualizer(document.getElementById('visualizer'));

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
            this.processor = new ClarinetProcessor();
            await this.processor.initialize();

            // Apply current parameter values
            this.updateParameter('breath', this.knobs.breath.value / 100);
            this.updateParameter('reed', this.knobs.reed.value / 100);
            this.updateParameter('noise', this.knobs.noise.value / 100);
            this.updateParameter('attack', this.knobs.attack.value / 100);
            this.updateParameter('damping', this.knobs.damping.value / 100);
            this.updateParameter('brightness', this.knobs.brightness.value / 100);
            this.updateParameter('vibrato', this.knobs.vibrato.value / 100);
            this.updateParameter('release', this.knobs.release.value / 100);

            // Start visualizer
            this.visualizer.start(() => this.processor.getAnalyserData());

            this.isActive = true;

            // Update UI
            document.getElementById('power-button').classList.add('active');
            this.updateStatus();

            console.log('Clarinet synthesizer powered on');
        } catch (error) {
            console.error('Failed to initialize audio:', error);
            alert('Failed to initialize audio. Please check browser compatibility.');
        }
    }

    powerOff() {
        if (this.processor) {
            this.keyboard.releaseAllKeys();
            this.processor.shutdown();
            this.processor = null;
        }

        this.visualizer.stop();
        this.isActive = false;
        this.currentNote = null;

        // Update UI
        document.getElementById('power-button').classList.remove('active');
        this.updateStatus();

        console.log('Clarinet synthesizer powered off');
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

    updateStatus() {
        const statusElement = document.getElementById('status');
        const noteElement = document.getElementById('current-note');

        statusElement.textContent = this.isActive ? 'ON' : 'OFF';
        statusElement.style.color = this.isActive ? '#4eff4a' : '#ff4a4a';

        noteElement.textContent = this.currentNote || '---';

        // Simulate CPU usage (would need actual implementation)
        if (this.isActive) {
            this.updateCPU();
        } else {
            document.getElementById('cpu').textContent = '0%';
        }
    }

    updateCPU() {
        // Rough approximation of CPU usage
        // In real implementation, would use performance.now() timing
        const usage = this.processor && this.processor.engine && this.processor.engine.isPlaying
            ? Math.floor(15 + Math.random() * 10)
            : Math.floor(5 + Math.random() * 5);
        document.getElementById('cpu').textContent = usage + '%';

        if (this.isActive) {
            setTimeout(() => this.updateCPU(), 100);
        }
    }
}

// Initialize app when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        window.synthApp = new ClarinetSynthApp();
    });
} else {
    window.synthApp = new ClarinetSynthApp();
}

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
    if (window.synthApp && window.synthApp.isActive) {
        window.synthApp.powerOff();
    }
});
