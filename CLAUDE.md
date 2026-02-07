# Flues Project Notes

## A. General Synth & Web Practices

### Repository Layout

```
flues/
├── .github/
│   └── workflows/
│       └── deploy.yml   # GitHub Actions deployment workflow
├── experiments/         # JavaScript experimentation projects
│   ├── clarinet-synth/  # Digital waveguide clarinet synthesizer
│   │   ├── src/
│   │   │   ├── audio/          # Audio engine modules
│   │   │   ├── ui/             # UI controller modules
│   │   │   └── main.js         # Application entry point
│   │   ├── index.html          # HTML entry point
│   │   └── package.json        # Dependencies and scripts
│   └── pm-synth/       # General-purpose physical modelling synth
│       ├── docs/                # Requirements, plan, status
│       ├── src/
│       │   ├── audio/           # Modular DSP engine + worklet
│       │   ├── ui/              # Knobs, switches, keyboard, visualizer
│       │   └── main.js          # Application coordinator
│       ├── index.html           # UI layout
│       └── package.json         # Dependencies and scripts
├── html/                # Original prototypes and experiments
├── reference/           # Reference materials
├── www/                 # Built static site for GitHub Pages
│   ├── index.html       # Landing page
│   ├── clarinet-synth/  # Built clarinet synth app
│   └── pm-synth/        # Built Stove app
└── package.json         # Root package with build scripts

```

### JavaScript Development Checklist

Projects use **ES modules** (not CommonJS) and are:
- Packaged with **Vite**
- Tested with **Vitest**
- Written in vanilla JavaScript for browser execution

### Bundling & Compatibility

- **AudioWorklet bundling:** use `new URL('./worklet.js', import.meta.url)` so Vite emits a single chunk; Safari/iOS requires bundled worklet code.
- **Service workers:** register after `load`, reference paths relative to the deployed folder (`sw.js`, `manifest.webmanifest`, `icon.svg`).
- **PWA prompts:** listen for `beforeinstallprompt`, stash the event, and surface an install button.
- **iOS audio unlock:** call the unlock helper immediately during the first user gesture (power button) and provide ScriptProcessor fallback if the worklet fails to load.
- **Testing:** run `npx vitest run --pool vmThreads --maxWorkers=1` before shipping.
- **Deployment:** pushes to `main` trigger GitHub Pages build to `https://danja.github.io/flues/<app>/`; always smoke-test on mobile Safari/Chrome.

## B. Physical Modelling Apps

### Clarinet Synthesizer

### Clarinet Synthesizer

**Location:** `experiments/clarinet-synth/`

A digital waveguide physical modeling synthesizer that simulates clarinet acoustics using:
- Karplus-Strong style delay line
- Nonlinear reed model
- Breath pressure and noise simulation
- Real-time parameter control

**Architecture:**
- `src/audio/ClarinetEngine.js` - Core DSP synthesis engine
- `src/audio/ClarinetProcessor.js` - Web Audio API interface
- `src/ui/KnobController.js` - Rotary knob controls
- `src/ui/KeyboardController.js` - Musical keyboard interface
- `src/ui/Visualizer.js` - Waveform visualization
- `src/main.js` - Application coordination

**Running:**
```bash
cd experiments/clarinet-synth
npm install
npm run dev
```

**Controls:**
- Reed & Excitation: Breath, Reed Stiffness, Noise, Attack
- Bore & Resonance: Damping, Brightness, Vibrato, Release
- Keyboard: Mouse/touch on visual keys or computer keyboard (AWSEDFTGYHUHJK)

### Chatterbox Synthesizer

**Location:** `experiments/chatterbox/`

A speech synthesis application using formant filtering and source-filter vocal tract modeling. Runs entirely in the browser with interactive IPA vowel quadrilateral control.

**Architecture:**
- `src/audio/modules/` - Six DSP modules:
  - `LarynxModule.js` - Modified sawtooth oscillator with vibrato and vocal fry
  - `AspiratorModule.js` - White noise generator for unvoiced sounds
  - `FormantModule.js` - Single resonant bandpass filter (biquad)
  - `FormantBankModule.js` - Cascade of four formants (F1-F4) + optional nasal formant
  - `EnvelopeModule.js` - Attack/release envelope
  - `ReverbModule.js` - Schroeder reverb
- `src/audio/ChatterboxEngine.js` - Main coordinator with parameter mapping
- `src/audio/chatterbox-worklet.js` - AudioWorklet processor with vocal mode processing
- `src/ui/JoystickControl.js` - 2D canvas controller for F1/F2 (unique to Chatterbox)
- `src/ui/AppView.js` - UI coordinator
- `docs/CHATTERBOX-PLAN.md` - Complete implementation plan
- `tests/` - Unit tests for DSP modules, vocal modes, and joystick interaction

**Running:**
```bash
cd experiments/chatterbox
npm install
npm run dev
```

**Controls:**
- Vowel Canvas: Click and drag to speak, move to morph vowels (i, e, a, o, u)
- Spacebar: Hold to trigger sound (hands-free)
- Pitch Slider: Adjust fundamental frequency (80-400 Hz)
- Source: Voiced/Aspirated checkboxes, Noise level
- Formants: F3 and F4 frequency sliders
- Envelope: Attack and release controls
- Vocal Modes: Nasal, Sing (vibrato), Shout, Fry (vocal fry)
- Stress: Amplitude control with soft clipping (Soft → Very Loud)

**Key Features:**
- Interactive IPA vowel quadrilateral with visual markers
- Real-time formant manipulation (F1-F4) + optional nasal formant (250 Hz)
- Dual excitation sources (larynx + aspirator)
- Advanced vocal modes: vibrato (5.5 Hz), vocal fry (subharmonics), shout (15% formant boost)
- Stress processing: 0.5-2.0x gain with tanh soft clipping
- Q-based bandpass filters with cascade gain compensation
- 41 passing tests (DSP + vocal modes + interaction)

