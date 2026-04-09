# Flues DrumGen

DrumGen is a transport-synced LV2 MIDI drum-pattern generator. It is designed to pair cleanly with `lv2/drumkit`, defaulting to MIDI channel `10` and the `drumkit` note layout.

Current implementation status:

- host `time:Position` sync
- polyphonic MIDI drum note generation
- genre-biased lane templates with Euclidean variation
- exact pattern persistence through LV2 State
- X11/Cairo UI with core controls, action buttons, and grid preview
- `Flues Drumkit` and `GM` note-map presets

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
- Feel: Density, Variation, Fill, Seed
- Lane Macros: Kick, Backbeat, Hat, Aux
- Actions:
  - `New` regenerates the full pattern
  - `Mutate` rerolls the pattern with the current feel settings
  - `Fill` refreshes the last bar more aggressively

## Default DrumKit Mapping

- Kick `36`
- Clap `39`
- Snare `40`
- Crash `41`
- Closed Hat `42`
- Low Tom `45`
- Open Hat `46`
- High Tom `50`

Accessory `drumkit` voices (`51` bash, `52` cowbell, `53` clave) are not generated yet.
