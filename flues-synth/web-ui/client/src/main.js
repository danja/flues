/**
 * Flues Web UI - Main Entry Point
 * Wires 29 parameters to WebSocket MIDI control
 */

import { WebSocketClient } from './api/WebSocketClient.js';
import { KnobController } from './ui/KnobController.js';
import { RotarySwitchController } from './ui/RotarySwitchController.js';
import { Visualizer } from './ui/Visualizer.js';
import { VoiceMeters } from './ui/VoiceMeters.js';
import { PARAMETERS, paramToMidi, formatParamValue } from './utils/parameterMaps.js';

// WebSocket client
const wsUrl = `ws://${window.location.hostname}:8081`;
const ws = new WebSocketClient(wsUrl);

// Connection status
const statusIndicator = document.getElementById('status-indicator');
const statusText = document.getElementById('status-text');

ws.onConnect(() => {
    statusIndicator.className = 'status-indicator connected';
    statusText.textContent = 'Connected';
    console.log('✓ Connected to flues-synth');
});

ws.onDisconnect(() => {
    statusIndicator.className = 'status-indicator disconnected';
    statusText.textContent = 'Disconnected (reconnecting...)';
    console.log('✗ Disconnected from server');
});

// Start connection
ws.connect();

// Visualizer
const visualizer = new Visualizer(document.getElementById('visualizer-canvas'));
visualizer.start();

ws.onAudio((audioData) => {
    visualizer.update(audioData);
    voiceMeters.update(audioData.voiceCount);
});

// Voice meters
const voiceMeters = new VoiceMeters(document.getElementById('voice-meters'));

/**
 * Create knob controller and wire to WebSocket
 * @param {string} paramName - Parameter name from PARAMETERS
 * @param {HTMLElement} container - Container element for knob
 * @param {string} label - Display label
 * @returns {KnobController}
 */
function createKnob(paramName, container, label) {
    const param = PARAMETERS[paramName];
    const defaultValue = param.default;

    // Normalize default value to 0-1 range
    let normalizedDefault;
    if (param.map === 'linear' || param.map === 'exponential') {
        normalizedDefault = (defaultValue - param.min) / (param.max - param.min);
    } else if (param.map === 'bipolar') {
        normalizedDefault = (defaultValue + 1) / 2;  // -1..+1 → 0..1
    } else if (param.map === 'discrete') {
        normalizedDefault = defaultValue / param.max;
    } else {
        normalizedDefault = defaultValue;
    }

    const knob = new KnobController(container, {
        label: label,
        value: normalizedDefault,
        min: 0,
        max: 1,
        step: 0.001,
        onChange: (value) => {
            const midiValue = paramToMidi(paramName, value);
            ws.sendCC(param.cc, midiValue);

            // Update value display
            const displayValue = formatParamValue(paramName, value);
            knob.updateValueDisplay(displayValue);
        }
    });

    // Set initial value display
    knob.updateValueDisplay(formatParamValue(paramName, normalizedDefault));

    return knob;
}

/**
 * Create rotary switch controller
 * @param {string} paramName - Parameter name
 * @param {HTMLElement} container - Container element
 * @param {Array<string>} options - Option labels
 * @returns {RotarySwitchController}
 */
function createRotarySwitch(paramName, container, options) {
    const param = PARAMETERS[paramName];
    const defaultValue = param.default || 0;

    const rotary = new RotarySwitchController(container, {
        options: options,
        value: defaultValue,
        onChange: (index) => {
            const normalizedValue = index / (options.length - 1);
            const midiValue = paramToMidi(paramName, normalizedValue);
            ws.sendCC(param.cc, midiValue);
        }
    });

    return rotary;
}

/**
 * Create toggle button
 * @param {string} paramName - Parameter name
 * @param {HTMLElement} container - Container element
 * @param {string} label - Display label
 */
function createToggle(paramName, container, label) {
    const param = PARAMETERS[paramName];
    const button = document.createElement('button');
    button.className = 'toggle-button';
    button.textContent = label;
    button.dataset.param = paramName;

    let isOn = param.default || false;

    const updateState = () => {
        button.classList.toggle('active', isOn);
        const midiValue = isOn ? 127 : 0;
        ws.sendCC(param.cc, midiValue);
    };

    button.addEventListener('click', () => {
        isOn = !isOn;
        updateState();
    });

    updateState();
    container.appendChild(button);

    return button;
}