### PM Synthesizer

**Location:** `experiments/pm-synth/`

A modular physical modelling synthesizer that expands the clarinet experiment into a general-purpose instrument designer supporting plucked, struck, reed, flute, and brass behaviours. Runs entirely in the browser with AudioWorklet processing and ScriptProcessor fallback.

**Architecture:**
- `src/audio/modules/` - Eight focused DSP modules:
  - `SourcesModule.js` - DC, white-noise, and sawtooth excitation sources
  - `EnvelopeModule.js` - Gate-driven attack/release amplifier
  - `InterfaceModule.js` - Strategy pattern context class managing 12 interface types
  - `interface/` - Modular interface system:
    - `InterfaceStrategy.js` - Base class and InterfaceType enum
    - `InterfaceFactory.js` - Factory for creating strategy instances
    - `strategies/` - 12 concrete implementations (8 physical + 4 hypothetical)
    - `utils/` - Shared DSP utilities (nonlinearity, excitation, delay, energy tracking)
  - `DelayLinesModule.js` - Dual delay lines with pitch tuning and ratio control
  - `FeedbackModule.js` - Independent delay and post-filter feedback returns
  - `FilterModule.js` - State-variable filter morphing through LP/BP/HP
  - `ModulationModule.js` - LFO providing bipolar AM↔FM modulation
  - `ReverbModule.js` - Schroeder reverb with room size and wet/dry mix
- `src/audio/PMSynthEngine.js` - Coordinates modules, applies note lifecycle
- `src/audio/pm-synth-worklet.js` - Bundled AudioWorklet processor with ScriptProcessor fallback
- `src/ui/` - Knob and rotary switch controllers, keyboard, waveform visualizer
- `src/main.js` - UI wiring, install prompt handling, power management
- `docs/` - Requirements, implementation plan, research, and refactoring documentation

**Running:**
```bash
cd experiments/pm-synth
npm install
npm run dev
```

**Controls:**
- Sources: DC, Noise, Tone levels
- Envelope: Attack, Release
- Interface: 12-position type switch (Pluck, Hit, Reed, Flute, Brass, Bow, Bell, Drum, Crystal, Vapor, Quantum, Plasma), Intensity
- Delay Lines: Tuning, Ratio
- Feedback: Delay 1, Delay 2, Filter returns
- Filter: Frequency, Q, Shape morph
- Modulation: LFO Frequency, AM↔FM bipolar depth
- Reverb: Size, Level
- Keyboard: On-screen keys or `AWSEDFTGYHUHJK` computer keys (monophonic)

## C. GTK Desktop Application

### PM Synth GTK

**Location:** `gtk-synth/`

A standalone GTK4 desktop application that ports the `experiments/pm-synth` JavaScript synthesizer to native C code. Provides the same physical modeling synthesis engine with a native Linux UI.

**Key Features:**
- Native C implementation of all DSP modules
- GTK4 interface with sliders and controls
- PulseAudio audio backend
- Real-time synthesis with low latency
- Keyboard input for playing notes

**Architecture:**
- `include/` - Public API headers (pm_synth.h, dsp_modules.h, interface_strategy.h, dsp_utils.h, audio_backend.h)
- `src/audio/` - DSP engine implementation
  - `pm_synth_engine.c` - Main synthesizer coordinator
  - `audio_backend_pulse.c` - PulseAudio threaded backend
  - `modules/` - All 8 DSP modules (Sources, Envelope, Interface, DelayLines, Feedback, Filter, Modulation, Reverb)
  - `modules/strategies/` - Interface strategies (Reed fully implemented, 11 stubs)
- `src/ui/synth_window.c` - GTK4 main window and controls
- `reference/` - Copied JavaScript code for reference during development
- `docs/PORTING_NOTES.md` - Detailed translation patterns and mapping

**Building:**
```bash
cd gtk-synth

# Install dependencies (Ubuntu/Debian)
sudo apt install build-essential meson ninja-build libgtk-4-dev libpulse-dev

# Build and run
meson setup builddir
ninja -C builddir
./builddir/pm-synth-gtk
```

**Usage:**
1. Click "Click Here First" to enable audio
2. Select interface type from dropdown (Reed, Pluck, etc.)
3. Play notes with keyboard keys A-K (C4-C5)
4. Adjust parameters with sliders

**Implementation Status:**
- ✓ Core engine and signal flow (matches JavaScript exactly)
- ✓ All 8 DSP modules fully implemented
- ✓ Reed interface strategy (full implementation)
- ✓ Basic GTK4 UI with Sources/Envelope/Feedback controls
- ⚠ Remaining 11 interface strategies (using simple stubs)
- ⚠ Complete UI (missing Filter, Modulation, Reverb controls)
- TODO: Waveform visualizer, MIDI input, preset system

**Translation Fidelity:**
The C code is a direct, line-by-line translation of the JavaScript algorithms. All DSP math is preserved exactly:
- Same signal flow with DC blocker on feedback path only
- Identical filter coefficients and state equations
- Same Strategy pattern for interface types
- Matching default parameter values from constants.js

See `gtk-synth/docs/PORTING_NOTES.md` for detailed JavaScript→C translation patterns.

## D. LV2 Plugins

### Disyn LV2 Plugin

**Location:** `lv2/disyn/`

A standalone LV2 plugin that ports the `experiments/disyn` JavaScript distortion synthesizer to native C++. Provides seven distortion synthesis algorithms in a monophonic instrument plugin.

