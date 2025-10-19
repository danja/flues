import { DisynEngine } from '../audio/DisynEngine.js';
import { ALGORITHMS, getAlgorithmById, getDefaultParamState, resolveParamValue, DEFAULT_ALGORITHM_ID } from '../audio/AlgorithmRegistry.js';
import { KnobControl } from '@shared/ui/KnobControl.js';
import { KeyboardInput } from '@shared/ui/KeyboardInput.js';
import { MidiInputManager } from '@shared/midi/MidiInputManager.js';

const formatSeconds = (seconds) => {
  if (seconds < 0.01) {
    return `${Math.round(seconds * 1000)}ms`;
  }
  if (seconds < 1) {
    return `${(seconds * 1000).toFixed(0)}ms`;
  }
  return `${seconds.toFixed(2)}s`;
};

const mapAttackSeconds = (normalized) => {
  const min = 0.001;
  const max = 1.0;
  return min * Math.pow(max / min, normalized);
};

const mapReleaseSeconds = (normalized) => {
  const min = 0.01;
  const max = 3.0;
  return min * Math.pow(max / min, normalized);
};

const LARGE_SCREEN_QUERY = '(min-width: 900px)';
const KEY_SEQUENCE = ['a', 'w', 's', 'e', 'd', 'f', 't', 'g', 'y', 'h', 'u', 'j', 'k', 'o', 'l', 'p', ';', "'", ']', 'z', 'x', 'c', 'v', 'b', 'n'];

const NOTE_PATTERN = [
  { name: 'C', isBlack: false },
  { name: 'C#', isBlack: true },
  { name: 'D', isBlack: false },
  { name: 'D#', isBlack: true },
  { name: 'E', isBlack: false },
  { name: 'F', isBlack: false },
  { name: 'F#', isBlack: true },
  { name: 'G', isBlack: false },
  { name: 'G#', isBlack: true },
  { name: 'A', isBlack: false },
  { name: 'A#', isBlack: true },
  { name: 'B', isBlack: false },
];

const KEYBOARD_HORIZONTAL_PADDING = 18;

export class AppView {
  constructor(container) {
    this.container = container;
    this.engineState = null;

    this.engine = new DisynEngine({
      onStateChange: (state) => this.handleEngineState(state),
      onError: (error) => this.showError(error),
    });

    this.midi = new MidiInputManager({
      onNoteOn: (payload) => this.handleNoteOn(payload),
      onNoteOff: (payload) => this.handleNoteOff(payload),
      onActivity: () => this.flashMidiActivity(),
    });

    this.algorithmKnobs = new Map();
    this.envelopeKnobs = new Map();
    this.reverbKnobs = new Map();

    this.keyboardMedia = window.matchMedia(LARGE_SCREEN_QUERY);
    this.currentOctaves = this.keyboardMedia.matches ? 2 : 1;
    this.handleMediaChange = (event) => {
      const octaves = event.matches ? 2 : 1;
      this.updateKeyboardLayout(octaves);
    };
    this.handleResize = () => {
      this.updateKeyboardLayout(this.currentOctaves);
    };
  }

  mount() {
    this.render();
    this.setupAlgorithmControls();
    this.setupEnvelopeControls();
    this.setupReverbControls();
    this.setupKeyboard();
    this.setupPowerButton();
    this.setupMidiControls();
    this.keyboardMedia.addEventListener('change', this.handleMediaChange);
    window.addEventListener('resize', this.handleResize);

    // Prime UI with default algorithm settings
    this.updateAlgorithmParams(DEFAULT_ALGORITHM_ID, getDefaultParamState());
    this.updateStatusPill('pending');
  }

