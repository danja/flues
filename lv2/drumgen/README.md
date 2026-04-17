# Flues DrumGen

DrumGen is a transport-synced LV2 MIDI drum-pattern generator. It is designed to pair cleanly with `lv2/drumkit`, defaulting to MIDI channel `10` and the `drumkit` note layout.

Current implementation status:

- host `time:Position` sync
- polyphonic MIDI drum note generation
- genre-biased lane templates with Euclidean variation
- explicit fill motifs layered onto the last bar
- modular DSP internals split into pattern, variation, transport, and state helpers
- exact pattern persistence through LV2 State
- X11/Cairo UI with core controls, action buttons, and grid preview
- `Flues Drumkit` and `GM` note-map presets
- loop-aware `Vary` control for gradual pattern evolution

## Build

```bash
cd lv2/drumgen
cmake -S . -B build
cmake --build build
cmake --install build --prefix ~/.lv2
```

From the repo root:

```bash
./install-drumgen.sh
```

## Controls

- Selectors: Genre, Channel, Kit Map, Bars, Resolution
- Feel: Density, Variation, Fill, Vary, Seed
- Lane Macros: Kick, Backbeat, Hat, Tom, Metal, Perc
- Actions:
  - `New` regenerates the full pattern
  - `Mutate` rerolls the pattern with the current feel settings
  - `Fill` refreshes the last bar more aggressively
- `Variation` still controls within-pattern rhythmic complexity and Euclidean looseness.
- `Vary` runs from `0-100%` and mutates the pattern across repeated loops: low values make small bar-scale nudges every several bars, while `100%` fully regenerates every loop.

## Default DrumKit Mapping

- Kick `36`
- Clap `39`
- Snare `40`
- Crash `41`
- Closed Hat `42`
- Low Tom `45`
- Open Hat `46`
- High Tom `50`
- Bash `51`
- Cowbell `52`
- Clave `53`
