# Changelog

All notable changes to the Flues Project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] - 2025-12-12

### Added

#### New Distortion Synthesis Algorithms
- 10 new algorithms (7-16) in Disyn/Floozy/Flues-Synth:
  - **Algorithm 7** (Hybrid Formant): ModFM + 3× fixed formants
  - **Algorithm 8** (Cascaded): DSF → Asymmetric FM → Tanh
  - **Algorithm 9** (Parallel Bank): 5× simultaneous algorithms
  - **Algorithm 10** (Feedback): ModFM with feedback loop
  - **Algorithm 11** (Morphing): Crossfade DSF ↔ ModFM ↔ PAF
  - **Algorithm 12** (Inharmonic): DSF + offset PAF
  - **Algorithm 13** (Adaptive Filter): DSF + ModFM mix
  - **Algorithm 14** (Multi-Stage): Tanh → exp → ring mod
  - **Algorithm 15** (Freq Asymmetry): Dual Asymmetric FM
  - **Algorithm 16** (Cross-Mod): 4-algorithm cross-modulation
- param3 support throughout DSP stack (OscillatorModule, disyn_wrapper, synth_engine)
- 10 new MIDI programs (18-27) showcasing new algorithms
- Asymmetric FM synthesis helper function

#### Flues-Synth
- Complete headless synthesizer for Raspberry Pi
- 29 MIDI programs with dynamic slider remapping
- ALSA MIDI/audio backends with auto-detect
- Monophonic voice management
- Dual DC blocking (source + feedback loop)
- Test suite: engine-smoke, envelope-test, disyn-levels, polyphony-smoke, noise-isolation, program6-test

#### LV2 Plugins
- **Disyn** - 7 primitive distortion algorithms + envelope + reverb
- **Floozy** - Hybrid Disyn + PM-Synth signal chain
- **Chatterbox** - Formant speech synthesizer (4 formants, 4 vocal modes)
- **ChatGen** - Text-to-speech MIDI generator (40+ phonemes)
- **Drumkit** - Industrial drum synthesizer (11 voices)
- **PM-Synth** - Physical modeling synth (12 interface types)
- **Flues-Control** - MIDI CC controller (29 programs, 9 sliders)

#### Documentation
- Complete algorithm reference (algorithms.md)
- MIDI CC mapping guide (midi.md)
- 29 program descriptions (PROGRAM_CHANGE.md)
- Program 6 race condition fix notes (PROGRAM6_FIX.md)
- Flues-Synth architecture overview
- Build and deployment guides

### Fixed
- **Race condition in interface_module.c** - Caching strategy prevents segfaults (commit 50bb5c0)
  - All 12 interface types now pre-created and cached
  - Type switching is instant (pointer reassignment, no malloc/free)
  - Thread-safe by design
- **Program 6 (Full Hybrid)** - Re-enabled after race fix, now stable with all modules active
- **CC routing for programs 18-27** - Added param3 mappings to midi_mapping.c
- **Algorithm output levels** - Calibrated all 17 algorithms for safe levels (peak < 0.95)
  - Algorithm 7: Reduced ModFM index 8.0→3.0, output 0.6→0.4
  - Algorithm 13: Reduced ModFM index 8.0→2.0, output 0.5→0.15
  - Algorithm 14: Reduced exp depth 5.0→1.5, output 0.4→0.25
  - Algorithm 16: Reduced modulation depths 2.0→0.5, 5.0→1.0, output 0.5→0.35
- Implicit declaration warning for disyn_set_param3 in dsp_modules.h

### Changed
- Increased default noise level (0.02 → 0.15) for formant excitation
- Reduced formant makeup gain (3.0× → 2.0×) to prevent Disyn clipping
- Boosted master gain (0.35 → 0.5) and global pad (0.5 → 0.7)
- Increased Disyn level (0.2 → 0.8) for better audibility
- CC 19 repurposed from "Disyn Level" to "param3" (users must use CC 7 for level control)

### Deprecated
- None

### Removed
- Aggressive output DC blocker (was killing envelope attack)
- Optimized to dual DC blocking (source + feedback loop only)

### Security
- No security issues addressed in this release

## [0.0.1] - 2025-11-01 (Internal)

### Added
- Initial prototype implementations
- Basic Disyn algorithms (0-6)
- PM-Synth core modules
- Chatterbox formant synthesis
- Experimental web applications (clarinet-synth, pm-synth)

---

## Release Links

- [0.1.0] - https://github.com/danja/flues/releases/tag/v0.1.0

## Comparison Links

- [Unreleased]: https://github.com/danja/flues/compare/v0.1.0...HEAD
- [0.1.0]: https://github.com/danja/flues/releases/tag/v0.1.0