  render() {
    this.container.innerHTML = `
      <div class="app">
        <header class="app__header">
          <h1 class="app__title">Disyn Distortion Synth</h1>
          <div class="app__status">
            <span class="status-pill status-pill--off" data-status-pill>Power Off</span>
            <button class="power-button" data-power>Power</button>
          </div>
        </header>

        <section class="panel algorithm-panel">
          <h2 class="panel__title">Oscillator Algorithms</h2>
          <div class="algorithm-select">
            <div class="algorithm-select__control">
              <label for="algorithm-select">Algorithm</label>
              <select id="algorithm-select" data-algorithm-select>
                ${ALGORITHMS.map((alg) => `<option value="${alg.id}">${alg.label}</option>`).join('')}
              </select>
            </div>
            <p class="algorithm-select__description" data-algorithm-description></p>
          </div>
          <div class="controls-grid" data-algorithm-controls></div>
        </section>

        <section class="panel">
          <h2 class="panel__title">Envelope & Reverb</h2>
          <div class="controls-grid">
            <div data-envelope-controls class="controls-grid"></div>
            <div data-reverb-controls class="controls-grid"></div>
          </div>
        </section>

        <section class="panel">
          <h2 class="panel__title">Keyboard & MIDI</h2>
          <div class="keyboard">
            <div class="keyboard__keys" data-keyboard-keys></div>
            <div class="keyboard__legend" data-keyboard-legend></div>
          </div>
          <div class="midi-status">
            <span class="status-pill status-pill--off" data-midi-pill>MIDI Offline</span>
            <div class="midi-status__devices">
              <label for="midi-devices">Device</label>
              <select id="midi-devices" data-midi-select disabled>
                <option>No Devices</option>
              </select>
            </div>
            <label>
              <input type="checkbox" class="toggle" data-midi-toggle checked />
            </label>
            <div class="midi-activity" data-midi-activity></div>
          </div>
        </section>
      </div>
    `;
  }

  setupPowerButton() {
    const powerButton = this.container.querySelector('[data-power]');
    powerButton.addEventListener('click', async () => {
      try {
        if (!this.engine.audioContext || this.engine.audioContext.state === 'suspended') {
          await this.engine.ensureRunning();
        } else {
          this.engine.suspend();
        }
      } catch (error) {
        this.showError(error);
      }
    });
  }

  setupAlgorithmControls() {
    this.algorithmSelect = this.container.querySelector('[data-algorithm-select]');
    this.algorithmDescription = this.container.querySelector('[data-algorithm-description]');
    this.algorithmControlsContainer = this.container.querySelector('[data-algorithm-controls]');

    this.algorithmSelect.addEventListener('change', (event) => {
      const algorithmId = event.target.value;
      const algorithm = getAlgorithmById(algorithmId);
      this.algorithmDescription.textContent = algorithm.description;
      this.buildAlgorithmKnobs(algorithmId);
      this.engine.setAlgorithm(algorithmId);
    });

    const defaultAlgorithm = getAlgorithmById(DEFAULT_ALGORITHM_ID);
    this.algorithmDescription.textContent = defaultAlgorithm.description;
    this.buildAlgorithmKnobs(DEFAULT_ALGORITHM_ID);
  }

  buildAlgorithmKnobs(algorithmId) {
    this.algorithmControlsContainer.innerHTML = '';
    this.algorithmKnobs.clear();

    const algorithm = getAlgorithmById(algorithmId);
    const defaultState = getDefaultParamState(algorithmId);

    algorithm.params.forEach((param) => {
      const knobElement = document.createElement('div');
      knobElement.className = 'knob';
      knobElement.innerHTML = `
        <div class="knob__ring"></div>
        <div class="knob__label">${param.label}</div>
        <div class="knob__value" data-value>-</div>
      `;

      this.algorithmControlsContainer.appendChild(knobElement);

      const ring = knobElement.querySelector('.knob__ring');
      const valueElement = knobElement.querySelector('[data-value]');
      const normalized = defaultState[param.id] ?? param.default ?? 0.5;
      const resolved = resolveParamValue(algorithmId, param.id, normalized);

      valueElement.textContent = resolved.formatted;

      const knobControl = new KnobControl({
        element: ring,
        valueElement,
        initial: normalized * 100,
        onInput: (value) => {
          this.engine.setAlgorithmParam(param.id, value);
        },
        formatDisplay: (norm) => resolveParamValue(algorithmId, param.id, norm).formatted,
      });

      this.algorithmKnobs.set(param.id, { control: knobControl, valueElement });
    });
  }

