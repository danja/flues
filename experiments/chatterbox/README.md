# Chatterbox Speech Synthesizer

A real-time speech synthesis application using formant filtering and source-filter vocal tract modeling. Built with Web Audio API and runs entirely in the browser.

**[Try it live](https://danja.github.io/flues/chatterbox/)**

## Overview

Chatterbox simulates human speech production using a classic source-filter model:

- **Source:** Larynx (sawtooth oscillator) + Aspirator (noise generator)
- **Filter:** Cascade of four formant bandpass filters (F1-F4)
- **Control:** Interactive IPA vowel quadrilateral joystick

## Features

- **Interactive Joystick:** 2D canvas control mapped to IPA vowel space
  - X-axis controls F2 (tongue front/back)
  - Y-axis controls F1 (jaw opening)
  - Visual markers for vowels: i, e, a, o, u
- **Real-time Formant Control:** Four independently adjustable formants
- **Dual Excitation Sources:**
  - Voiced (larynx): Modified sawtooth wave
  - Aspirated (noise): White noise generator
- **Pitch Control:**
  - Computer keyboard: 3-octave piano layout (C3-E6)
  - MIDI keyboard: Full note range with proper pitch
  - Pitch slider: Base pitch for joystick/spacebar (80-400 Hz)
- **Envelope Shaping:** Attack/release controls
- **Vocal Modes:**
  - **Nasal:** Adds 250 Hz nasal formant in parallel
  - **Sing:** Vibrato (5.5 Hz, ±1.5% pitch modulation)
  - **Shout:** Boosts formant frequencies by 15% and increases noise
  - **Fry:** Adds subharmonic at octave below (vocal fry)
  - **Stress:** Amplitude control (0.5-2.0x) with soft clipping at high levels
- **Keyboard Shortcuts:** Spacebar to trigger sound
- **MIDI Support:** External keyboard control
- **Visual Feedback:** Canvas glows when speaking

## How to Use

### Running Locally

```bash
cd experiments/chatterbox
npm install
npm run dev
```

Open http://localhost:5173/ in your browser.

### Controls

1. **Power Button:** Click to initialize audio context
2. **Vowel Canvas:**
   - Click and drag to speak
   - Move around to morph between vowels
   - Spacebar for hands-free triggering
3. **Pitch Slider:** Adjust fundamental frequency (80-400 Hz)
4. **Source Controls:**
   - Voiced checkbox: Enable/disable larynx
   - Aspirated checkbox: Enable/disable noise
   - Noise Level: Control aspiration amount
5. **Formant Sliders:** Fine-tune F3 and F4 frequencies
6. **Envelope:** Adjust attack and release times
7. **Vocal Modes:**
   - Nasal: Add nasal resonance (parallel 250 Hz formant)
   - Sing: Add vibrato to pitch (5.5 Hz LFO)
   - Shout: Boost formants and noise for louder voice
   - Fry: Add vocal fry (subharmonic at octave below)
   - Stress: Control amplitude from Soft to Very Loud with distortion

### Keyboard Shortcuts

**Pitched Keyboard (Piano Layout):**
- **Q-P row:** C5-C6 (high octave)
  - Q=C5, W=D5, E=E5, R=F5, T=G5, Y=A5, U=B5, I=C6, O=D6, P=E6
- **A-L row:** C4-C5 (middle octave, contains Middle C)
  - A=C4, S=D4, D=E4, F=F4, G=G4, H=A4, J=B4, K=C5, L=D5
- **Z-M row:** C3-C4 (low octave)
  - Z=C3, X=D3, C=E3, V=F3, B=G3, N=A3, M=B3
- **Spacebar:** Trigger sound using pitch slider value (for joystick-like control)

**MIDI Input:** External MIDI keyboard sends proper note pitches

**Mouse/Touch:** Click and drag on joystick canvas to control formants (vowel position)

## Architecture

### Signal Flow

```
Larynx (sawtooth) + Aspirator (noise)
    ↓
FormantBank (F1→F2→F3→F4 cascade)
    ↓
Envelope (AR)
    ↓
Reverb (Schroeder)
    ↓
Output
```

### DSP Modules

- **LarynxModule:** Modified sawtooth oscillator with waveshaping, vibrato, and vocal fry
- **AspiratorModule:** White noise generator with level control
- **FormantModule:** Resonant bandpass filter (biquad implementation)
- **FormantBankModule:** Cascade of 4 formants + optional nasal formant (250 Hz)
- **EnvelopeModule:** Attack/release envelope generator
- **ReverbModule:** Schroeder reverb for ambience
- **Vocal Processing:** Stress (gain + soft clipping), shout mode (formant boost)

### Files

```
experiments/chatterbox/
├── src/
│   ├── audio/
│   │   ├── ChatterboxEngine.js       # Main coordinator
│   │   ├── chatterbox-worklet.js     # AudioWorklet processor
│   │   └── modules/
│   │       ├── LarynxModule.js       # Voiced excitation
│   │       ├── AspiratorModule.js    # Noise excitation
│   │       ├── FormantModule.js      # Single bandpass filter
│   │       ├── FormantBankModule.js  # 4-formant cascade
│   │       ├── EnvelopeModule.js     # AR envelope
│   │       └── ReverbModule.js       # Reverb effect
│   ├── ui/
│   │   ├── AppView.js                # UI coordinator
│   │   └── JoystickControl.js        # 2D canvas controller
│   ├── main.js                       # Application entry
│   └── styles.css                    # UI styling
├── tests/
│   ├── formant.spec.js               # DSP module tests
│   └── joystick.spec.js              # Interaction tests
├── index.html
├── package.json
└── README.md
```

## Technical Details

### Formant Frequencies

Approximate formant frequencies for vowels (male voice):

| Vowel | F1 (Hz) | F2 (Hz) | F3 (Hz) | F4 (Hz) |
|-------|---------|---------|---------|---------|
| i     | 270     | 2290    | 3010    | 3500    |
| e     | 530     | 1840    | 2480    | 3500    |
| a     | 730     | 1090    | 2440    | 3200    |
| o     | 570     | 840     | 2410    | 3200    |
| u     | 300     | 870     | 2240    | 3200    |

### Filter Design

- **Type:** Biquad bandpass filters
- **Q Calculation:** Q = f₀ / BW
- **Cascade Gain:** 8x makeup gain to compensate attenuation
- **Bandwidths:** 80-200 Hz (wider for lower formants)

### IPA Vowel Quadrilateral

The joystick maps to the International Phonetic Alphabet vowel space:

```
    i           u     (High vowels - closed jaw)
      e       o       (Mid vowels)
         a            (Low vowels - open jaw)

Front ← → Back  (Tongue position)
```

- **X-axis (F2):** Tongue front/back (inverted: left=high, right=low)
- **Y-axis (F1):** Jaw opening (top=high, bottom=low)

## Testing

```bash
npm test
```

**Test Coverage:**
- FormantModule: frequency/bandwidth setting, stability, DC attenuation
- FormantBankModule: cascade processing, vowel presets, nasal mode
- LarynxModule: waveform generation, pitch control, vibrato, vocal fry
- AspiratorModule: noise generation, level control
- JoystickControl: click simulation, drag gestures
- Vocal Modes: nasal, sing, shout, fry, stress processing
- Integration: complete synthesis chain with multiple modes

All 41 tests passing ✓

## Development

Built following the patterns established in the Flues project:

- **ES Modules:** Modern JavaScript module system
- **Vite:** Fast dev server with HMR
- **Vitest:** Unit testing framework
- **AudioWorklet:** Low-latency audio processing
- **Shared Components:** Reusable MIDI and UI modules

## References

### Speech Synthesis Theory

- Fant, G. (1960). *Acoustic Theory of Speech Production*
- Stevens, K. N. (1998). *Acoustic Phonetics*
- [IPA Vowel Chart](https://www.internationalphoneticassociation.org/)

### Related Projects

- [Pink Trombone](https://dood.al/pinktrombone/) - Interactive vocal tract simulator
- [Praat](https://www.fon.hum.uva.nl/praat/) - Speech analysis software
- Klatt synthesizer - Classic formant synthesis

## License

MIT

## Author

Danny Ayers