**Key Features:**
- Native C++ implementation of distortion algorithms
- Seven synthesis modes: Dirichlet Pulse, DSF Single/Double, Tanh Square/Saw, PAF, Modified FM
- Real-time synthesis with low latency
- LV2 standard interface for DAW integration
- Attack/Release envelope and Schroeder reverb

**Architecture:**
- `src/modules/` - DSP modules (OscillatorModule.hpp, EnvelopeModule.hpp, ReverbModule.hpp)
- `src/DisynEngine.hpp` - Main synthesizer coordinator
- `src/disyn_plugin.cpp` - LV2 interface and MIDI handling
- `disyn.lv2/*.ttl` - LV2 metadata and port definitions

**Building:**
```bash
cd lv2/disyn
cmake -S . -B build
cmake --build build
cmake --install build --prefix ~/.lv2
```

**Usage:**
Load in any LV2 host (Ardour, Carla, Jalv):
```bash
jalv.gtk https://danja.github.io/flues/plugins/disyn
```

**Implementation Status:**
- ✓ All 7 algorithms fully implemented
- ✓ Monophonic voice management
- ✓ Sample-accurate MIDI processing
- ✓ Attack/Release envelope
- ✓ Schroeder reverb with size/level controls
- ✓ Algorithm selector and per-algorithm parameters
- ⚠ Monophonic only (no polyphony yet)
- TODO: GTK UI, preset system, polyphony

**Translation Fidelity:**
The C++ code is a direct, line-by-line translation of the JavaScript algorithms. All DSP math is preserved exactly:
- Same synthesis algorithms with identical formulas
- Identical parameter mapping (exponential/linear curves)
- Same signal flow: oscillator → envelope → reverb
- Matching default parameter values

See `lv2/disyn/README.md` for detailed usage and `experiments/disyn/docs/DISYN-LV2.md` for implementation notes.

**2025-02 UI update:** Added a self-contained X11/Cairo control panel (`src/ui/disyn_ui_x11.c`) so hosts that ignore GTK embed paths still show a fully rendered GUI. Layout mirrors the algorithm workflow (Algorithm/Params, Envelope, Reverb, Master) and includes a discrete selector with labelled scale points for the seven modes. CMake now builds `disyn_ui.so` linking against `x11`, `cairo`, and `pthread`; manifests/TTLs advertise the new X11 UI (`disyn.ttl`, `manifest.ttl`). Reinstall the bundle after building to pick up the UI binary.

### Floozy LV2 Plugin

**Location:** `lv2/floozy/`

Hybrid LV2 instrument that merges Disyn’s distortion algorithms with the PM-Synth physical modelling signal chain. A Disyn-driven source block feeds the PM-style interface/pipe/filter/modulation/reverb pipeline, yielding aggressive spectral content that still sits within the resonant acoustic feedback loop.

- DSP: `src/floozy_plugin.cpp`, `src/FloozyEngine.hpp`, `src/modules/FloozySourceModule.hpp` (wraps Disyn oscillators + PM noise/DC sources).
- UI: raw X11/Cairo panel (`src/ui/floozy_ui_x11.c`) laid out in five rows (Source, Interface/Envelope, Delay, Filter, Modulation/Reverb + master).
- Metadata: `floozy.lv2/manifest.ttl`, `floozy.lv2/floozy.ttl`.

**Build:** from repo root `cmake -S lv2/floozy -B lv2/floozy/build && cmake --build lv2/floozy/build`. Install with `cmake --install lv2/floozy/build --prefix ~/.lv2`.

### Chatterbox LV2 Plugin

**Location:** `lv2/chatterbox/`

A speech synthesis LV2 instrument plugin based on formant filtering and source-filter vocal tract modeling. Ported from the `experiments/chatterbox` JavaScript/AudioWorklet web application.

**Key Features:**
- Native C++ implementation of formant synthesis
- Source-filter model: Larynx + Aspirator → F1-F4 formant cascade
- Four formants controlling jaw (F1), tongue (F2), lips (F3), and quality (F4)
- Vocal modes: Nasal (250Hz resonance), Sing (5.5Hz vibrato), Shout (15% boost), Fry (f₀/2 subharmonics)
- Dynamic processing: Attack/release envelope, stress control with soft clipping
- Built-in Schroeder reverb
- X11/Cairo UI with 20 controls organized in 6 groups
- Monophonic with MIDI note and velocity support

**Architecture:**
- `src/modules/` - Five DSP modules:
  - `LarynxModule.hpp` - Modified sawtooth with vibrato and vocal fry
  - `AspiratorModule.hpp` - White noise generator (LCG)
  - `FormantModule.hpp` - Biquad bandpass filter (Q-based)
  - `FormantBankModule.hpp` - Cascade of F1-F4 + optional nasal
  - `EnvelopeModule.hpp` - Linear AR envelope with exponential time mapping
- `src/ChatterboxEngine.hpp` - Main DSP coordinator
- `src/chatterbox_plugin.cpp` - LV2 wrapper with sample-accurate MIDI
- `src/ui/chatterbox_ui_x11.c` - X11/Cairo UI (20 knobs + toggles)
- `chatterbox.lv2/*.ttl` - LV2 metadata and 22 port definitions

**Signal Flow:**
```
MIDI → Larynx (sawtooth + vibrato + fry) + Aspirator (noise)
       ↓
       Formant Cascade (F1→F2→F3→F4) + Nasal (parallel)
       ↓
       Envelope (AR) → Stress (gain + soft clip) → Reverb → Master
```

**Building:**
```bash
cd lv2/chatterbox
cmake -S . -B build
cmake --build build
cmake --install build --prefix ~/.lv2
```

**Usage:**
```bash
# Verify installation
lv2ls | grep chatterbox

# Load in host
jalv.gtk https://danja.github.io/flues/plugins/chatterbox

# Or load in Ardour, Carla, Reaper, etc.
```