  setupEnvelopeControls() {
    const container = this.container.querySelector('[data-envelope-controls]');
    container.innerHTML = '';

    const attack = this.createKnob({
      label: 'Attack',
      initial: 0.2,
      format: (norm) => formatSeconds(mapAttackSeconds(norm)),
      onChange: (value) => {
        this.engine.setEnvelope({ attack: value });
      },
    });

    const release = this.createKnob({
      label: 'Release',
      initial: 0.4,
      format: (norm) => formatSeconds(mapReleaseSeconds(norm)),
      onChange: (value) => {
        this.engine.setEnvelope({ release: value });
      },
    });

    container.appendChild(attack.element);
    container.appendChild(release.element);

    this.envelopeKnobs.set('attack', attack.control);
    this.envelopeKnobs.set('release', release.control);
  }

  setupReverbControls() {
    const container = this.container.querySelector('[data-reverb-controls]');
    container.innerHTML = '';

    const size = this.createKnob({
      label: 'Room Size',
      initial: 0.5,
      format: (norm) => `${Math.round(norm * 100)}%`,
      onChange: (value) => {
        this.engine.setReverb({ size: value });
      },
    });

    const level = this.createKnob({
      label: 'Wet Level',
      initial: 0.3,
      format: (norm) => `${Math.round(norm * 100)}%`,
      onChange: (value) => {
        this.engine.setReverb({ level: value });
      },
    });

    container.appendChild(size.element);
    container.appendChild(level.element);

    this.reverbKnobs.set('size', size.control);
    this.reverbKnobs.set('level', level.control);
  }

  createKnob({ label, initial = 0.5, format, onChange }) {
    const wrapper = document.createElement('div');
    wrapper.className = 'knob';
    wrapper.innerHTML = `
      <div class="knob__ring"></div>
      <div class="knob__label">${label}</div>
      <div class="knob__value">${format(initial)}</div>
    `;

    const ring = wrapper.querySelector('.knob__ring');
    const valueElement = wrapper.querySelector('.knob__value');

    const control = new KnobControl({
      element: ring,
      valueElement,
      initial: initial * 100,
      onInput: (normalized) => {
        valueElement.textContent = format(normalized);
        onChange(normalized);
      },
      formatDisplay: format,
    });

    return { element: wrapper, control };
  }

  setupKeyboard() {
    this.keysContainer = this.container.querySelector('[data-keyboard-keys]');
    this.keyboardLegend = this.container.querySelector('[data-keyboard-legend]');
    this.updateKeyboardLayout(this.currentOctaves);
  }

  updateKeyboardLayout(octaves) {
    this.currentOctaves = octaves;
    if (!this.keysContainer) return;

    const layout = this.generateKeyLayout(octaves);
    this.renderKeyboardLayers(layout);
    this.updateKeyboardLegend(octaves);
    this.initializeKeyboardInput(octaves);
  }

  initializeKeyboardInput(octaves) {
    if (this.keyboard && typeof this.keyboard.destroy === 'function') {
      this.keyboard.destroy();
    }

    this.keyboard = new KeyboardInput({
      container: this.keysContainer,
      baseMidiNote: 60,
      keyMap: this.getKeyMapForOctaves(octaves),
      onNoteOn: (payload) => this.handleNoteOn(payload),
      onNoteOff: (payload) => this.handleNoteOff(payload),
    });
  }

