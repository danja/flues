# PM Synth GTK

A physical modeling synthesizer with GTK4 interface, translated from the JavaScript/Web Audio implementation in `experiments/pm-synth`.

## Features

- **12 Interface Types**: Pluck, Hit, Reed, Flute, Brass, Bow, Bell, Drum, Crystal, Vapor, Quantum, Plasma
- **Modular DSP Architecture**: Sources, Envelope, Interface, Delay Lines, Feedback, Filter, Modulation, Reverb
- **Real-time Audio**: PulseAudio backend with threaded processing
- **GTK4 UI**: Native Linux desktop interface with sliders and controls
- **Keyboard Input**: Play notes using computer keyboard (A-K = C4-C5)

## Architecture

The application is structured following the same modular design as the JavaScript version:

```
gtk-synth/
├── include/              # Header files
│   ├── pm_synth.h           # Main synth engine API
│   ├── dsp_modules.h        # DSP module interfaces
│   ├── interface_strategy.h # Interface strategy pattern
│   ├── dsp_utils.h          # DSP utility functions
│   └── audio_backend.h      # Audio backend interface
├── src/
│   ├── audio/            # Audio engine implementation
│   │   ├── pm_synth_engine.c
│   │   ├── audio_backend_pulse.c
│   │   └── modules/      # DSP modules
│   │       ├── sources_module.c
│   │       ├── envelope_module.c
│   │       ├── interface_module.c
│   │       ├── interface_strategy.c
│   │       ├── delay_lines_module.c
│   │       ├── feedback_module.c
│   │       ├── filter_module.c
│   │       ├── modulation_module.c
│   │       ├── reverb_module.c
│   │       └── strategies/
│   │           ├── reed_strategy.c      (full implementation)
│   │           └── all_strategies_stub.c (stubs for remaining 11)
│   └── ui/               # GTK4 user interface
│       └── synth_window.c
├── reference/            # Original JavaScript code for reference
│   ├── modules/          # Copied from experiments/pm-synth/src/audio/modules
│   └── docs/             # Copied from experiments/pm-synth/docs
└── meson.build           # Build configuration
```

## Building

### Dependencies

#### Ubuntu/Debian
```bash
sudo apt install build-essential meson ninja-build
sudo apt install libgtk-4-dev libpulse-dev
```

#### Fedora
```bash
sudo dnf install meson ninja-build
sudo dnf install gtk4-devel pulseaudio-libs-devel
```

#### Arch Linux
```bash
sudo pacman -S meson ninja
sudo pacman -S gtk4 libpulse
```

### Compile and Install

```bash
cd gtk-synth
meson setup builddir
meson compile -C builddir
meson install -C builddir
```

Or to build without installing:
```bash
cd gtk-synth
meson setup builddir
ninja -C builddir
./builddir/pm-synth-gtk
```

**Note:** If you modify source files and ninja doesn't detect changes, force a rebuild:
```bash
touch src/ui/synth_window.c  # or whichever file changed
ninja -C builddir
```

## Usage

1. **Start the application**: Run `pm-synth-gtk` (or `./builddir/pm-synth-gtk` if not installed)
2. **Enable audio**: Click the "Click Here First" button to initialize audio
3. **Select interface**: Choose from the dropdown (Reed, Pluck, etc.)
4. **Play notes**: Use keyboard keys A-K to play C4-C5
   - `A` = C4, `W` = C#4, `S` = D4, `E` = D#4, `D` = E4, `F` = F4
   - `T` = F#4, `G` = G4, `Y` = G#4, `H` = A4, `U` = A#4, `J` = B4, `K` = C5
5. **Adjust parameters**: Use the sliders in the tabbed interface

## Controls

### Sources Tab
- **DC**: Constant DC offset (0-100%)
- **Noise**: White noise level (0-100%)

### Envelope Tab
- **Attack**: Attack time (1ms-1000ms)
- **Release**: Release time (10ms-5000ms)
- **Intensity**: Interface behavior intensity (0-100%)

### Feedback Tab
- **Delay 1**: Feedback amount from first delay line (0-100%)
- **Delay 2**: Feedback amount from second delay line (0-100%)

## Implementation Status

### Complete ✓
- [x] Core synth engine structure
- [x] All 8 DSP modules (Sources, Envelope, Interface, DelayLines, Feedback, Filter, Modulation, Reverb)
- [x] Interface Strategy pattern infrastructure
- [x] Reed interface (full implementation)
- [x] PulseAudio backend
- [x] GTK4 UI with basic controls
- [x] Keyboard input
- [x] Meson build system

### TODO
- [ ] Complete implementations for remaining 11 interface strategies
  - Translate from `reference/modules/interface/strategies/*.js`
  - Currently using simple tanh() stubs
- [ ] Add remaining UI controls:
  - Tuning, Ratio (Delay Lines)
  - Filter Feedback (Feedback)
  - Filter Frequency, Q, Shape (Filter)
  - LFO Frequency, Modulation Depth (Modulation)
  - Reverb Size, Level (Reverb)
- [ ] Add waveform visualizer
- [ ] JACK backend support
- [ ] MIDI input support
- [ ] Preset saving/loading

## Translating Interface Strategies

To complete an interface strategy implementation:

1. Read the JavaScript implementation in `reference/modules/interface/strategies/`
2. Create a new C file in `src/audio/modules/strategies/`
3. Follow the pattern used in `reed_strategy.c`:
   - Define implementation data structure
   - Create vtable with function pointers
   - Implement process, reset, set_intensity, set_gate, destroy functions
   - Export the creator function
4. Remove the stub from `all_strategies_stub.c`
5. Add the new .c file to `meson.build`

## Signal Flow

The signal flow matches the JavaScript implementation:

1. **Sources** → Generate DC, noise, tone
2. **Envelope** → Apply attack/release envelope
3. **Feedback** → Mix delayed signals (DC-blocked)
4. **Interface** → Nonlinear physical modeling behavior
5. **Delay Lines** → Dual pitch-tuned delay lines
6. **Filter** → State-variable filter (LP/BP/HP)
7. **Modulation** → LFO (AM ↔ FM)
8. **Reverb** → Schroeder reverb
9. **Output** → Final signal

**Key detail**: DC blocker is only applied to the feedback path, allowing DC from sources to bias the interface operating point (essential for reed/brass/bow interfaces).

## References

- Original JavaScript implementation: `experiments/pm-synth/`
- Algorithm research: `reference/docs/interface-algorithms-research.md`
- Signal flow documentation: `reference/docs/interface-signal-flow.md`

## License

Part of the Flues project. See main repository for license information.