**Parameters (20 controls):**
- **Source:** Pitch (80-400Hz), Voiced, Aspirated, Noise Level
- **Formants:** F1 (200-1000Hz), F2 (500-3000Hz), F3 (1500-4000Hz), F4 (2500-4500Hz)
- **Vocal Modes:** Nasal, Sing, Shout, Fry (all toggles)
- **Dynamics:** Stress, Attack (1-1000ms), Release (10-3000ms)
- **Effects:** Reverb Size, Reverb Level
- **Output:** Master Gain

**Implementation Status:**
- ✓ All 5 DSP modules fully implemented
- ✓ Monophonic voice management
- ✓ Sample-accurate MIDI processing (note on/off, velocity, 17 CC mappings including CC 103/104)
- ✓ All 4 vocal modes working (nasal, sing, shout, fry)
- ✓ Stress processing with tanh soft clipping
- ✓ X11/Cairo UI with 6 logical groups
- ✓ Rotary knobs and LED toggle indicators
- ✓ IPA vowel quadrilateral joystick for F1/F2 control
- ⚠ Monophonic only (no polyphony)
- TODO: Preset system

**Translation Fidelity:**
The C++ code is a direct, line-by-line translation of the JavaScript AudioWorklet implementation:
- Same signal flow: excitation → formant cascade → envelope → stress → reverb
- Identical DSP algorithms (cubic waveshaping 0.15, vibrato 5.5Hz ±1.5%, fry at f₀/2)
- Same parameter mappings (exponential pitch/formants, exponential envelope times)
- Matching default values (formant makeup gain 3.0×, reduced from 8.0× to fix distortion)
- Preserved vocal mode processing (shout 15% boost, stress tanh clipping above 0.6)

**UI Design:**
The X11/Cairo UI follows the Disyn/Floozy pattern with pthread event handling and real-time rendering:
- **Row 0:** Source (Pitch, Voiced, Aspirated, Noise) + Vowel Space Joystick (F1/F2 interactive canvas)
- **Row 1:** Formants F3/F4 + Vocal Modes (Nasal, Sing, Shout, Fry toggles) + Dynamics (Stress, Attack, Release)
- **Row 2:** Reverb (Size, Level) + Output (Master)
- Rotary knobs for continuous parameters (vertical drag or scroll)
- LED indicators for toggles (green when active, dark when off)
- IPA vowel quadrilateral joystick with visual markers (i, e, a, o, u)
- Group backgrounds with titles for visual organization

**MIDI Control:**
Comprehensive MIDI CC mapping covering all major parameters:
- **Standard CCs:** 1 (Stress), 7 (Master Gain), 10 (F2/Tongue)
- **Sound Controllers:** 71 (F1/Jaw), 72 (Release), 73 (Attack), 74 (F3/Lips), 75 (F4/Quality)
- **Vocal Mode Toggles:** 80 (Nasal), 81 (Sing), 82 (Shout), 83 (Fry) - on when ≥64
- **Effects:** 84 (Reverb Size), 85 (Reverb Level), 91 (Reverb Level alt)
- **Source Control:** 102 (Noise Level), 103 (Aspirated toggle), 104 (Voiced toggle)

Note: CC 103 and CC 104 enable MIDI control of the Aspirated and Voiced toggles, critical for text-to-speech via ChatGen.

See `lv2/chatterbox/README.md` for detailed usage, sound design tips, and vowel presets.

### ChatGen LV2 Plugin

**Location:** `lv2/chatgen/`

A text-to-speech MIDI generator that converts English text into MIDI events (Note On/Off + 7 CCs) for controlling Chatterbox's formant speech synthesizer. ChatGen parses text into phonemes and generates sample-accurate MIDI synchronized to a musical beat grid.

**Key Features:**
- Native C++ implementation with X11/Cairo UI
- Full phoneme parsing: 40+ phonemes (vowels, fricatives, plosives, nasals, liquids, affricates)
- MIDI Note generation: Note On (C4) when Play starts, Note Off when stops
- 7 MIDI CCs: Formants (F1-F4), Noise Level, Aspirated toggle, Voiced toggle
- Text input field with real-time phoneme preview
- Text persistence across UI focus changes and DAW restarts
- Half-note timing: Each phoneme lasts 2 beats (1 second at 120 BPM)
- DAW tempo sync via LV2 Time extension
- Designed for serial routing: `[ChatGen] → [Chatterbox] → Audio Out` on same track

**Architecture:**
- `src/modules/` - Four DSP modules:
  - `TextParser.hpp` - Text → phoneme sequence (40+ phonemes with digraph detection)
  - `PhonemeMapper.hpp` - Phoneme → MIDI CC values (formants + noise + voiced flags)
  - `ClockSync.hpp` - BPM-based half-note timing engine
  - `MidiGenerator.hpp` - Note On/Off + CC sequence writer
- `src/ChatGenEngine.hpp` - Module coordinator
- `src/chatgen_plugin.cpp` - LV2 wrapper with atom communication
- `src/ui/chatgen_ui_x11.c` - X11/Cairo UI with text input and phoneme preview
- `chatgen.lv2/*.ttl` - LV2 metadata and 5 port definitions

**Signal Flow:**
```
Text Input (UI) → TextParser → Phoneme Sequence
                       ↓
                PhonemeMapper → Formant CCs + Voiced/Noise flags
                       ↓
                  ClockSync → Half-note beat detection (every 2 beats)
                       ↓
               MidiGenerator → Note On/Off + 7 MIDI CCs → Output
```

**Building:**
```bash
cd lv2/chatgen
cmake -S . -B build
cmake --build build
cmake --install build --prefix ~/.lv2
```

