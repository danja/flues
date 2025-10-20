# flues

[Live synth experiments](https://danja.github.io/flues/)

First attempt was a physical modelling approach to clarinet. Second, a more general physical modelling setup. Latest addition explores fairly obscure distortion synthesis techniques largely based on Victor Lazzarini’s [Distortion Synthesis tutorial in Csound Journal Issue 11](https://csoundjournal.com/issue11/distortionSynthesis.html), with a browser-based Disyn instrument covering DSF, waveshaping, and modified FM algorithms.

I'm aiming to put some of these things onto hardware (Daisy Seed) but making browser-based, LV2 and standalone versions seems a useful exercise. Good fun!

## Projects

### Stove
A modular physical modeling synthesizer featuring **12 interface types** (8 physical models + 4 hypothetical) with strategy pattern architecture.

Available in three implementations:

#### 1. Web App (Browser)
* **[Try it live](https://danja.github.io/flues/pm-synth/)**
* [Project README](experiments/pm-synth/README.md)
* Web Audio API with AudioWorklet processing
* PWA support for offline use
* Documentation:
  * [Implementation Plan](experiments/pm-synth/docs/PLAN.md)
  * [Implementation Status](experiments/pm-synth/docs/IMPLEMENTATION_STATUS.md)
  * [Interface Refactoring Summary](docs/interface-refactoring-summary.md)
  * [Interface Algorithms Research](docs/interface-algorithms-research.md)
  * [Signal Flow Documentation](docs/interface-signal-flow.md)
  * [Adding New Interfaces Guide](docs/adding-new-interface-guide.md)

#### 2. GTK4 Desktop App (Linux Native)
* **[Project README](gtk-synth/README.md)**
* Native C implementation with GTK4 interface
* PulseAudio backend with threaded processing
* All 12 interface strategies fully implemented
* Complete DSP engine matching JavaScript exactly
* Build: `cd gtk-synth && meson setup builddir && ninja -C builddir`

#### 3. LV2 Plugin: Stove Synth
* Source & docs: [`lv2/pm-synth/`](lv2/pm-synth)
* Root-level helper: `./build_pm_synth.sh --clean --install-default`
  - Installs to `~/.lv2/pm-synth.lv2/`

**Features (all implementations):**
- 12 interface types: Pluck, Hit, Reed, Flute, Brass, Bow, Bell, Drum, Crystal, Vapor, Quantum, Plasma
- Modular DSP architecture (8 modules) with strategy pattern
- Real-time parameter control (18 parameters)
- High-fidelity physical modeling algorithms

### Clarinet Synth
Digital waveguide clarinet synthesizer - the original experiment that led to the PM Synth.

* **[Try it live](https://danja.github.io/flues/clarinet-synth/)**
* [Project README](experiments/clarinet-synth/README.md)

## Reference Materials
* [CLAUDE.md](CLAUDE.md) - Project guidelines and development practices
* [AGENTS Project Notes](AGENTS.md)
* Wikipedia: [Physical Modelling Synthesis](https://en.wikipedia.org/wiki/Physical_modelling_synthesis), [Digital waveguide synthesis](https://en.wikipedia.org/wiki/Digital_waveguide_synthesis), [Karplus-Strong Algorithm](https://en.wikipedia.org/wiki/Karplus%E2%80%93Strong_string_synthesis)
* [Physical Audio Signal Processing](http://ccrma.stanford.edu/~jos/pasp/) - Julius O. Smith III
