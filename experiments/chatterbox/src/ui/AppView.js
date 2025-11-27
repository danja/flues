// AppView.js
// Main UI coordinator for Chatterbox speech synthesizer

import { ChatterboxEngine } from '../audio/ChatterboxEngine.js';
import { JoystickControl } from './JoystickControl.js';
import { MidiInputManager } from '@shared/midi/MidiInputManager.js';

const formatHz = (hz) => `${Math.round(hz)} Hz`;
const formatSeconds = (seconds) => {
  if (seconds < 0.01) return `${Math.round(seconds * 1000)}ms`;
  if (seconds < 1) return `${(seconds * 1000).toFixed(0)}ms`;
  return `${seconds.toFixed(2)}s`;
};

export class AppView {
  constructor(container) {
    this.container = container;
    this.engineState = null;

    this.engine = new ChatterboxEngine({
      onStateChange: (state) => this.handleEngineState(state),
      onError: (error) => this.showError(error),
    });

    this.midi = new MidiInputManager({
      onNoteOn: (payload) => this.handleNoteOn(payload),
      onNoteOff: (payload) => this.handleNoteOff(payload),
      onActivity: () => this.flashMidiActivity(),
    });

    this.joystick = null;
    this.isJoystickActive = false;
  }

  mount() {
    this.render();
    this.setupPowerButton();
    this.setupSourceControls();
    this.setupJoystick();
    this.setupFormantControls();
    this.setupEnvelopeControls();
    this.setupMidiControls();
    this.updateStatusPill('pending');
  }