**Usage:**
```bash
# Verify installation
lv2ls | grep chatgen

# Load in host (standalone testing)
jalv.gtk https://danja.github.io/flues/plugins/chatgen

# Typical setup in DAW: Same track with [ChatGen] → [Chatterbox] → Audio Out
```

**Phoneme Types (40+ total):**
- **Vowels (11)**: /i/, /ɪ/, /e/, /æ/, /ɑ/, /ɔ/, /o/, /u/, /ʊ/, /ʌ/, /ɜ/ - all voiced, no noise
- **Plosives (6)**: /p/, /b/, /t/, /d/, /k/, /g/ - burst noise (70), voiced vs unvoiced
- **Fricatives (9)**: /f/, /v/, /s/, /z/, /θ/, /ð/, /ʃ/, /ʒ/, /h/ - high noise (90-120), voiced vs unvoiced
- **Affricates (2)**: /tʃ/, /dʒ/ - medium noise (95), voiced vs unvoiced
- **Nasals (3)**: /m/, /n/, /ŋ/ - voiced with slight noise (10)
- **Liquids (4)**: /l/, /r/, /w/, /j/ - voiced with minimal noise (0-5)

**MIDI Output (Channel 1):**
- **Note On**: MIDI note 60 (C4), velocity 96 - sent when Play starts
- **Note Off**: MIDI note 60 (C4) - sent when Play stops
- **CC 71**: F1 (Jaw) - 0-127 → 200-1000 Hz exponential
- **CC 10**: F2 (Tongue) - 0-127 → 500-3000 Hz exponential
- **CC 74**: F3 (Lips) - 0-127 → 1500-4000 Hz exponential
- **CC 75**: F4 (Quality) - 0-127 → 2500-4500 Hz exponential
- **CC 102**: Noise Level - 0-127 linear (fricatives 90-120, plosives 70, vowels 0)
- **CC 103**: Aspirated - 0 (off) or 127 (on) - enables noise generator when noise > 0
- **CC 104**: Voiced - 0 (off) or 127 (on) - enables larynx for vowels and voiced consonants

**Text Parsing Examples:**
```
Input: "cat dog fish"
Phonemes: [k] [ae] [t] [d] [o] [g] [f] [ih] [sh]
Voiced: OFF ON OFF ON ON ON OFF ON OFF
Noise: 70 0 70 70 0 70 110 0 115
Duration: 9 phonemes × 2 beats = 18 beats = 9 seconds at 120 BPM

Input: "hello world"
Phonemes: [h] [e] [l] [o] [w] [er] [l] [d]
Voiced: OFF ON ON ON ON ON ON ON
Noise: 90 0 5 0 0 5 5 70
```

**Digraph Support:**
- "sh" → /ʃ/, "ch" → /tʃ/, "th" → /θ/, "ng" → /ŋ/
- "ee" → /i/, "oo" → /u/, "er" → /ɜ/, "aw" → /ɔ/
- Checked before single-character mapping

**Implementation Status:**
- ✓ All 4 DSP modules fully implemented
- ✓ 40+ phoneme support with voiced/unvoiced distinction
- ✓ Note On/Off generation with C4 fixed pitch
- ✓ 7 MIDI CC output (formants + noise + source control)
- ✓ X11/Cairo UI with text input and phoneme preview
- ✓ Text persistence via atom communication (UI ↔ DSP)
- ✓ Half-note timing (2 beats per phoneme)
- ✓ DAW tempo sync via LV2 Time extension
- ✓ Transport play/stop sync
- TODO: Variable phoneme duration, pitch control, polyphonic mode

**UI Design:**
The X11/Cairo UI provides text input and visual feedback:
- **Text Input Field**: Type English text, press Enter to send to DSP
- **Phoneme Preview**: Shows parsed phonemes in real-time (e.g., `[k] [ae] [t]`)
- **Play Button**: Green when active, starts phoneme playback
- **Loop Button**: Blue when active, repeats phoneme sequence
- Simple layout focused on text entry and playback control

**Integration with Chatterbox:**
ChatGen is specifically designed to control Chatterbox via MIDI CCs. The key innovation is **Voiced/Unvoiced control**:
- **Voiced phonemes** (vowels, /m/, /n/, /l/, /r/, etc.): CC 104 = 127, CC 103 = 0
  - Chatterbox larynx ON, aspirator OFF → pure tonal sound
- **Unvoiced fricatives** (/f/, /s/, /sh/, /th/, /h/): CC 104 = 0, CC 103 = 127
  - Chatterbox larynx OFF, aspirator ON → pure breathy noise
- **Voiced fricatives** (/v/, /z/, /zh/, /dh/): CC 104 = 127, CC 103 = 127
  - Both sources enabled → mixed tonal + noisy sound

This enables proper speech articulation with distinct vowels and consonants.

**Text Persistence Pattern:**
Text survives UI focus changes via bidirectional atom communication:
- **UI → DSP**: Bare atom:String sent with atomEventTransferUrid (host wraps in sequence)
- **DSP → UI**: Text broadcast on every audio callback in sequence
- **UI protection**: `user_is_editing` flag prevents overwrites while typing
- **Port subscription**: UI subscribes to TEXT_OUT port for updates

This ensures typed text persists even when the UI window loses focus or is closed and reopened.

See `lv2/chatgen/README.md` for detailed usage, phoneme tables, sound design tips, and troubleshooting.

### Drumkit LV2 Plugin

**Location:** `lv2/drumkit/`

A hardcore industrial drum synthesizer LV2 plugin with 11 voices inspired by the TR-909 but pushed into aggressive territory. Designed for industrial, EBM, and harsh electronic music.

