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
├── kxmx_bluemchen/      # Eurorack module related code
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

## C Development

**Location:** `c-code/`

To be organized as C implementations are developed.

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