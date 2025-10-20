# Floozy LV2 Plugin

Floozy is a hybrid LV2 instrument that fuses the distortion-rich source algorithms from **Disyn** with the resonant acoustic signal chain of the **PM Synth**. The result is a single-voice synthesizer capable of combining aggressive spectral content with the expressive feedback, filtering, and modulation found in the Stove pipeline.

## Signal Flow

```
Disyn Source (7 algorithms + level)
        │
Noise + DC injection
        │
PM Envelope ─┐
Interface Module (12 behaviour models)
        │
Dual Delay Lines (tuning / ratio / feedback)
        │
Filter & Feedback Loop
        │
Modulation (AM↔FM) & Reverb
        │
Master Gain → Audio Out
```

### Source Engines
- Seven Disyn oscillator modes (Dirichlet, DSF single/double, Tanh Square, Tanh Saw, PAF, Modified FM)
- Two algorithm-specific parameters mapped 0–1
- Tone level (post oscillator), Noise level, DC offset controls

### Interface & Envelope
- 12 PM interface strategies (Pluck, Hit, Reed, Flute, Brass, Bow, Bell, Drum, Crystal, Vapor, Quantum, Plasma)
- Envelope gate with attack/release
- Interface intensity morphs the non-linear interaction

### Pipe & Delay
- Dual Karplus delay lines with tuning, ratio, and independent feedback returns
- Additional feedback tap into the filter bus

### Filter & Modulation
- State-variable filter with morphable shape, Q, frequency
- AM↔FM modulation module with bipolar depth and LFO frequency

### Reverb & Output
- Schroeder reverb (size/level)
- Master gain post processing

## Build

```bash
cmake -S lv2/floozy -B lv2/floozy/build
cmake --build lv2/floozy/build
cmake --install lv2/floozy/build --prefix ~/.lv2   # optional
```

Dependencies: `lv2`, `libx11-dev`, `libcairo2-dev`, `pkg-config`, `cmake`, and a C++17 toolchain.

## UI

The plugin ships with a native X11/Cairo control panel (`floozy_ui.so`) organised into five rows:
1. **Source Engines** – all source controls in a single row
2. **Interface + Envelope**
3. **Delay Lines**
4. **Filter & Feedback**
5. **Modulation, Reverb, Master**

Knobs respond to drag, mouse-wheel, and MIDI port updates. Algorithm selection displays discrete labels for each mode.

## MIDI

Monophonic: note-on triggers the engine with frequency-transposed oscillator + pipe; note-off releases the envelope. All-notes-off CCs flush the voice state.

## Directory Layout

```
lv2/floozy/
├── CMakeLists.txt             # Build definitions for DSP + UI
├── floozy.lv2/
│   ├── floozy.ttl             # LV2 metadata, port definitions
│   └── manifest.ttl           # Bundle manifest with UI
└── src/
    ├── FloozyEngine.hpp       # Hybrid DSP core
    ├── floozy_plugin.cpp      # LV2 entry points
    ├── modules/
    │   └── FloozySourceModule.hpp   # Disyn+PM source wrapper
    └── ui/
        └── floozy_ui_x11.c    # X11/Cairo control surface
```