**Key Features:**
- Native C++ implementation with 8 synthesized drum voices
- X11/Cairo UI with 18 rotary knobs organized by drum type (4 rows)
- MIDI omni mode (responds to all MIDI channels)
- General MIDI note mapping (C2-D3, notes 36-50)
- Velocity sensitivity on kick, snare, and toms
- Hi-hat choke group (closed hi-hat kills open hi-hat)
- Master FX chain: Bit crusher, distortion (tanh), Schroeder reverb

**Architecture:**
- `src/modules/` - Seven DSP modules:
  - `NoiseGenerator.hpp` - LCG white noise generator
  - `ADEnvelope.hpp` - Attack/decay envelope for one-shot drums
  - `PitchEnvelope.hpp` - Exponential pitch sweep for kick/toms
  - `BiquadFilter.hpp` - Bandpass/highpass filtering
  - `Distortion.hpp` - Tanh soft clipping
  - `Bitcrusher.hpp` - Bit depth reduction (16→4 bit)
  - `DCBlocker.hpp` - DC offset removal (R=0.999)
- `src/voices/` - Six drum voice classes (8 instances):
  - `KickVoice.hpp` - Pitch env → sine → distortion → punch (4 params)
  - `SnareVoice.hpp` - Dual resonators (180/330 Hz) + noise (2 params)
  - `ClapVoice.hpp` - Multi-impulse noise bursts + resonant BP filter (2 params)
  - `TomVoice.hpp` - Pitch env → resonant bandpass (2 params, shared Lo/Hi)
  - `HiHatVoice.hpp` - 6× inharmonic oscillators + ring mod (2 params, shared)
  - `CrashVoice.hpp` - Noise → 3× bandpass cascade (2 params)
- `src/DrumKitEngine.hpp` - Main coordinator (11 voices + master FX)
- `src/drumkit_plugin.cpp` - LV2 wrapper with sample-accurate MIDI
- `src/ui/drumkit_ui_x11.c` - X11/Cairo UI (18 knobs, 4 rows)
- `drumkit.lv2/*.ttl` - LV2 metadata and 20 port definitions

**Signal Flow:**
```
MIDI Note → Voice Synthesis → Voice Mixer
    ↓
Bit Crusher (global)
    ↓
Master Distortion (tanh)
    ↓
Reverb (Schroeder, shared)
    ↓
DC Blocker (R=0.999)
    ↓
Master Gain
    ↓
Audio Out
```

**Building:**
```bash
cd lv2/drumkit
cmake -S . -B build
cmake --build build
cmake --install build --prefix ~/.lv2
```

**Usage:**
```bash
# Verify installation
lv2ls | grep drumkit
lv2info https://danja.github.io/flues/plugins/drumkit

# Load in DAW (Reaper, Ardour, Carla, etc.)
```

