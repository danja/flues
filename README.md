# flues

Synth experiments. 

I really want to try some physical modelling synthesis with DIY Daisy Seed-based Eurorack module(s). But while I'm trying to sort out PCBs I'm having a play with the algorithms in HTML/JS. 

[Experiments](https://danja.github.io/flues/)

## Projects

### PM Synthesizer (Stove)
A modular physical modeling synthesizer featuring **12 interface types** (8 physical models + 4 hypothetical) with strategy pattern architecture.

* **[Try it live](https://danja.github.io/flues/pm-synth/)**
* [Project README](experiments/pm-synth/README.md)
* Documentation:
  * [Implementation Plan](experiments/pm-synth/docs/PLAN.md)
  * [Implementation Status](experiments/pm-synth/docs/IMPLEMENTATION_STATUS.md)
  * [Interface Refactoring Summary](docs/interface-refactoring-summary.md)
  * [Interface Algorithms Research](docs/interface-algorithms-research.md)
  * [Signal Flow Documentation](docs/interface-signal-flow.md)
  * [Adding New Interfaces Guide](docs/adding-new-interface-guide.md)

**Features:**
- 12 interface types: Pluck, Hit, Reed, Flute, Brass, Bow, Bell, Drum, Crystal, Vapor, Quantum, Plasma
- Modular DSP architecture with strategy pattern
- Real-time parameter control
- Web Audio API with AudioWorklet processing
- PWA support for offline use

### Clarinet Synth
Digital waveguide clarinet synthesizer - the original experiment that led to the PM Synth.

* **[Try it live](https://danja.github.io/flues/clarinet-synth/)**
* [Project README](experiments/clarinet-synth/README.md)

## Reference Materials
* [CLAUDE.md](CLAUDE.md) - Project guidelines and development practices
* [AGENTS Project Notes](AGENTS.md)
* Wikipedia: [Physical Modelling Synthesis](https://en.wikipedia.org/wiki/Physical_modelling_synthesis), [Digital waveguide synthesis](https://en.wikipedia.org/wiki/Digital_waveguide_synthesis), [Karplus-Strong Algorithm](https://en.wikipedia.org/wiki/Karplus%E2%80%93Strong_string_synthesis)
* [Physical Audio Signal Processing](http://ccrma.stanford.edu/~jos/pasp/) - Julius O. Smith III