  generateKeyLayout(octaves) {
    const whiteKeys = [];
    const blackKeys = [];

    let midi = 60; // C4
    let octave = 4;
    let whiteIndex = 0;

    for (let o = 0; o < octaves; o++) {
      NOTE_PATTERN.forEach((step) => {
        const noteName = `${step.name}${octave}`;
        if (step.isBlack) {
          blackKeys.push({
            midi,
            name: noteName,
            precedingWhiteIndex: Math.max(whiteIndex - 1, 0),
          });
        } else {
          whiteKeys.push({
            midi,
            name: noteName,
            label: noteName,
          });
          whiteIndex += 1;
        }
        midi += 1;
        if (!step.isBlack && step.name === 'B') {
          octave += 1;
        }
      });
    }

    const topNoteName = `C${octave}`;
    whiteKeys.push({
      midi,
      name: topNoteName,
      label: topNoteName,
    });

    return {
      whiteKeys,
      blackKeys,
    };
  }

  renderKeyboardLayers({ whiteKeys, blackKeys }) {
    this.keysContainer.innerHTML = '';

    const whiteLayer = document.createElement('div');
    whiteLayer.className = 'keyboard__white-keys';

    const whiteElements = whiteKeys.map((key) => {
      const el = document.createElement('div');
      el.className = 'key white';
      el.dataset.midi = key.midi;
      el.dataset.note = key.name;
      el.innerHTML = `<span class="key__label">${key.label}</span>`;
      whiteLayer.appendChild(el);
      return el;
    });

    this.keysContainer.appendChild(whiteLayer);

    const whiteRects = whiteElements.map((el) => el.getBoundingClientRect());
    const containerRect = this.keysContainer.getBoundingClientRect();
    const paddingLeft = parseFloat(getComputedStyle(this.keysContainer).paddingLeft) || KEYBOARD_HORIZONTAL_PADDING;

    const blackLayer = document.createElement('div');
    blackLayer.className = 'keyboard__black-keys';

    blackKeys.forEach((key) => {
      const prevIndex = key.precedingWhiteIndex;
      const nextIndex = prevIndex + 1;
      const prevRect = whiteRects[prevIndex];
      const nextRect = whiteRects[nextIndex];
      if (!prevRect || !nextRect) return;

      const el = document.createElement('div');
      el.className = 'key black';
      el.dataset.midi = key.midi;
      el.dataset.note = key.name;

      const center = (prevRect.right + nextRect.left) / 2;
      const availableWidth = Math.min(prevRect.width, nextRect.width);
      const keyWidth = Math.max(availableWidth * 0.6, 20);
      const left = center - keyWidth / 2 - containerRect.left - paddingLeft;

      el.style.width = `${keyWidth}px`;
      el.style.left = `${left}px`;

      blackLayer.appendChild(el);
    });

    this.keysContainer.appendChild(blackLayer);
  }

  updateKeyboardLegend(octaves) {
    if (!this.keyboardLegend) return;
    const totalNotes = octaves * 12 + 1;
    const sequence = KEY_SEQUENCE.slice(0, totalNotes).map((key) => key.toUpperCase());

    if (octaves === 1) {
      this.keyboardLegend.textContent = sequence.join(' ');
    } else {
      const midpoint = Math.ceil(sequence.length / 2);
      const firstRow = sequence.slice(0, midpoint).join(' ');
      const secondRow = sequence.slice(midpoint).join(' ');
      this.keyboardLegend.innerHTML = `${firstRow}<br>${secondRow}`;
    }
  }

  getKeyMapForOctaves(octaves) {
    const totalNotes = octaves * 12 + 1;
    const map = {};
    KEY_SEQUENCE.slice(0, totalNotes).forEach((key, index) => {
      map[key] = index;
    });
    return map;
  }

  async handleNoteOn(payload) {
    try {
      await this.engine.ensureRunning();
      this.engine.noteOn(payload);
    } catch (error) {
      this.showError(error);
    }
  }

