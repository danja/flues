# Chatterbox Implementation Plan

## Overview

Chatterbox is a speech-like sound generator PWA based on human vocal tract simulation. It follows the architectural patterns established in `experiments/disyn` but implements speech synthesis rather than distortion synthesis.

## Architecture Pattern

Following the disyn model:
- **ES modules** with Vite bundling
- **AudioWorklet** for real-time DSP processing
- **Modular UI components** from shared library
- **MIDI support** via MidiInputManager
- **Progressive Web App** with service worker

## Project Structure

```
experiments/chatterbox/
├── index.html                 # Entry point
├── package.json              # Dependencies and scripts
├── vite.config.js            # Vite configuration with @shared alias
├── vitest.config.js          # Test configuration
├── src/
│   ├── main.js               # Bootstrap application
│   ├── styles.css            # Application styles
│   ├── audio/
│   │   ├── ChatterboxEngine.js       # Main engine coordinator
│   │   ├── chatterbox-worklet.js     # AudioWorklet processor
│   │   └── modules/
│   │       ├── LarynxModule.js       # Modified sawtooth oscillator
│   │       ├── AspiratorModule.js    # Noise generator
│   │       ├── FormantModule.js      # Single formant filter
│   │       ├── FormantBankModule.js  # F1-F4 filter cascade
│   │       ├── EnvelopeModule.js     # Attack/decay envelope
│   │       └── ReverbModule.js       # Optional reverb (reuse from disyn)
│   └── ui/
│       ├── AppView.js                # Main UI coordinator
│       ├── JoystickControl.js        # 2D canvas joystick controller
│       └── CheckboxGroup.js          # Voice mode checkboxes
└── tests/
    └── formant.spec.js               # Formant filter tests
```

## Core DSP Modules

### 1. LarynxModule.js

**Purpose:** Generate voiced excitation signal (modified sawtooth)

**Features:**
- Modified sawtooth wave generation (richer in odd harmonics)
- Pitch control (50-400 Hz typical vocal range)
- Phase accumulation with anti-aliasing considerations
- Waveform shaping for more vocal-like quality

**Interface:**
```javascript
class LarynxModule {
  constructor(sampleRate)
  setPitch(frequency)      // Hz
  setVoiced(enabled)       // boolean
  process()                // returns sample
  reset()
}
```

**Implementation Notes:**
- Use phase accumulation like OscillatorModule from disyn
- Apply mild waveshaping to create more vocal-like harmonics
- Consider simple anti-aliasing (PolyBLEP or similar)

### 2. AspiratorModule.js

**Purpose:** Generate unvoiced excitation (breath noise)

**Features:**
- White noise generation
- Amplitude control
- Smooth on/off switching

**Interface:**
```javascript
class AspiratorModule {
  constructor(sampleRate)
  setLevel(amount)         // 0-1 normalized
  setAspirated(enabled)    // boolean
  process()                // returns sample
}
```

**Implementation Notes:**
- Simple `Math.random() * 2 - 1` for noise
- Consider adding a one-pole lowpass for colored noise option

### 3. FormantModule.js

**Purpose:** Single resonant bandpass filter (formant)

**Features:**
- Center frequency control (200-4000 Hz)
- Bandwidth (Q) control
- State-variable or biquad implementation
- High resonance for vowel quality

**Interface:**
```javascript
class FormantModule {
  constructor(sampleRate)
  setFrequency(hz)         // Center frequency
  setBandwidth(hz)         // Bandwidth in Hz
  setQ(q)                  // Alternative: quality factor
  process(input)           // returns filtered sample
  reset()
}
```

**Implementation Notes:**
- Use biquad bandpass filter for stability
- High Q values (5-20) for sharp formant peaks
- Based on FilterModule from pm-synth but optimized for bandpass

### 4. FormantBankModule.js

**Purpose:** Cascade of four formant filters (F1, F2, F3, F4)

**Features:**
- Four FormantModule instances in series
- Independent frequency/bandwidth control for each
- Preset vowel configurations (optional)
- Gain compensation for cascade

**Interface:**
```javascript
class FormantBankModule {
  constructor(sampleRate)
  setFormant(index, frequency, bandwidth)  // index 0-3
  process(input)                           // returns filtered sample
  reset()
}
```