  render() {
    this.container.innerHTML = `
      <div class="app">
        <header class="app__header">
          <h1 class="app__title">Chatterbox Speech Synthesizer</h1>
          <div class="app__status">
            <span class="status-pill status-pill--off" data-status-pill>Power Off</span>
            <button class="power-button" data-power>Power</button>
          </div>
        </header>

        <section class="panel">
          <h2 class="panel__title">Source</h2>
          <div class="source-controls">
            <div class="slider-control">
              <label for="pitch-slider">Pitch</label>
              <input type="range" id="pitch-slider" min="0" max="100" value="30" data-pitch-slider />
              <span data-pitch-value>120 Hz</span>
            </div>
            <div class="slider-control">
              <label for="noise-slider">Noise Level</label>
              <input type="range" id="noise-slider" min="0" max="100" value="20" data-noise-slider />
              <span data-noise-value>20%</span>
            </div>
            <div class="checkbox-group">
              <label>
                <input type="checkbox" data-voiced checked />
                Voiced
              </label>
              <label>
                <input type="checkbox" data-aspirated />
                Aspirated
              </label>
            </div>
          </div>
        </section>

        <section class="panel">
          <h2 class="panel__title">Vowel Space (F1 & F2)</h2>
          <div class="joystick-container">
            <canvas data-joystick width="500" height="350"></canvas>
          </div>
          <div class="formant-readout">
            <span>F1: <span data-f1-readout>500 Hz</span></span>
            <span>F2: <span data-f2-readout>1500 Hz</span></span>
          </div>
        </section>

        <section class="panel">
          <h2 class="panel__title">Formants F3 & F4</h2>
          <div class="formant-controls">
            <div class="slider-control">
              <label for="f3-slider">F3 Frequency</label>
              <input type="range" id="f3-slider" min="0" max="100" value="50" data-f3-slider />
              <span data-f3-value>2500 Hz</span>
            </div>
            <div class="slider-control">
              <label for="f4-slider">F4 Frequency</label>
              <input type="range" id="f4-slider" min="0" max="100" value="50" data-f4-slider />
              <span data-f4-value>3500 Hz</span>
            </div>
          </div>
        </section>

        <section class="panel">
          <h2 class="panel__title">Envelope</h2>
          <div class="envelope-controls">
            <div class="slider-control">
              <label for="attack-slider">Attack</label>
              <input type="range" id="attack-slider" min="0" max="100" value="5" data-attack-slider />
              <span data-attack-value>10ms</span>
            </div>
            <div class="slider-control">
              <label for="release-slider">Release</label>
              <input type="range" id="release-slider" min="0" max="100" value="30" data-release-slider />
              <span data-release-value>150ms</span>
            </div>
          </div>
        </section>

        <section class="panel">
          <h2 class="panel__title">Voice Modes (Future Features)</h2>
          <div class="checkbox-group">
            <label>
              <input type="checkbox" disabled />
              Nasal
            </label>
            <label>
              <input type="checkbox" disabled />
              Sing
            </label>
            <label>
              <input type="checkbox" disabled />
              Shout
            </label>
            <label>
              <input type="checkbox" disabled />
              Fry
            </label>
          </div>
        </section>

        <section class="panel">
          <h2 class="panel__title">MIDI</h2>
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

  setupSourceControls() {
    // Pitch slider
    const pitchSlider = this.container.querySelector('[data-pitch-slider]');
    const pitchValue = this.container.querySelector('[data-pitch-value]');
    pitchSlider.addEventListener('input', (e) => {
      const normalized = e.target.value / 100;
      this.engine.setPitch(normalized);
      const hz = 80 * Math.pow(400 / 80, normalized);
      pitchValue.textContent = formatHz(hz);
    });

    // Noise level slider
    const noiseSlider = this.container.querySelector('[data-noise-slider]');
    const noiseValue = this.container.querySelector('[data-noise-value]');
    noiseSlider.addEventListener('input', (e) => {
      const level = e.target.value / 100;
      this.engine.setNoiseLevel(level);
      noiseValue.textContent = `${Math.round(level * 100)}%`;
    });

    // Voiced checkbox
    const voicedCheckbox = this.container.querySelector('[data-voiced]');
    voicedCheckbox.addEventListener('change', (e) => {
      this.engine.setVoiced(e.target.checked);
    });

    // Aspirated checkbox
    const aspiratedCheckbox = this.container.querySelector('[data-aspirated]');
    aspiratedCheckbox.addEventListener('change', (e) => {
      this.engine.setAspirated(e.target.checked);
    });
  }

  setupJoystick() {
    const canvas = this.container.querySelector('[data-joystick]');
    const f1Readout = this.container.querySelector('[data-f1-readout]');
    const f2Readout = this.container.querySelector('[data-f2-readout]');

    this.joystick = new JoystickControl({
      canvas,
      width: 500,
      height: 350,
      onInput: (x, y) => {
        // X maps to F2 (inverted: left=high, right=low)
        const f2Normalized = 1 - x;
        this.engine.setFormant(1, f2Normalized);
        const f2Hz = 500 * Math.pow(3000 / 500, f2Normalized);
        f2Readout.textContent = formatHz(f2Hz);

        // Y maps to F1 (inverted: top=low, bottom=high)
        const f1Normalized = y;
        this.engine.setFormant(0, f1Normalized);
        const f1Hz = 200 * Math.pow(1000 / 200, f1Normalized);
        f1Readout.textContent = formatHz(f1Hz);
      },
      onStart: async () => {
        // Trigger note on when joystick is clicked
        if (!this.isJoystickActive) {
          this.isJoystickActive = true;
          await this.handleNoteOn({
            midi: 60,
            frequency: 120,
            velocity: 0.8,
          });
        }
      },
      onEnd: () => {
        // Trigger note off when joystick is released
        if (this.isJoystickActive) {
          this.isJoystickActive = false;
          this.handleNoteOff({ midi: 60 });
        }
      },
    });
  }

  setupFormantControls() {
    // F3 slider
    const f3Slider = this.container.querySelector('[data-f3-slider]');
    const f3Value = this.container.querySelector('[data-f3-value]');
    f3Slider.addEventListener('input', (e) => {
      const normalized = e.target.value / 100;
      this.engine.setFormant(2, normalized);
      const hz = 1500 * Math.pow(4000 / 1500, normalized);
      f3Value.textContent = formatHz(hz);
    });

    // F4 slider
    const f4Slider = this.container.querySelector('[data-f4-slider]');
    const f4Value = this.container.querySelector('[data-f4-value]');
    f4Slider.addEventListener('input', (e) => {
      const normalized = e.target.value / 100;
      this.engine.setFormant(3, normalized);
      const hz = 2500 * Math.pow(4500 / 2500, normalized);
      f4Value.textContent = formatHz(hz);
    });
  }

  setupEnvelopeControls() {
    // Attack slider
    const attackSlider = this.container.querySelector('[data-attack-slider]');
    const attackValue = this.container.querySelector('[data-attack-value]');
    attackSlider.addEventListener('input', (e) => {
      const normalized = e.target.value / 100;
      const seconds = 0.001 * Math.pow(1.0 / 0.001, normalized);
      this.engine.setEnvelope({ attack: normalized });
      attackValue.textContent = formatSeconds(seconds);
    });

    // Release slider
    const releaseSlider = this.container.querySelector('[data-release-slider]');
    const releaseValue = this.container.querySelector('[data-release-value]');
    releaseSlider.addEventListener('input', (e) => {
      const normalized = e.target.value / 100;
      const seconds = 0.01 * Math.pow(3.0 / 0.01, normalized);
      this.engine.setEnvelope({ release: normalized });
      releaseValue.textContent = formatSeconds(seconds);
    });
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
        option.textContent = device.name;
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

  handleEngineState(state) {
    this.engineState = state;
    this.updateStatusPill(state.contextState);
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

  showError(error) {
    console.error('[Chatterbox] Error', error);
    window.dispatchEvent(new CustomEvent('chatterbox:error', { detail: error }));
  }
}
