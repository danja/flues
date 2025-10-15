// main.js
// Main application initialization and coordination

import { PMSynthProcessor } from './audio/PMSynthProcessor.js';
import { InterfaceType } from './audio/PMSynthEngine.js';
import { KnobController } from './ui/KnobController.js';
import { RotarySwitchController } from './ui/RotarySwitchController.js';
import { KeyboardController } from './ui/KeyboardController.js';
import { MidiController } from './ui/MidiController.js';
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
    DEFAULT_REVERB_SIZE,
    DEFAULT_REVERB_LEVEL,
    INTERFACE_TYPE_NAMES
} from './constants.js';

if ('serviceWorker' in navigator) {
    window.addEventListener('load', () => {
        navigator.serviceWorker.register('sw.js').catch((err) => {
            console.warn('Service worker registration failed', err);
        });
    });
}

let deferredInstallPrompt = null;

function setupInstallPromptUI() {
    const installButton = document.getElementById('install-button');
    if (!installButton) return;

    window.addEventListener('beforeinstallprompt', (event) => {
        event.preventDefault();
        deferredInstallPrompt = event;
        installButton.classList.add('visible');
    });

    installButton.addEventListener('click', async () => {
        if (!deferredInstallPrompt) return;
        try {
            installButton.disabled = true;
            await deferredInstallPrompt.prompt();
            await deferredInstallPrompt.userChoice;
        } catch (err) {
            console.warn('Install prompt failed', err);
        } finally {
            deferredInstallPrompt = null;
            installButton.disabled = false;
            installButton.classList.remove('visible');
        }
    });

    window.addEventListener('appinstalled', () => {
        deferredInstallPrompt = null;
        installButton.classList.remove('visible');
        installButton.disabled = true;
    });
}

setupInstallPromptUI();

class PMSynthApp {
    constructor() {
        this.processor = null;
        this.keyboard = null;
        this.midi = null;
        this.visualizer = null;
        this.knobs = {};
        this.switches = {};
        this.isActive = false;
        this.currentNote = null;

        this.initializeUI();
        this.initializeMidi();
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

        // Initialize Reverb knobs
        this.knobs.reverbSize = new KnobController(
            document.getElementById('reverb-size-knob'),
            document.getElementById('reverb-size-value'),
            (value) => this.updateParameter('reverbSize', value),
            0, 100, DEFAULT_REVERB_SIZE
        );

        this.knobs.reverbLevel = new KnobController(
            document.getElementById('reverb-level-knob'),
            document.getElementById('reverb-level-value'),
            (value) => this.updateParameter('reverbLevel', value),
            0, 100, DEFAULT_REVERB_LEVEL
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
            const statsElement = document.getElementById('visualizer-stats');
            this.visualizer = new Visualizer(visualizerElement, statsElement);
        }

        // Update status display
        this.updateStatus();
    }

    async initializeMidi() {
        this.midi = new MidiController(
            (note, frequency, velocity) => this.handleNoteOn(note, frequency),
            (note) => this.handleNoteOff(note)
        );

        const midiAvailable = await this.midi.initialize();

        if (midiAvailable) {
            console.log('[PMSynthApp] MIDI initialized successfully');

            // Set up callbacks for UI updates
            this.midi.onDeviceChange = (devices) => {
                this.updateMidiDeviceList(devices);
            };

            this.midi.onActivity = () => {
                this.showMidiActivity();
            };

            // Initialize MIDI UI
            this.updateMidiDeviceList(this.midi.getInputDevices());
        } else {
            console.log('[PMSynthApp] MIDI not available');
            this.hideMidiPanel();
        }
    }

    updateMidiDeviceList(devices) {
        const select = document.getElementById('midi-device-select');
        if (!select) return;

        // Clear existing options
        select.innerHTML = '<option value="">No device</option>';

        // Add available devices
        devices.forEach(device => {
            const option = document.createElement('option');
            option.value = device.id;
            option.textContent = device.name;
            if (this.midi.selectedInput && this.midi.selectedInput.id === device.id) {
                option.selected = true;
            }
            select.appendChild(option);
        });

        // Update status indicator
        this.updateMidiStatus(devices.length > 0);
    }

    updateMidiStatus(hasDevices) {
        const statusIndicator = document.getElementById('midi-status-indicator');
        const statusText = document.getElementById('midi-status-text');
        if (!statusIndicator || !statusText) return;

        if (!hasDevices) {
            statusIndicator.className = 'midi-status-indicator disconnected';
            statusText.textContent = 'No devices';
        } else if (this.midi.selectedInput) {
            statusIndicator.className = 'midi-status-indicator connected';
            statusText.textContent = 'Connected';
        } else {
            statusIndicator.className = 'midi-status-indicator warning';
            statusText.textContent = 'Select device';
        }
    }

    showMidiActivity() {
        const indicator = document.getElementById('midi-activity-indicator');
        if (!indicator) return;

        indicator.classList.add('active');
        setTimeout(() => {
            indicator.classList.remove('active');
        }, 100);
    }

    hideMidiPanel() {
        const panel = document.getElementById('midi-panel');
        if (panel) {
            panel.style.display = 'none';
        }
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

            console.log('Stove powered on');
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
            if (this.midi) {
                this.midi.releaseAllNotes();
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

        console.log('Stove powered off');
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
            modulationTypeLevel: 'modulationTypeLevel',
            reverbSize: 'reverbSize',
            reverbLevel: 'reverbLevel'
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

// Setup MIDI panel event listeners after app initialization
document.addEventListener('DOMContentLoaded', () => {
    const app = window.synthApp;
    if (!app) return;

    // MIDI device selector
    const deviceSelect = document.getElementById('midi-device-select');
    if (deviceSelect) {
        deviceSelect.addEventListener('change', (e) => {
            if (app.midi) {
                app.midi.selectInput(e.target.value);
                app.updateMidiStatus(app.midi.getInputDevices().length > 0);
            }
        });
    }

    // MIDI channel selector
    const channelSelect = document.getElementById('midi-channel-select');
    if (channelSelect) {
        channelSelect.addEventListener('change', (e) => {
            if (app.midi) {
                app.midi.setChannel(e.target.value);
            }
        });
    }

    // MIDI enable toggle
    const enableToggle = document.getElementById('midi-enable-toggle');
    if (enableToggle) {
        enableToggle.addEventListener('change', (e) => {
            if (app.midi) {
                app.midi.setEnabled(e.target.checked);
            }
        });
    }

    // MIDI panic button (all notes off)
    const panicButton = document.getElementById('midi-panic-button');
    if (panicButton) {
        panicButton.addEventListener('click', () => {
            if (app.midi) {
                app.midi.releaseAllNotes();
            }
        });
    }

    // MIDI panel collapse/expand toggle
    const toggleButton = document.getElementById('midi-panel-toggle');
    const panelContent = document.getElementById('midi-panel-content');
    if (toggleButton && panelContent) {
        toggleButton.addEventListener('click', () => {
            panelContent.classList.toggle('collapsed');
            toggleButton.textContent = panelContent.classList.contains('collapsed') ? '▼' : '▲';
        });
    }
});

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
    if (window.synthApp) {
        if (window.synthApp.isActive) {
            window.synthApp.powerOff();
        }
        if (window.synthApp.midi) {
            window.synthApp.midi.shutdown();
        }
    }
});
