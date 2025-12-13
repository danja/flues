# Flues Project v{VERSION} Release Notes

## Overview

Flues is a collection of experimental physical modeling and distortion synthesis instruments for Linux. This release includes 7 LV2 plugins and a standalone headless synthesizer for Raspberry Pi.

## What's New in v{VERSION}

### Major Features
- **10 New Distortion Synthesis Algorithms** - Expanded from 7 to 17 algorithms in Disyn/Floozy
  - Combination algorithms (7-13): Hybrid Formant, Cascaded, Parallel Bank, Feedback, Morphing, Inharmonic, Adaptive Filter
  - Novel extrapolations (14-16): Multi-Stage, Frequency Asymmetry, Cross-Mod
- **param3 Support** - All new algorithms support 3-parameter control via CC 19
- **Program 6 Re-enabled** - Full Hybrid program fixed after race condition resolution
- **29 MIDI Programs** - Complete program set with dynamic slider remapping

### Bug Fixes
- Fixed race condition in interface_module.c that caused segfaults
- Fixed param3 integration across C/C++ boundary
- Calibrated output levels for all 17 distortion algorithms
- Fixed CC message routing for programs 18-27

### Improvements
- Added program6-test for stability verification
- Updated all documentation (algorithms.md, midi.md, PROGRAM_CHANGE.md)
- Improved slider mappings for new programs
- Better level management (dual DC blocking, formant makeup gain)

## Included Components

### LV2 Plugins (7 total)

1. **Disyn** - Distortion synthesis instrument
   - 7 primitive algorithms (Dirichlet, DSF Single/Double, Tanh Square/Saw, PAF, ModFM)
   - Attack/Release envelope
   - Schroeder reverb
   - Monophonic

2. **Floozy** - Hybrid distortion + physical modeling
   - Disyn source block feeds PM signal chain
   - 12 interface types (Reed, Pluck, Hit, Flute, Brass, Bow, Bell, Drum, Crystal, Vapor, Quantum, Plasma)
   - Full modulation and reverb
   - X11/Cairo UI

3. **Chatterbox** - Formant speech synthesizer
   - Source-filter vocal tract model
   - 4 formants (F1-F4) + optional nasal
   - Vocal modes: Nasal, Sing (vibrato), Shout, Fry
   - IPA vowel quadrilateral joystick
   - 20 parameters via MIDI CC

4. **ChatGen** - Text-to-speech MIDI generator
   - English text → phoneme parsing
   - 40+ phoneme support
   - Generates Note On/Off + 7 MIDI CCs
   - Designed for serial routing with Chatterbox
   - Half-note timing (2 beats per phoneme)

5. **Drumkit** - Industrial drum synthesizer
   - 8 synthesized voices (Kick, Snare, Clap, Toms, Hi-Hats, Crash)
   - Master FX: Bit crusher, distortion, reverb
   - General MIDI note mapping (36-50)
   - Velocity sensitive

6. **PM-Synth** - Physical modeling synthesizer
   - 8 DSP modules (Sources, Envelope, Interface, Delays, Feedback, Filter, Modulation, Reverb)
   - 12 interface strategies
   - X11/Cairo UI (raw, no GTK dependency)

7. **Flues-Control** - MIDI CC controller
   - 29 program selector
   - 9 slider controls (CC: 73, 72, 28, 30, 74, 71, 1, 27, 7)
   - Context-dependent slider remapping
   - Designed for controlling flues-synth from DAW

### Flues-Synth (Headless Raspberry Pi)

- Monophonic synthesis engine
- 29 MIDI programs with dynamic parameter routing
- 17 distortion synthesis algorithms
- ALSA MIDI/audio backends
- Optimized for Raspberry Pi 4
- 48kHz sample rate, ~10ms latency

## Installation

### System Requirements

- Linux (x86_64 or ARM)
- ALSA (libasound2)
- X11 and Cairo (for plugin UIs)
- LV2 host application (Ardour, Reaper, Carla, etc.)

### Quick Install

```bash
# Extract tarball
tar xzf flues-v{VERSION}-linux-{ARCH}.tar.gz
cd flues-v{VERSION}

# Install (requires root)
sudo ./install.sh
```

### Manual Install

**LV2 Plugins:**
```bash
cp -r lv2-plugins/* ~/.lv2/
```

**Flues-Synth:**
```bash
sudo cp flues-synth/flues-synth /usr/local/bin/
sudo chmod +x /usr/local/bin/flues-synth
```

### Verify Installation

```bash
# Check LV2 plugins
lv2ls | grep flues

# Test plugin
jalv.gtk https://danja.github.io/flues/plugins/disyn

# Run synth
flues-synth
```

## Usage Examples

### Chatterbox + ChatGen (Text-to-Speech)

1. Load both plugins on same MIDI track in DAW
2. Route: `[ChatGen] → [Chatterbox] → Audio Out`
3. Type text in ChatGen UI
4. Press Play in DAW
5. Text converted to speech via formant synthesis

### Flues-Synth (Raspberry Pi)

```bash
# Auto-detect audio device
flues-synth

# Specify device
flues-synth hw:2,0

# Enable debug
FLUES_MIDI_DEBUG=1 flues-synth
```

**MIDI Control:**
- Program Change 0-28: Select synthesis program
- CC 73-7: Hardware sliders (context-dependent)
- Notes 60-84: Chromatic keyboard (C4-C6)

### Floozy (Hybrid Synthesis)

1. Load in DAW or run standalone: `jalv.gtk https://danja.github.io/flues/plugins/floozy`
2. Select Disyn algorithm (0-16)
3. Adjust algorithm parameters (param1, param2, param3)
4. Choose interface type (Reed, Pluck, etc.)
5. Blend distortion source with physical model feedback

## Documentation

Complete documentation in `docs/` directory:

- **FLUES-SYNTH.md** - Synthesis engine architecture
- **algorithms.md** - 17 distortion synthesis algorithms reference
- **midi.md** - MIDI CC mapping and control reference
- **PROGRAM_CHANGE.md** - 29 program descriptions
- **PROGRAM6_FIX.md** - Race condition fix notes

## Known Issues

- Flues-synth is monophonic only (polyphony planned for future release)
- Plugin UIs require X11 (no Wayland support yet)
- ChatGen phoneme parsing is basic (no prosody or timing variation)

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for detailed version history.

## Building from Source

See project README for build instructions:
```bash
git clone https://github.com/danja/flues.git
cd flues
./scripts/build-release.sh {VERSION}
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for development guidelines.

## License

See individual component licenses in source repository.

## Credits

- Design & Implementation: Danny Ayers
- Development assisted by: Claude (Anthropic)

## Support

- Issues: https://github.com/danja/flues/issues
- Discussions: https://github.com/danja/flues/discussions
