# Flues

**Live synthesis experiments exploring physical modeling, speech synthesis, distortion algorithms, and hybrid techniques**

[![Live Demos](https://img.shields.io/badge/Try-Live%20Demos-blue?style=for-the-badge)](https://danja.github.io/flues/)

---

## What is this?

Flues is a collection of experimental synthesizers exploring different approaches to sound generation. What started as a simple clarinet simulator has evolved into a family of instruments spanning physical modeling, vocal synthesis, distortion techniques, and hybrid architectures.

Each concept begins as a browser-based experiment (using Web Audio API), then evolves through multiple implementations: native desktop apps (GTK4/C), DAW plugins (LV2), and embedded hardware (Raspberry Pi). The goal is to understand synthesis techniques deeply by implementing them across different platforms.

---

## 🎹 Try It Now - Live Web Experiments

No installation required - these run entirely in your browser:

| Experiment | Synthesis Type | What It Does |
|------------|---------------|--------------|
| **[Stove](https://danja.github.io/flues/pm-synth/)** | Physical Modeling | 12 interface types (reed, pluck, bow, brass, etc.) with waveguide synthesis |
| **[Chatterbox](https://danja.github.io/flues/chatterbox/)** | Speech Synthesis | Formant-based vocal synthesis with IPA vowel quadrilateral control |
| **[Clarinet](https://danja.github.io/flues/clarinet-synth/)** | Physical Modeling | Digital waveguide clarinet (the original experiment) |

All web experiments support MIDI input, keyboard control, and touch/mouse interaction.

---

## The Experimental Journey

### 1. **Physical Modeling** → Stove

**Concept**: Simulate acoustic instruments using digital waveguides, delay lines, and nonlinear interfaces.

Started with a clarinet synthesizer (Karplus-Strong algorithm), then generalized into **Stove** - a modular physical modeling engine with 12 different interface types representing physical (reed, bow, brass) and hypothetical (crystal, vapor, quantum, plasma) excitation methods.

**Signal Flow**: `Sources → Envelope → Interface → Delay Lines → Filter → Modulation → Reverb`

**Available as:**
- 🌐 **[Web app](https://danja.github.io/flues/pm-synth/)** - Try in browser ([docs](experiments/pm-synth/README.md))
- 🖥️ **Desktop app** - GTK4/C native ([gtk-synth/](gtk-synth/))
- 🔌 **LV2 plugin** - For DAWs ([lv2/pm-synth/](lv2/pm-synth/))

### 2. **Speech Synthesis** → Chatterbox + ChatGen

**Concept**: Source-filter vocal tract modeling using formant cascade filters.

**Chatterbox** implements a larynx (voiced source), aspirator (noise), and four formant filters (F1-F4) representing jaw, tongue, lips, and voice quality. Includes vocal modes: nasal resonance, vibrato (sing), shout (boost), and vocal fry (subharmonics).

**ChatGen** is a companion LV2 plugin that converts typed English text into MIDI events controlling Chatterbox, enabling text-to-speech synthesis in a DAW.

**Available as:**
- 🌐 **[Chatterbox web app](https://danja.github.io/flues/chatterbox/)** - Interactive vowel quadrilateral ([docs](experiments/chatterbox/README.md))
- 🔌 **Chatterbox LV2** - Full vocal synthesis plugin ([lv2/chatterbox/](lv2/chatterbox/))
- 🔌 **ChatGen LV2** - Text-to-speech MIDI generator ([lv2/chatgen/](lv2/chatgen/))
  - Usage: `[ChatGen] → [Chatterbox]` on same track

### 3. **Distortion Synthesis** → Disyn

**Concept**: Generate complex spectra using mathematical distortion functions.

Based on Victor Lazzarini's [Distortion Synthesis tutorial](https://csoundjournal.com/issue11/distortionSynthesis.html), implementing 17 algorithms including: Dirichlet Pulse, DSF Single/Double, Tanh Square/Saw, PAF (Phase-Aligned Formant), Modified FM, and 10 extended algorithms with param3 control for enhanced spectral manipulation.

**Available as:**
- 🔌 **LV2 plugin** - Monophonic distortion synth ([lv2/disyn/](lv2/disyn/))

### 4. **Hybrid Synthesis** → Floozy + Flues-Synth

**Concept**: Combine multiple synthesis approaches in a single signal chain.

**Floozy** grafts Disyn's aggressive distortion algorithms onto Stove's physical modeling engine. Disyn provides the excitation source, feeding into the waveguide/filter/modulation chain for acoustic resonance.

![Floozy screenshot](docs/images/floozy-plugin.png)

**Flues-Synth** is the ultimate hybrid: a headless ALSA synthesizer for Raspberry Pi combining Disyn, formants, and physical modeling with MIDI program changes dynamically reconfiguring the signal chain.

**Signal Flow**: `Disyn → Formants → Interface → Delays → Filter → Modulation`

**Available as:**
- 🔌 **Floozy (mono)** - Single voice hybrid ([lv2/floozy/](lv2/floozy/))
- 🔌 **Floozy Poly** - 8-voice polyphonic ([lv2/floozy-poly/](lv2/floozy-poly/))
- 🎛️ **Flues-Synth** - Headless Raspberry Pi ([flues-synth/](flues-synth/))
  - [Signal flow diagram](flues-synth/docs/flues-synth-signal-flow.svg)
  - [Algorithm reference](flues-synth/docs/algorithms.md) - Complete DSP documentation
  - 29 MIDI programs (0-28) switching signal chain configurations
  - 29 MIDI CCs + 6 control notes for live tweaking

### 5. **Drum Synthesis** → Drumkit

**Concept**: Synthesized drum voices inspired by TR-909 but pushed into hardcore industrial territory.

8 synthesized voices (kick, snare, clap, toms, hi-hats, crash) using pitch envelopes, resonant filters, noise bursts, and inharmonic oscillators. Master FX chain includes bit crushing, distortion, and reverb for aggressive, metallic tones.

**Available as:**
- 🔌 **LV2 plugin** - 8 voices, 18 parameters, General MIDI mapping ([lv2/drumkit/](lv2/drumkit/))

---

## Implementation Patterns

Each synthesis concept follows a progression:

```
🌐 Web Experiment          → Learn the DSP, iterate quickly
   ↓
🖥️ Native Desktop App      → Optimize performance, direct hardware access
   ↓
🔌 LV2 Plugin             → Integrate with DAWs (Ardour, Reaper, etc.)
   ↓
🎛️ Embedded Hardware       → Deploy on Raspberry Pi / Daisy Seed
```

**Technologies used:**
- **Web**: JavaScript ES modules, Web Audio API, AudioWorklet, Vite, Vitest
- **Desktop**: C/GTK4, PulseAudio, Meson
- **Plugins**: C++, LV2, X11/Cairo UI, CMake
- **Embedded**: C/C++, ALSA MIDI/Audio, Meson

---

## Getting Started

### Try the Live Demos (Easiest)
Visit https://danja.github.io/flues/ and start playing immediately. All web experiments support MIDI controllers and computer keyboard input.

### Download Pre-Built Releases

Pre-built binaries are available for Ubuntu desktop (x86_64) and Raspberry Pi (aarch64):

**[Download Latest Release](https://github.com/danja/flues/releases/latest)**

Three distribution options:
- **Full Release** (`flues-v*.tar.gz`) - All LV2 plugins + Flues-Synth
- **Plugins Only** (`flues-plugins-v*.tar.gz`) - LV2 plugins for desktop DAWs
- **Synth Only** (`flues-synth-v*.tar.gz`) - Headless synthesizer for Raspberry Pi

**Quick install:**
```bash
# Extract release
tar xzf flues-v0.1.0-multi-arch.tar.gz
cd flues-v0.1.0

# Install (requires sudo)
sudo ./install.sh

# Verify plugins
lv2ls | grep flues

# Run flues-synth (Raspberry Pi)
flues-synth
```

### Build From Source

**Prerequisites**: `cmake`, `build-essential`, `meson`, `ninja-build`, `lv2-dev`, `libx11-dev`, `libcairo2-dev`, `libasound2-dev`

**Quick start** (builds all plugins):
```bash
git clone https://github.com/danja/flues.git
cd flues
./build_synths.sh --install-default
```

Plugins install to `~/.lv2/` and appear in any LV2 host (Ardour, Reaper, Carla, etc.)

**Multi-platform release builds**: See [scripts/MULTI_PLATFORM_BUILD.md](scripts/MULTI_PLATFORM_BUILD.md) for building optimized binaries across desktop and Raspberry Pi platforms.

**Individual builds**: See subdirectory READMEs for specific instructions
- [lv2/pm-synth/README.md](lv2/pm-synth/README.md) - Stove plugin
- [lv2/chatterbox/README.md](lv2/chatterbox/README.md) - Speech synthesis
- [lv2/chatgen/README.md](lv2/chatgen/README.md) - Text-to-speech
- [lv2/disyn/README.md](lv2/disyn/README.md) - Distortion synthesis
- [lv2/floozy/README.md](lv2/floozy/README.md) - Hybrid mono
- [lv2/floozy-poly/README.md](lv2/floozy-poly/README.md) - Hybrid poly
- [lv2/drumkit/README.md](lv2/drumkit/README.md) - Drum synthesizer

### Build Native Apps

**GTK Desktop (Stove):**
```bash
cd gtk-synth
meson setup builddir
ninja -C builddir
./builddir/pm-synth-gtk
```

**Raspberry Pi (Flues-Synth):**
```bash
cd flues-synth
meson setup builddir
meson compile -C builddir
./builddir/flues-synth    # Auto-detects MIDI/audio
```

---

## Project Structure

```
flues/
├── experiments/           # Web-based experiments (JavaScript/Web Audio)
│   ├── pm-synth/         # Stove physical modeling
│   ├── chatterbox/       # Speech synthesis
│   └── clarinet-synth/   # Original clarinet experiment
├── lv2/                  # LV2 plugins for DAWs
│   ├── pm-synth/         # Stove plugin
│   ├── chatterbox/       # Speech synthesis plugin
│   ├── chatgen/          # Text-to-speech MIDI generator
│   ├── disyn/            # Distortion synthesis plugin
│   ├── floozy/           # Hybrid mono plugin
│   ├── floozy-poly/      # Hybrid polyphonic plugin
│   └── drumkit/          # Drum synthesizer plugin
├── gtk-synth/            # GTK4 native desktop app (Stove)
├── flues-synth/          # Headless Raspberry Pi synthesizer
├── www/                  # Built static site (GitHub Pages)
└── docs/                 # Cross-project documentation
```

---

## Philosophy

These are **experiments**, not polished products. The goal is to:
- Understand DSP algorithms by implementing them multiple ways
- Explore unconventional synthesis techniques (distortion, hybrid chains)
- Learn platform differences (web timing vs. native RT audio vs. embedded)
- Prioritize sound quality and expressiveness over preset count
- Keep code readable and educational (see [CLAUDE.md](CLAUDE.md))

Expect rough edges, experimental features, and ongoing evolution. That's part of the fun.

---

## Recent Updates

### v0.1.0 Release (December 2025)

**First Official Release** with multi-platform builds optimized for Ubuntu desktop (x86_64) and Raspberry Pi (aarch64).

**Download**: [GitHub Releases](https://github.com/danja/flues/releases)

**What's Included:**
- **7 LV2 Plugins**: Disyn, Chatterbox, ChatGen, PM-Synth, Floozy, Floozy-Poly, Drumkit
- **Flues-Synth**: Headless Raspberry Pi synthesizer with 29 MIDI programs
- **Multi-Platform Binaries**: Optimized builds for each target platform
- **Complete Documentation**: Algorithm reference, MIDI spec, build guides

**Multi-Platform Build System:**
- **Desktop plugins** built on Ubuntu with `-march=native` optimization (AVX, SSE)
- **Raspberry Pi synth** built natively on Pi 4 with ARM-specific optimization
- **Three distribution formats**: Full release, plugins-only, synth-only
- Build process documented in [scripts/MULTI_PLATFORM_BUILD.md](scripts/MULTI_PLATFORM_BUILD.md)

**Flues-Synth Expansion (29 MIDI Programs):**

Expanded from 8 to 29 programs, leveraging 17 Disyn algorithms (7 original + 10 extended with param3) through creative signal chain routing:

- **Programs 0-7**: Original physical modeling presets
- **Programs 8-17**: Distortion synthesis explorations (ModFM Formant, DSF Bells, PAF Vowels, Hybrid chains)
- **Programs 18-28**: Experimental combinations (Morphing engines, feedback networks, spectral animators)

Each program includes optimized slider mappings (9 sliders) and level safety. See [flues-synth/docs/PROGRAM_CHANGE.md](flues-synth/docs/PROGRAM_CHANGE.md) for complete program guide.

**Backwards Compatibility:**
- OscillatorModule updated with optional param3 support (default 0.5)
- All existing code continues to work without modification
- New algorithms accessible via explicit param3 parameter

**Algorithm Documentation:**
- Comprehensive [algorithms.md](flues-synth/docs/algorithms.md) with implementation-level DSP detail
- Signal flow diagrams and level calibration notes
- Complete MIDI reference in [flues-synth/docs/midi.md](flues-synth/docs/midi.md)

---

## Reference Materials

- [CLAUDE.md](CLAUDE.md) - Project guidelines and development practices
- [AGENTS.md](AGENTS.md) - Agent collaboration notes
- [Physical Audio Signal Processing](http://ccrma.stanford.edu/~jos/pasp/) - Julius O. Smith III
- [Distortion Synthesis](https://csoundjournal.com/issue11/distortionSynthesis.html) - Victor Lazzarini
- Wikipedia: [Physical Modelling Synthesis](https://en.wikipedia.org/wiki/Physical_modelling_synthesis), [Digital waveguide synthesis](https://en.wikipedia.org/wiki/Digital_waveguide_synthesis), [Karplus-Strong](https://en.wikipedia.org/wiki/Karplus%E2%80%93Strong_string_synthesis)

---

## Future Directions

- **Hardware deployment**: Port to Daisy Seed / Electro-Smith platform
- **Polyphony**: Add voice allocation to remaining plugins
- **Preset system**: Cross-platform preset storage
- **WebAssembly**: Compile C/C++ engines to WASM for web
- **MPE support**: Polyphonic expression for expressive controllers
- **More hybrid experiments**: What happens when formants feedback through delay lines?

---

**License**: See individual subdirectories
**Author**: Danny Ayers
**Status**: Active development, expect changes