**Typical Formant Ranges:**
- F1: 200-1000 Hz (jaw opening, high/low vowels)
- F2: 500-3000 Hz (tongue front/back)
- F3: 1500-4000 Hz (lip rounding, consonants)
- F4: 2500-4500 Hz (voice quality, brightness)

### 5. EnvelopeModule.js

**Purpose:** Attack/decay amplitude envelope

**Reuse:** Copy from `experiments/disyn/src/audio/modules/EnvelopeModule.js`

**Interface:**
```javascript
class EnvelopeModule {
  constructor(sampleRate)
  configure({ attack, decay })
  gate(on)
  process()               // returns envelope value 0-1
}
```

### 6. ReverbModule.js (Optional)

**Reuse:** Copy from `experiments/disyn/src/audio/modules/ReverbModule.js`

## Audio Engine Architecture

### ChatterboxEngine.js

**Pattern:** Follows DisynEngine.js architecture

**Responsibilities:**
- AudioContext management
- AudioWorklet node creation
- Parameter state management
- Message passing to/from worklet
- MIDI note on/off handling

**State:**
```javascript
{
  // Source controls
  pitch: 0.5,              // 0-1 → 80-400 Hz
  voiced: true,
  aspirated: false,
  noiseLevel: 0.2,

  // Formant frequencies (Hz)
  f1: { freq: 700, bw: 100 },
  f2: { freq: 1200, bw: 120 },
  f3: { freq: 2500, bw: 150 },
  f4: { freq: 3500, bw: 200 },

  // Envelope
  attack: 0.01,
  decay: 0.15,

  // Voice modes (future)
  nasal: false,
  sing: false,
  shout: false,
  fry: false,

  // Master
  masterGain: 0.8
}
```

### chatterbox-worklet.js

**Pattern:** Follows disyn-worklet.js architecture

**Signal Flow:**
```
┌─────────────┐     ┌──────────────┐
│ LarynxModule├────►│              │
└─────────────┘     │              │
                    │  Excitation  │
┌─────────────┐     │     Mix      │
│ Aspirator   ├────►│              │
└─────────────┘     └──────┬───────┘
                           │
                           ▼
                    ┌──────────────┐
                    │  FormantBank │
                    │  F1→F2→F3→F4 │
                    └──────┬───────┘
                           │
                           ▼
                    ┌──────────────┐
                    │  Envelope    │
                    └──────┬───────┘
                           │
                           ▼
                    ┌──────────────┐
                    │  Master Gain │
                    └──────┬───────┘
                           │
                           ▼
                        Output
```

**Message Handling:**
- `init` - Initialize with sample rate and default parameters
- `noteOn` - Gate envelope, set pitch from MIDI note
- `noteOff` - Release envelope
- `setSource` - Update larynx/aspirator settings
- `setFormant` - Update F1-F4 frequencies/bandwidths
- `setEnvelope` - Update attack/decay
- `setMaster` - Update master gain

## UI Components

### AppView.js

**Pattern:** Follows disyn AppView.js architecture

**Layout Sections:**
1. **Header** - Title, status pill, power button
2. **Source Panel** - Pitch slider, voiced/aspirated checkboxes, noise level
3. **Joystick Panel** - Canvas-based 2D controller for F1/F2
4. **Formant Panel** - Sliders for F3/F4 frequency
5. **Envelope Panel** - Attack/decay sliders
6. **Voice Modes** - Checkboxes for nasal/sing/shout/fry (future features)
7. **Keyboard & MIDI** - MIDI status and device selection

**Key Methods:**
```javascript
class AppView {
  constructor(container)
  mount()
  render()
  setupSourceControls()
  setupJoystick()
  setupFormantControls()
  setupEnvelopeControls()
  setupMidiControls()
  handleNoteOn(payload)
  handleNoteOff(payload)
}
```

### JoystickControl.js (NEW COMPONENT)

**Purpose:** 2D canvas-based joystick for controlling F1 and F2 frequencies

**Features:**
- Click/touch to set position
- Drag to update continuously
- Visual feedback (crosshair, position indicator)
- Maps X → F2 frequency (500-3000 Hz)
- Maps Y → F1 frequency (200-1000 Hz, inverted)
- IPA vowel quadrilateral overlay (optional visual guide)