**MIDI Note Mapping:**
| MIDI Note | Drum Voice | Velocity Sensitive |
|-----------|------------|-------------------|
| 36 (C2) | Kick | Yes |
| 38 (D2) | Snare | Yes |
| 39 (Eb2) | Clap | No (fixed level) |
| 42 (F#2) | Closed Hi-Hat | No (chokes open HH) |
| 45 (A2) | Lo Tom | Yes |
| 46 (A#2) | Open Hi-Hat | No (fixed level) |
| 49 (C#3) | Crash | No (fixed level) |
| 50 (D3) | Hi Tom | Yes |

**Parameters (18 total):**
- **Kick (4)**: Pitch (60-250Hz), Decay (50-1500ms), Drive (1-10×), Punch (0-100%)
- **Snare (2)**: Tone (body/shell mix + Q 4-20), Snap (noise + HPF 500Hz-4kHz)
- **Clap (2)**: Density (3-7 impulses, 25-10ms spacing, 3-7ms bursts), Tone (1000-4500Hz, Q 4-8)
- **Toms (2, shared)**: Pitch (Lo 60-150Hz, Hi 150-400Hz), Decay (80-800ms)
- **Hi-Hats (2, shared)**: Brightness (HPF 4-12kHz), Decay (Closed 50-200ms, Open 200-1200ms)
- **Crash (2)**: Brightness (BP 1.5-10kHz), Decay (300-2500ms)
- **Master (4)**: Bit Crush (0-4 bit), Drive (1-5× tanh), Reverb (0-60%), Gain (0-100%)

**Implementation Status:**
- ✓ All 8 drum voices fully implemented
- ✓ Sample-accurate MIDI processing (omni mode)
- ✓ Velocity sensitivity on kick/snare/toms
- ✓ Hi-hat choke group working
- ✓ Master FX chain complete
- ✓ X11/Cairo UI with 18 knobs
- ✓ Industrial sound design: aggressive transients, metallic resonance, harsh harmonics

**Sound Design Notes:**
The clap voice was significantly improved from initial implementation:
- Changed from single-sample impulses to 3-7ms noise bursts (much more punch)
- Higher Q resonance (4-8, was fixed at 3) for more "snap"
- Brighter frequency range (1000-4500Hz, was 800-3500Hz)
- Faster attack (1ms, was 10ms) for immediate response
- Louder bursts (1.2× amplitude) and higher output level (0.8×, was 0.5×)
- Dynamic Q: higher tone settings increase Q for cutting bright claps

**Translation Fidelity:**
The C++ code follows established patterns from other LV2 plugins:
- Same DSP module architecture as Disyn/Chatterbox
- Identical X11/Cairo UI pattern with pthread event handling
- Sample-accurate MIDI processing with LV2 Atom sequences
- Proper voice management with reset and trigger methods

See `lv2/drumkit/README.md` for detailed usage, sound design tips, and synthesis methods.

### P-Mix LV2 Plugin

**Location:** `lv2/p-mix/`

A probabilistic mixer LV2 effect that toggles audio on/off at bar boundaries using host transport/tempo. Designed for rhythmic dropouts and stochastic mutes across up to eight channels.

**Key Features:**
- 8-channel in/out gain switching
- Weighted Maintain/Fade/Cut behavior at decision points
- Bias control to target overall play ratio
- X11/Cairo UI with rotary controls

**Controls:**
- Granularity (bars)
- Maintain, Fade, Cut (weights)
- Fade Dur Max (max fade fraction)
- Bias (target play percentage)

See `docs/p-mix--manual.md` for user-facing details.

### MIDI Flip LV2 Plugin

**Location:** `lv2/midi-flip/`

A MIDI utility plugin that reflects note events around a pivot note. Useful for inverted melodies and mirrored phrases.

**Key Features:**
- MIDI in/out atom ports
- Pivot note control (0–127, default C4)
- Note On/Off flipping, other events pass through
- X11/Cairo UI slider

See `lv2/midi-flip/README.md` for usage and build steps.

### PM Synth LV2 UI Refactor (2025-02)

**What changed:** The LV2 GUI for `pm-synth` previously relied on GTK widgets embedded through the host’s X11 parent. Hosts such as Reaper were not driving the GTK draw loop, so the window showed stale pixels or never painted. The UI was rebuilt as a host-agnostic, raw X11 + Cairo surface.

**Key steps:**
- Removed `FluesKnob` GTK custom widget and the accompaning `pm_synth_ui.c`.
- Added `pm_synth_ui_x11.c` which creates its own X11 child window, renders all groups and rotary controls with Cairo, and drives interaction directly.
- Introduced an internal event/render thread using `XInitThreads`, `XPending`, and `pthread` to handle events and redraws independently of the host idle interface.
- Layout logic now centres each control group, adapts the window size from the measured content, and mirrors the original three-row design (Steam/Interface/Envelope; Pipe/Filter; Modulation/Reverb).
- Updated `CMakeLists.txt` to link against `x11`, `cairo`, and `pthread` instead of GTK, ensuring the UI builds wherever those minimal dependencies exist.

**Result:** The UI paints immediately in Reaper/Ardour, no stale buffers, knob interaction works, and the window dimensions match the control grid. Reinstall `pm_synth_ui.so` after building to pick up the change.

## E. Flues Synth (Headless Raspberry Pi)

### Flues Synth

**Location:** `flues-synth/`

A standalone headless C/C++ ALSA MIDI synthesizer for Raspberry Pi that combines Disyn, Chatterbox, and PM-Synth architectures. Designed for deployment on Raspberry Pi 4 with MIDI controller.

**Key Features:**
- Native C/C++ implementation with ALSA audio/MIDI
- Hybrid synthesis: Disyn (7 algorithms) → Formants (F1-F4) → Interface (12 types) → Delays → Filter → Modulation
- 29 MIDI programs (0-28) with signal chain reconfiguration and context-dependent slider remapping
- Dual DC blocking (R=0.999) prevents feedback latching
- Calibrated signal levels (0.28 peak output, safe margin)
- 29 MIDI CCs + 6 control notes (36-41) for debug toggles
- 48kHz sample rate, 512 sample buffer (~10ms latency)
- Test suite: engine-smoke, envelope-test, disyn-levels, polyphony-smoke, noise-isolation
- Real-time CC name printing on first occurrence

**Architecture:**
- `include/` - Public API headers (synth_engine.h, dsp_modules.h, config.h, audio/midi backends)
- `src/main.c` - Entry point, MIDI CC mapping, control note handling
- `src/synth_engine.c` - Voice management, signal flow coordinator
- `src/audio/audio_backend_alsa.c` - ALSA PCM output with device auto-detection
- `src/audio/midi_backend_alsa.c` - ALSA sequencer input with auto-connect
- `src/audio/modules/` - Nine DSP modules (Disyn, Sources, Envelope, Formants, Interface, Delays, Feedback, Filter, Modulation)
- `src/audio/modules/strategies/` - 12 interface implementations (Reed, Pluck, Hit, Flute, Brass, Bow, Bell, Drum, Crystal, Vapor, Quantum, Plasma)
- `tests/` - Five test suites (engine-smoke, envelope-test, disyn-levels, polyphony-smoke, noise-isolation)
- `docs/` - Documentation including SVG signal flow diagram, algorithms.md (complete DSP reference), PROGRAM_CHANGE.md (29 programs)

**Building:**
```bash
cd flues-synth
meson setup builddir
meson compile -C builddir
meson test -C builddir
```

**Running:**
```bash
# Auto-detect audio device
./builddir/flues-synth

# Specify device
./builddir/flues-synth hw:2,0

# Enable MIDI debug
FLUES_MIDI_DEBUG=1 ./builddir/flues-synth hw:2,0
```

**Signal Flow:**
```
MIDI Input → Disyn (×0.8) → [DC Block 1] → Sources Mix
           ↓
         Envelope (AR) → Formants (×2.0 makeup)
           ↓
         + Feedback ← [DC Block 2] ← Feedback Mix ← Delay1/2 + Filter
           ↓
         Interface (nonlinear) → Dual Delays → SVF Filter
           ↓
         × AM Mod → Global (×0.7 pad → tanh → ×0.5 master) → Output
```

**Control Notes (36-41):**
- **Note 36**: Toggle Noise source
- **Note 37**: Toggle Disyn oscillator
- **Note 38**: Toggle Feedback loop
- **Note 39**: Toggle Formant cascade
- **Note 40**: Toggle SVF filter
- **Note 41**: Hard mute (clears delays, DC blockers, filters)

**MIDI CC Mapping (29 parameters):**
- **Source**: CC 1 (Intensity), 16-19 (Disyn alg/params/level), 20-21 (Noise/DC level)
- **Formants**: CC 71 (F1/Jaw), 10 (F2/Tongue), 74 (F3/Lips), 75 (F4/Quality)
- **Vocal Modes**: CC 80-83 (Nasal, Sing, Shout, Fry - toggle ≥64)
- **Envelope**: CC 73 (Attack), 72 (Release)
- **Interface**: CC 24 (Type), 26 (Tuning), 27 (Delay Ratio)
- **Feedback**: CC 28-30 (Delay1/Delay2/Filter returns)
- **Filter**: CC 32-34 (Frequency, Q, Shape LP→BP→HP)
- **Modulation**: CC 36-37 (LFO Freq, AM↔FM Depth)
- **Output**: CC 7 (Master Gain)

**DC Blocking Strategy:**
- **DC Blocker 1** (after Disyn): R=0.999, catches DC at source, -60dB @ DC, -3dB @ 7.6 Hz
- **DC Blocker 2** (feedback loop): R=0.999, prevents accumulation in delay/filter loop
- **Why dual**: Original triple-stage blocking (with output blocker) killed envelope attack
- **Removed**: Output DC blocker was too aggressive, treated slow attack ramp as DC offset

**Level Management:**
- **Disyn level**: 0.8 (user adjustable via CC 19, was 0.2 before fix)
- **Formant makeup**: 2.0× (compensates for Q-induced attenuation, was 3.0×)
- **Global pad**: 0.7× (was 0.5×)
- **Master gain**: 0.5× (was 0.35×)
- **Final peak**: ~0.28 (safe, no clipping)
- **Soft clipping**: tanh guard at output prevents runaway noise

**Testing:**
```bash
# Run all tests
meson test -C builddir

# Individual tests
./builddir/engine-smoke    # Verifies non-zero RMS output
./builddir/envelope-test   # Tests attack/sustain/release
./builddir/disyn-levels    # Measures all 7 algorithms
```

**Implementation Status:**
- ✅ Single-voice operation with full signal chain
- ✅ All 9 DSP modules fully implemented
- ✅ All 12 interface strategies (Reed fully implemented, 11 stubs functional)
- ✅ ALSA audio/MIDI backends with auto-connect
- ✅ 29 MIDI CC controls with name printing
- ✅ 6 control notes for debug toggles
- ✅ Dual DC blocking protection
- ✅ Test suite (all 3 tests passing)
- ☐ 4-voice polyphony (Phase 2)
- ☐ GTK4 UI (Phase 3, optional)

**Recent Changes (2025-12-05):**
- Removed aggressive output DC blocker (was killing envelope attack)
- Optimized to dual DC blocking (source + feedback loop)
- Increased default noise level (0.02 → 0.15) for formant excitation
- Reduced formant makeup gain (3.0× → 2.0×) to prevent Disyn clipping
- Boosted master gain (0.35 → 0.5) and global pad (0.5 → 0.7)
- Increased Disyn level (0.2 → 0.8) for better audibility
- Added CC name printing on first occurrence
- All tests now passing (engine-smoke, envelope-test, disyn-levels)

**Translation Fidelity:**
The C code is a direct translation of the JavaScript/LV2 implementations:
- Same signal flow and DSP algorithms
- Identical parameter mappings (exponential/linear curves)
- Matching default parameter values
- Preserved DC blocking strategy (after optimization)
- Same formant cascade with Q-based biquad filters

See `flues-synth/README.md` for detailed usage, `flues-synth/docs/flues-synth-signal-flow.svg` for visual architecture, and `flues-synth/docs/*.md` for implementation notes.

## F. C Code (Legacy/Future)

**Location:** `c-code/`

Reserved for future C implementations and experiments that don't fit the GTK desktop app or LV2 plugin patterns.

## Original Prototypes

**Location:** `html/`

Contains original standalone HTML files used during initial development:
- `clarinet-synth.html` - Original monolithic version
- `clarinet-engine.js` - Extracted engine code
- `main-app.js` - Extracted app logic
- `ui-controller.js` - Extracted UI controllers
- Various test and iteration files

These files were refactored into the modular `experiments/clarinet-synth/` project.

### Future Experiments

Additional JavaScript experiments should follow the same pattern:
1. Create directory under `experiments/`
2. Set up with Vite + Vitest
3. Use ES modules
4. Include package.json with `dev`, `build`, and `test` scripts
5. Document in this file

### GitHub Pages Deployment

**Location:** `www/`

The `www/` directory contains the built static site deployed to GitHub Pages.

**Local Development (Testing Built Site):**
```bash
npm run dev
```

This command:
1. Builds all experiments (runs `npm run ghp`)
2. Starts a local Vite dev server serving the `www/` directory
3. Opens browser at `http://localhost:5173/`

This lets you test the built site locally exactly as it will appear on GitHub Pages.

**Building and Publishing:**
```bash
npm run ghp
```

This command:
1. Builds all experiments (`clarinet-synth`, `pm-synth`)
2. Copies built files to `www/` directory mirroring deployment structure
3. Ready for GitHub Pages deployment

**Automatic Deployment:**
The `.github/workflows/deploy.yml` workflow automatically:
- Triggers on push to `main` branch
- Builds all experiments
- Deploys to GitHub Pages

**Manual Deployment:**
Can be triggered via GitHub Actions "workflow_dispatch" event.

### Notes

- All Node.js style development uses ES modules (`type: "module"` in package.json).
- Vite provides fast HMR for development.
- Web Audio API requires user interaction before audio can play; iOS needs explicit unlock.
- Physical modelling synthesis is CPU-intensive but realistic.
- The `www/` directory is git-tracked and contains built artifacts for GitHub Pages.
- be sure and remove old code when it's no longer needed