// Initialize all controls

// === DISYN SOURCE ===
const disynContainer = document.getElementById('disyn-controls');
createRotarySwitch('disynAlgorithm', disynContainer.querySelector('[data-control="algorithm"]'),
    ['Dirichlet', 'DSF Single', 'DSF Double', 'Tanh Square', 'Tanh Saw', 'PAF', 'Modified FM']);
createKnob('disynParam1', disynContainer.querySelector('[data-control="param1"]'), 'Param 1');
createKnob('disynParam2', disynContainer.querySelector('[data-control="param2"]'), 'Param 2');
createKnob('disynLevel', disynContainer.querySelector('[data-control="level"]'), 'Level');
createKnob('noiseLevel', disynContainer.querySelector('[data-control="noise"]'), 'Noise');
createKnob('dcLevel', disynContainer.querySelector('[data-control="dc"]'), 'DC');

// === FORMANTS ===
const formantsContainer = document.getElementById('formants-controls');
createKnob('f1', formantsContainer.querySelector('[data-control="f1"]'), 'F1 (Jaw)');
createKnob('f2', formantsContainer.querySelector('[data-control="f2"]'), 'F2 (Tongue)');
createKnob('f3', formantsContainer.querySelector('[data-control="f3"]'), 'F3 (Lips)');
createKnob('f4', formantsContainer.querySelector('[data-control="f4"]'), 'F4 (Quality)');

const vocalModesContainer = formantsContainer.querySelector('[data-control="vocal-modes"]');
createToggle('nasal', vocalModesContainer, 'Nasal');
createToggle('sing', vocalModesContainer, 'Sing');
createToggle('shout', vocalModesContainer, 'Shout');
createToggle('fry', vocalModesContainer, 'Fry');

// === ENVELOPE ===
const envelopeContainer = document.getElementById('envelope-controls');
createKnob('attack', envelopeContainer.querySelector('[data-control="attack"]'), 'Attack');
createKnob('release', envelopeContainer.querySelector('[data-control="release"]'), 'Release');

// === INTERFACE ===
const interfaceContainer = document.getElementById('interface-controls');
createRotarySwitch('interfaceType', interfaceContainer.querySelector('[data-control="type"]'),
    ['Pluck', 'Hit', 'Reed', 'Flute', 'Brass', 'Bow', 'Bell', 'Drum', 'Crystal', 'Vapor', 'Quantum', 'Plasma']);
createKnob('intensity', interfaceContainer.querySelector('[data-control="intensity"]'), 'Intensity');
createKnob('tuning', interfaceContainer.querySelector('[data-control="tuning"]'), 'Tuning');
createKnob('ratio', interfaceContainer.querySelector('[data-control="ratio"]'), 'Ratio');

// === FEEDBACK ===
const feedbackContainer = document.getElementById('feedback-controls');
createKnob('delay1Feedback', feedbackContainer.querySelector('[data-control="delay1"]'), 'Delay 1');
createKnob('delay2Feedback', feedbackContainer.querySelector('[data-control="delay2"]'), 'Delay 2');
createKnob('filterFeedback', feedbackContainer.querySelector('[data-control="filter"]'), 'Filter');

// === FILTER ===
const filterContainer = document.getElementById('filter-controls');
createKnob('filterFreq', filterContainer.querySelector('[data-control="freq"]'), 'Frequency');
createKnob('filterQ', filterContainer.querySelector('[data-control="q"]'), 'Q');
createKnob('filterShape', filterContainer.querySelector('[data-control="shape"]'), 'Shape');

// === MODULATION ===
const modulationContainer = document.getElementById('modulation-controls');
createKnob('lfoFreq', modulationContainer.querySelector('[data-control="lfo-freq"]'), 'LFO Freq');
createKnob('amFmDepth', modulationContainer.querySelector('[data-control="am-fm"]'), 'AM ↔ FM');

// === OUTPUT ===
const outputContainer = document.getElementById('output-controls');
createKnob('masterGain', outputContainer.querySelector('[data-control="master"]'), 'Master Gain');

console.log('✓ Flues Web UI initialized - 29 parameters wired');
