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
│   │       └── strategies/          # All 12 interface types
│   │           ├── pluck_strategy.c
│   │           ├── hit_strategy.c
│   │           ├── reed_strategy.c
│   │           ├── flute_strategy.c
│   │           ├── brass_strategy.c
│   │           ├── bow_strategy.c
│   │           ├── bell_strategy.c
│   │           ├── drum_strategy.c
│   │           ├── crystal_strategy.c
│   │           ├── vapor_strategy.c
│   │           ├── quantum_strategy.c
│   │           └── plasma_strategy.c
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
2. **Select interface**: Choose from the dropdown (Pluck, Hit, Reed, Flute, Brass, Bow, Bell, Drum, Crystal, Vapor, Quantum, Plasma)
3. **Play notes**: Use keyboard keys A-K to play C4-C5
   - `A` = C4, `W` = C#4, `S` = D4, `E` = D#4, `D` = E4, `F` = F4
   - `T` = F#4, `G` = G4, `Y` = G#4, `H` = A4, `U` = A#4, `J` = B4, `K` = C5
4. **Adjust parameters**: Use the knobs/sliders to control all DSP modules

## Controls

All controls are arranged in a single-page layout with 8 module sections:

### Row 1: Sources, Envelope, Interface
- **Sources**
  - DC: Constant DC offset (0-100%)
  - Noise: White noise level (0-100%)
  - Tone: Sawtooth tone level (0-100%, follows note pitch)
- **Envelope**
  - Attack: Attack time (0-100)
  - Release: Release time (0-100)
- **Interface**
  - Type: Dropdown selector (12 interface types)
  - Intensity: Interface behavior intensity (0-100%)

### Row 2: Delay Lines, Feedback
- **Delay Lines**
  - Tuning: Pitch tuning adjustment (0-100)
  - Ratio: Delay line length ratio (0-100)
- **Feedback**
  - Delay 1: Feedback from first delay line (0-99%, default 95%)
  - Delay 2: Feedback from second delay line (0-99%, default 95%)
  - Filter: Post-filter feedback (0-99%, default 0%)

### Row 3: Filter, Modulation, Reverb
- **Filter**
  - Frequency: Cutoff frequency (0-100)
  - Q: Resonance (0-100)
  - Shape: Filter shape morph LP→BP→HP (0-100)
- **Modulation**
  - LFO Freq: LFO frequency (0-100)
  - Depth: AM↔FM modulation depth (0-100)
- **Reverb**
  - Size: Room size (0-100)
  - Level: Wet/dry mix (0-100)

## Implementation Status

### Complete ✓
- [x] Core synth engine structure matching JavaScript exactly
- [x] All 8 DSP modules (Sources, Envelope, Interface, DelayLines, Feedback, Filter, Modulation, Reverb)
- [x] Interface Strategy pattern infrastructure
- [x] **All 12 interface strategies** fully implemented:
  - [x] Pluck - One-way damping with transient brightening
  - [x] Hit - Sharp waveshaper with adjustable hardness
  - [x] Reed - Pressure-driven reed model with flow/opening dynamics
  - [x] Flute - Soft jet instability with breath turbulence
  - [x] Brass - Asymmetric lip buzz nonlinearity
  - [x] Bow - Stick-slip friction with controllable bite
  - [x] Bell - Metallic waveshaping with evolving harmonics
  - [x] Drum - Energy accumulator with percussive drive
  - [x] Crystal - Inharmonic resonator with golden-ratio partials
  - [x] Vapor - Chaotic aeroacoustic turbulence (3 coupled oscillators)
  - [x] Quantum - Amplitude quantization with zipper artifacts
  - [x] Plasma - Electromagnetic waveguide with nonlinear dispersion
- [x] PulseAudio backend with threaded processing
- [x] Complete GTK4 UI with all 18 parameter controls
- [x] Keyboard input (computer keys A-K)
- [x] Meson build system
- [x] Signal flow exactly matches JavaScript (DC blocker on feedback path only)

### TODO
- [ ] Waveform visualizer
- [ ] JACK backend support
- [ ] MIDI input support
- [ ] Preset saving/loading
- [ ] Improve UI styling/aesthetics

## Interface Strategy Architecture

All 12 interface strategies follow a consistent pattern using the Strategy design pattern:

1. **Base class**: `InterfaceStrategy` struct with vtable pointer
2. **Vtable**: Function pointers for `process()`, `reset()`, `set_intensity()`, `set_gate()`, `destroy()`
3. **Implementation**: Each strategy has its own state struct and implements the vtable functions
4. **Factory**: `interface_strategy_create()` creates the appropriate strategy based on type enum

Example structure from `reed_strategy.c`:
```c
typedef struct {
    float reed_state;  // Strategy-specific state
} ReedImpl;

static float reed_process(InterfaceStrategy* self, float input) {
    // DSP algorithm translated from JavaScript
}

InterfaceStrategy* reed_strategy_create(float sample_rate) {
    // Allocate strategy and impl_data, return initialized instance
}
```

All strategies are direct line-by-line translations from the JavaScript originals in `reference/modules/interface/strategies/`.

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