  handleNoteOff(payload) {
    this.engine.noteOff(payload);
  }

  setupMidiControls() {
    const midiToggle = this.container.querySelector('[data-midi-toggle]');
    const midiSelect = this.container.querySelector('[data-midi-select]');
    const midiPill = this.container.querySelector('[data-midi-pill]');

    midiToggle.addEventListener('change', (event) => {
      const enabled = event.target.checked;
      this.midi.setEnabled(enabled);
      this.updateMidiStatus(enabled);
    });

    midiSelect.addEventListener('change', (event) => {
      const inputId = event.target.value;
      this.midi.selectInput(inputId);
    });

    this.midi.onDeviceChange = (devices) => {
      midiSelect.innerHTML = '';
      if (devices.length === 0) {
        midiSelect.innerHTML = '<option>No Devices</option>';
        midiSelect.disabled = true;
        return;
      }

      devices.forEach((device) => {
        const option = document.createElement('option');
        option.value = device.id;
        option.textContent = `${device.name}`;
        midiSelect.appendChild(option);
      });
      midiSelect.disabled = false;
    };

    this.initMidi().then((success) => {
      midiPill.textContent = success ? 'MIDI Ready' : 'MIDI Unavailable';
      midiPill.classList.toggle('status-pill--off', !success);
      this.updateMidiStatus(success && midiToggle.checked);
    });
  }

  async initMidi() {
    const success = await this.midi.initialize();
    return success;
  }

  updateMidiStatus(enabled) {
    const midiPill = this.container.querySelector('[data-midi-pill]');
    if (!midiPill) return;

    if (enabled) {
      midiPill.textContent = 'MIDI Active';
      midiPill.classList.remove('status-pill--off');
    } else {
      midiPill.textContent = 'MIDI Muted';
      midiPill.classList.add('status-pill--off');
    }
  }

  flashMidiActivity() {
    const dot = this.container.querySelector('[data-midi-activity]');
    if (!dot) return;

    dot.classList.add('midi-activity--on');
    clearTimeout(this.midiActivityTimeout);
    this.midiActivityTimeout = setTimeout(() => {
      dot.classList.remove('midi-activity--on');
    }, 150);
  }

  handleEngineState(state) {
    this.engineState = state;
    this.updateStatusPill(state.contextState);
    this.updateAlgorithmState(state);
  }

  updateStatusPill(contextState) {
    const pill = this.container.querySelector('[data-status-pill]');
    if (!pill) return;

    let text = 'Power Off';
    let off = false;

    switch (contextState) {
      case 'running':
        text = 'Audio Running';
        off = false;
        break;
      case 'suspended':
        text = 'Suspended';
        off = true;
        break;
      case 'closed':
        text = 'Closed';
        off = true;
        break;
      default:
        text = 'Ready';
        off = true;
    }

    pill.textContent = text;
    pill.classList.toggle('status-pill--off', off);
  }

  updateAlgorithmState(state) {
    if (!state?.params) return;
    const algorithm = getAlgorithmById(state.algorithmId);
    algorithm.params.forEach((param) => {
      const knob = this.algorithmKnobs.get(param.id);
      if (!knob) return;

      const resolved = state.params[param.id];
      if (!resolved) return;

      const normalized = resolved.normalized;
      knob.control.setNormalized(normalized, { emit: false });
    });
  }

  updateAlgorithmParams(algorithmId, params) {
    const algorithm = getAlgorithmById(algorithmId);
    algorithm.params.forEach((param) => {
      const knob = this.algorithmKnobs.get(param.id);
      if (!knob) return;
      const normalized = params[param.id] ?? param.default ?? 0.5;
      knob.control.setNormalized(normalized, { emit: false });
    });
  }

  showError(error) {
    console.error('[Disyn] Error', error);
    window.dispatchEvent(new CustomEvent('disyn:error', { detail: error }));
  }
}