**Interface:**
```javascript
class JoystickControl {
  constructor({
    canvas,          // HTMLCanvasElement
    width,           // Canvas width
    height,          // Canvas height
    onInput,         // callback(x, y) with normalized 0-1 values
  })

  setPosition(x, y)  // Programmatically set position
  destroy()          // Cleanup listeners
}
```

**Implementation:**
- Canvas 2D rendering context
- Mouse and touch event handling
- Coordinate normalization
- Visual design: grid background, current position marker
- Optional: vowel labels at key positions (i, e, a, o, u)

**Visual Design:**
```
┌─────────────────────────┐
│  i                    u │  ← High vowels (F1 low)
│                         │
│     e               o   │  ← Mid vowels
│                         │
│         a               │  ← Low vowels (F1 high)
└─────────────────────────┘
  ↑ Front              Back ↑
  (F2 high)        (F2 low)
```

### Reusable Components

**From shared library:**
- `KnobControl` - Rotary knobs (alternative to sliders)
- `KeyboardInput` - Optional on-screen keyboard
- `MidiInputManager` - MIDI device handling

## UI Design

### Color Scheme

Follow disyn's clean, modern aesthetic:
- Dark background (#1a1a1a)
- Light text (#e0e0e0)
- Accent colors for active states
- Subtle borders and shadows

### Layout

```
┌─────────────────────────────────────────────────┐
│  Chatterbox Speech Synthesizer      [Status] [●]│
├─────────────────────────────────────────────────┤
│                                                 │
│  SOURCE                                         │
│  ┌─────────┐  □ Voiced    □ Aspirated          │
│  │ Pitch   │  Noise: ────────○──────           │
│  └─────────┘                                    │
├─────────────────────────────────────────────────┤
│  VOWEL SPACE (F1 & F2)                          │
│  ┌──────────────────────────────────────────┐  │
│  │    i                              u      │  │
│  │                                          │  │
│  │        e                      o          │  │
│  │                                          │  │
│  │                 a                        │  │
│  └──────────────────────────────────────────┘  │
├─────────────────────────────────────────────────┤
│  FORMANTS                                       │
│  F3 Freq: ────────○──────  F4 Freq: ────────○──│
├─────────────────────────────────────────────────┤
│  ENVELOPE                                       │
│  Attack: ──○──  Decay: ──────○─────            │
├─────────────────────────────────────────────────┤
│  VOICE MODES (future features)                  │
│  □ Nasal  □ Sing  □ Shout  □ Fry              │
├─────────────────────────────────────────────────┤
│  MIDI: [Device Select ▼] [●] Activity          │
└─────────────────────────────────────────────────┘
```

## Parameter Mapping

### Pitch
- **UI Range:** 0-1 (slider/knob)
- **Mapped Range:** 80-400 Hz (exponential)
- **MIDI:** Note number overrides slider when playing

### F1 Frequency (Joystick Y)
- **UI Range:** 0-1 (vertical, inverted)
- **Mapped Range:** 200-1000 Hz (exponential)
- **High Y (top):** Low frequency (high vowels like 'i')
- **Low Y (bottom):** High frequency (low vowels like 'a')

### F2 Frequency (Joystick X)
- **UI Range:** 0-1 (horizontal)
- **Mapped Range:** 500-3000 Hz (exponential)
- **Low X (left):** High frequency (front vowels like 'i')
- **High X (right):** Low frequency (back vowels like 'u')

### F3 Frequency (Slider)
- **UI Range:** 0-1
- **Mapped Range:** 1500-4000 Hz (exponential)

### F4 Frequency (Slider)
- **UI Range:** 0-1
- **Mapped Range:** 2500-4500 Hz (exponential)

### Noise Level
- **UI Range:** 0-1
- **Mapped Range:** 0-1 (linear)

### Attack/Decay
- **UI Range:** 0-1
- **Mapped Range:** 0.001-1.0s (exponential)

## MIDI Implementation

**Following disyn pattern:**

### MIDI Note Events
- **Note On:**
  - Trigger envelope gate
  - Set pitch from MIDI note (overrides slider)
  - Use velocity for amplitude scaling
- **Note Off:**
  - Release envelope gate

### MIDI Control Change (Future)
- **CC 1 (Mod Wheel):** F1 frequency
- **CC 2 (Breath):** Aspirated level
- **CC 7 (Volume):** Master gain
- **CC 71 (Resonance):** F2 frequency
- **CC 74 (Brightness):** F3 frequency

## Development Phases

### Phase 1: Core Audio Engine (Foundation)
1. Create project structure
2. Copy package.json, vite.config.js from disyn
3. Implement LarynxModule (sawtooth oscillator)
4. Implement AspiratorModule (noise generator)
5. Implement FormantModule (single bandpass filter)
6. Implement FormantBankModule (4-filter cascade)
7. Copy EnvelopeModule from disyn
8. Write basic unit tests

### Phase 2: AudioWorklet Integration
1. Implement ChatterboxEngine.js
2. Implement chatterbox-worklet.js
3. Wire up signal flow: sources → formants → envelope → output
4. Test audio output with default parameters

### Phase 3: Basic UI
1. Implement AppView.js skeleton
2. Add power button and status pill
3. Add basic sliders for pitch, F1-F4
4. Test parameter changes trigger audio updates

### Phase 4: Joystick Control
1. Implement JoystickControl.js
2. Add canvas rendering
3. Add mouse/touch interaction
4. Map X/Y to F2/F1 frequencies
5. Add visual feedback (IPA vowel quadrilateral overlay)

### Phase 5: Complete UI
1. Add source controls (voiced/aspirated checkboxes)
2. Add noise level slider
3. Add envelope controls
4. Add voice mode checkboxes (UI only, no DSP yet)
5. Styling and polish

### Phase 6: MIDI Support
1. Integrate MidiInputManager from shared
2. Add MIDI device selection UI
3. Wire up note on/off to engine
4. Test with MIDI keyboard

### Phase 7: PWA Features
1. Add service worker
2. Add manifest.json
3. Add install prompt
4. Test offline functionality
5. Test on mobile devices

### Phase 8: Advanced Features (Future)
1. **Nasal coupling:** Add nasal formant branch (parallel to oral)
2. **Singing mode:** Vibrato on pitch (LFO)
3. **Shout mode:** Increase formant amplitudes, add noise
4. **Fry mode:** Pulse register (subharmonics)
5. **Presets:** Store/recall vowel/consonant configurations
6. **Real-time spectrogram:** Visualize formant structure

## Code Reuse Strategy

### Copy Directly (No Modifications)
- `experiments/disyn/package.json` → Update name to "chatterbox"
- `experiments/disyn/vite.config.js` → Copy as-is
- `experiments/disyn/vitest.config.js` → Copy as-is
- `experiments/disyn/src/audio/modules/EnvelopeModule.js` → Copy as-is
- `experiments/disyn/src/audio/modules/ReverbModule.js` → Optional, copy as-is

### Use as Template (Modify Structure)
- `experiments/disyn/src/main.js` → Rename to ChatterboxEngine
- `experiments/disyn/src/audio/DisynEngine.js` → Template for ChatterboxEngine.js
- `experiments/disyn/src/audio/disyn-worklet.js` → Template for chatterbox-worklet.js
- `experiments/disyn/src/ui/AppView.js` → Template for AppView.js
- `experiments/disyn/index.html` → Update title to "Chatterbox"

### Reuse from Shared Library
- `@shared/ui/KnobControl.js` - For rotary controls
- `@shared/ui/KeyboardInput.js` - Optional on-screen keyboard
- `@shared/midi/MidiInputManager.js` - MIDI device handling

### Implement from Scratch
- `src/audio/modules/LarynxModule.js` - New oscillator
- `src/audio/modules/AspiratorModule.js` - New noise generator
- `src/audio/modules/FormantModule.js` - New filter (inspired by FilterModule)
- `src/audio/modules/FormantBankModule.js` - New filter cascade
- `src/ui/JoystickControl.js` - New 2D controller

## Testing Strategy

### Unit Tests
- **FormantModule:** Frequency response, stability
- **LarynxModule:** Waveform shape, pitch accuracy
- **AspiratorModule:** Output characteristics
- **FormantBankModule:** Cascade behavior, gain compensation

### Integration Tests
- **ChatterboxEngine:** Message passing, state management
- **chatterbox-worklet:** Sample generation, envelope behavior

### Manual Testing
- Browser compatibility (Chrome, Firefox, Safari, iOS Safari)
- MIDI device connectivity
- Touch interaction on mobile
- PWA installation
- Offline functionality

## Performance Considerations

### DSP Optimization
- Minimize allocations in audio process loop
- Pre-calculate filter coefficients when parameters change
- Use efficient phase accumulation (avoid trig in inner loop where possible)
- Consider using PolyBLEP for anti-aliasing only if needed

### UI Optimization
- Throttle joystick updates (60fps max)
- Debounce parameter changes
- Use requestAnimationFrame for canvas rendering
- Minimize DOM manipulation

## Accessibility

- Keyboard navigation for all controls
- ARIA labels on interactive elements
- Screen reader support for status messages
- High contrast mode support
- Focus indicators on controls

## Browser Compatibility

### Target Browsers
- Chrome 66+ (AudioWorklet support)
- Firefox 76+ (AudioWorklet support)
- Safari 14.1+ (AudioWorklet support)
- iOS Safari 14.5+ (AudioWorklet support)

### Fallback Strategy
- Display error message for unsupported browsers
- No ScriptProcessor fallback (complexity not justified)

## Future Enhancements

### Advanced DSP
1. **Consonant synthesis:** Burst generators, friction noise
2. **Glottal pulse shaping:** More realistic voice source
3. **Dynamic formant transitions:** Vowel-to-vowel morphing
4. **Formant bandwidth modulation:** Breathy vs modal voice
5. **Pitch jitter/shimmer:** Natural voice irregularity

### UI Improvements
1. **Real-time spectrogram:** FFT-based frequency visualization
2. **Formant trajectory recording:** Record and play back gestures
3. **IPA chart overlay:** Click vowel symbols to jump to position
4. **Preset system:** Save/load favorite sounds
5. **Tutorial mode:** Guided introduction to speech synthesis

### Performance Features
1. **Polyphony:** Multiple simultaneous notes
2. **Harmonizer:** Pitch-shifted copies for chords
3. **Chorus/flanger effects:** Thicken the sound

## References

### Speech Synthesis Theory
- Fant, G. (1960). *Acoustic Theory of Speech Production*
- Stevens, K. N. (1998). *Acoustic Phonetics*
- IPA Vowel Chart: https://www.internationalphoneticassociation.org/

### DSP Techniques
- Digital filter design (biquad, state-variable)
- Anti-aliasing methods (PolyBLEP, BLEP, MinBLEP)
- Formant synthesis (Klatt synthesizer, source-filter model)

### Existing Implementations
- Pink Trombone: https://dood.al/pinktrombone/
- Praat: https://www.fon.hum.uva.nl/praat/
- Klatt synthesizer implementations

## Success Criteria

### MVP (Phase 1-6)
- ✅ Real-time synthesis with < 20ms latency
- ✅ Four independently controllable formants
- ✅ Joystick control for F1/F2 produces recognizable vowel sounds
- ✅ MIDI note input works reliably
- ✅ Responsive UI on desktop and mobile
- ✅ No audio glitches or dropouts during parameter changes

### Full Release (Phase 7)
- ✅ PWA installable on mobile devices
- ✅ Works offline after first load
- ✅ Accessible to screen reader users
- ✅ Comprehensive documentation

### Excellence (Phase 8)
- ✅ Advanced voice modes (nasal, sing, shout, fry) implemented
- ✅ Real-time visual feedback (spectrogram)
- ✅ Preset system with shareable URLs
- ✅ Educational content about speech synthesis

## Conclusion

Chatterbox builds on the solid foundation of the disyn experiment while introducing speech synthesis concepts and new interaction paradigms (2D joystick control). By following established patterns and reusing proven components, the implementation can proceed systematically with clear milestones and testable outcomes.

## LV2 Plugin Implementation (2025)

Following successful completion of the web application, Chatterbox was ported to an LV2 plugin for use in DAWs. The plugin implementation:
- Preserves all DSP modules with line-by-line C++ translation
- Includes native X11/Cairo UI with IPA vowel quadrilateral joystick
- Provides comprehensive MIDI control (note on/off, velocity, 15 CC mappings)
- Maintains all vocal modes and effects from the web version
- See `lv2/chatterbox/README.md` for detailed plugin documentation
