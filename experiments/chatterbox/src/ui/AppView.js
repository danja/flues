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
    this.currentPitch = 120; // Hz, updated by pitch slider
  }

  mount() {
    this.render();
    this.setupPowerButton();
    this.setupSourceControls();
    this.setupJoystick();
    this.setupFormantControls();
    this.setupEnvelopeControls();
    this.setupVocalModes();
    this.setupMidiControls();
    this.setupKeyboardShortcuts();
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
              <label for="pitch-slider">Base Pitch (Joystick/Space)</label>
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
          <div class="instructions">
            <p><strong>Click and drag</strong> on the canvas to speak. Use computer keyboard for pitched notes:</p>
            <p><kbd>Q</kbd>-<kbd>P</kbd> = C5-C6 (high) • <kbd>A</kbd>-<kbd>L</kbd> = C4-C5 (middle) • <kbd>Z</kbd>-<kbd>M</kbd> = C3-C4 (low) • <kbd>Space</kbd> = slider pitch</p>
            <p>Move joystick to morph vowels: <strong>i</strong> (top-left), <strong>e</strong>, <strong>a</strong> (bottom), <strong>o</strong>, <strong>u</strong> (top-right)</p>
          </div>
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
          <h2 class="panel__title">Voice Modes</h2>
          <div class="checkbox-group">
            <label>
              <input type="checkbox" data-nasal />
              Nasal
            </label>
            <label>
              <input type="checkbox" data-sing />
              Sing (Vibrato)
            </label>
            <label>
              <input type="checkbox" data-shout />
              Shout
            </label>
            <label>
              <input type="checkbox" data-fry />
              Fry (Vocal Fry)
            </label>
          </div>
          <div class="slider-control">
            <label for="stress-slider">Stress</label>
            <input type="range" id="stress-slider" min="0" max="100" value="30" data-stress-slider />
            <span data-stress-value>Relaxed</span>
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
    // Pitch slider - only affects joystick/spacebar, not keyboard/MIDI
    const pitchSlider = this.container.querySelector('[data-pitch-slider]');
    const pitchValue = this.container.querySelector('[data-pitch-value]');
    pitchSlider.addEventListener('input', (e) => {
      const normalized = e.target.value / 100;
      const hz = 80 * Math.pow(400 / 80, normalized);
      this.currentPitch = hz; // Store for joystick/spacebar notes
      pitchValue.textContent = formatHz(hz);
      // NOTE: Don't call engine.setPitch() - keyboard/MIDI set their own pitches
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
          const canvas = this.container.querySelector('[data-joystick]');
          canvas.classList.add('active');
          await this.handleNoteOn({
            midi: 60,
            frequency: this.currentPitch, // Use current pitch slider value
            velocity: 0.8,
          });
        }
      },
      onEnd: () => {
        // Trigger note off when joystick is released
        if (this.isJoystickActive) {
          this.isJoystickActive = false;
          const canvas = this.container.querySelector('[data-joystick]');
          canvas.classList.remove('active');
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

  setupVocalModes() {
    // Nasal checkbox
    const nasalCheckbox = this.container.querySelector('[data-nasal]');
    nasalCheckbox.addEventListener('change', (e) => {
      this.engine.setNasal(e.target.checked);
    });

    // Sing checkbox (vibrato)
    const singCheckbox = this.container.querySelector('[data-sing]');
    singCheckbox.addEventListener('change', (e) => {
      this.engine.setSing(e.target.checked);
    });

    // Shout checkbox
    const shoutCheckbox = this.container.querySelector('[data-shout]');
    shoutCheckbox.addEventListener('change', (e) => {
      this.engine.setShout(e.target.checked);
    });

    // Fry checkbox (vocal fry)
    const fryCheckbox = this.container.querySelector('[data-fry]');
    fryCheckbox.addEventListener('change', (e) => {
      this.engine.setFry(e.target.checked);
    });

    // Stress slider
    const stressSlider = this.container.querySelector('[data-stress-slider]');
    const stressValue = this.container.querySelector('[data-stress-value]');
    stressSlider.addEventListener('input', (e) => {
      const level = e.target.value / 100;
      this.engine.setStress(level);

      // Map to descriptive labels
      let label = 'Normal';
      if (level < 0.3) label = 'Soft';
      else if (level < 0.5) label = 'Relaxed';
      else if (level > 0.7) label = 'Loud';
      else if (level > 0.9) label = 'Very Loud';

      stressValue.textContent = label;
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

  setupKeyboardShortcuts() {
    // Computer keyboard to MIDI note mapping (piano-style layout)
    // Bottom row: Z X C V B N M , . /  = C3-B3
    // Middle row: A S D F G H J K L ;  = C4-B4
    // Top row:    Q W E R T Y U I O P  = C5-B5
    const keyToMidi = {
      // C3 octave (48-59)
      'KeyZ': 48, 'KeyX': 50, 'KeyC': 52, 'KeyV': 53, 'KeyB': 55,
      'KeyN': 57, 'KeyM': 59,
      // C4 octave (60-71) - Middle C
      'KeyA': 60, 'KeyS': 62, 'KeyD': 64, 'KeyF': 65, 'KeyG': 67,
      'KeyH': 69, 'KeyJ': 71, 'KeyK': 72, 'KeyL': 74,
      // C5 octave (72-83)
      'KeyQ': 72, 'KeyW': 74, 'KeyE': 76, 'KeyR': 77, 'KeyT': 79,
      'KeyY': 81, 'KeyU': 83, 'KeyI': 84, 'KeyO': 86, 'KeyP': 88,
      // Spacebar triggers with pitch slider value
      'Space': 'slider'
    };

    this.activeKeys = new Set();

    // Convert MIDI note to frequency
    const midiToFreq = (midi) => 440 * Math.pow(2, (midi - 69) / 12);

    window.addEventListener('keydown', async (e) => {
      const midiNote = keyToMidi[e.code];
      if (!midiNote || e.repeat) return;

      e.preventDefault();

      // Avoid duplicate triggers
      if (this.activeKeys.has(e.code)) return;
      this.activeKeys.add(e.code);

      let frequency, midi;
      if (midiNote === 'slider') {
        // Spacebar uses pitch slider (for joystick-like triggering)
        frequency = this.currentPitch;
        midi = 60; // Arbitrary MIDI note
        this.isJoystickActive = true;
        const canvas = this.container.querySelector('[data-joystick]');
        canvas.classList.add('active');
      } else {
        // Piano keys use their MIDI pitch
        midi = midiNote;
        frequency = midiToFreq(midi);
      }

      await this.handleNoteOn({
        midi,
        frequency,
        velocity: 0.8,
      });
    });

    window.addEventListener('keyup', (e) => {
      const midiNote = keyToMidi[e.code];
      if (!midiNote) return;

      e.preventDefault();
      this.activeKeys.delete(e.code);

      if (midiNote === 'slider') {
        if (this.isJoystickActive) {
          this.isJoystickActive = false;
          const canvas = this.container.querySelector('[data-joystick]');
          canvas.classList.remove('active');
          this.handleNoteOff({ midi: 60 });
        }
      } else {
        this.handleNoteOff({ midi: midiNote });
      }
    });
  }

  showError(error) {
    console.error('[Chatterbox] Error', error);
    window.dispatchEvent(new CustomEvent('chatterbox:error', { detail: error }));
  }
}
