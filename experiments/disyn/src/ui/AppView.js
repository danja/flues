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
  }

  mount() {
    this.render();
    this.setupAlgorithmControls();
    this.setupEnvelopeControls();
    this.setupReverbControls();
    this.setupKeyboard();
    this.setupPowerButton();
    this.setupMidiControls();

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
            <div class="keyboard__legend">A W S E D F T G Y H U J K</div>
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
    const keysContainer = this.container.querySelector('[data-keyboard-keys]');
    keysContainer.innerHTML = '';

    const whiteKeys = [
      { midi: 60, label: 'C4' },
      { midi: 62, label: 'D4' },
      { midi: 64, label: 'E4' },
      { midi: 65, label: 'F4' },
      { midi: 67, label: 'G4' },
      { midi: 69, label: 'A4' },
      { midi: 71, label: 'B4' },
      { midi: 72, label: 'C5' },
      { midi: 74, label: 'D5' },
      { midi: 76, label: 'E5' },
      { midi: 77, label: 'F5' },
      { midi: 79, label: 'G5' },
      { midi: 81, label: 'A5' },
    ];

    whiteKeys.forEach((key) => {
      const keyElement = document.createElement('div');
      keyElement.className = 'key';
      keyElement.dataset.midi = key.midi;
      keyElement.textContent = key.label;
      keysContainer.appendChild(keyElement);
    });

    this.keyboard = new KeyboardInput({
      container: keysContainer,
      baseMidiNote: 60,
      onNoteOn: (payload) => this.handleNoteOn(payload),
      onNoteOff: (payload) => this.handleNoteOff(payload),
    });
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
