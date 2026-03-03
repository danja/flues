# Flues EuclidMono

Two-pattern Euclidean/stochastic rhythm generator LV2 plugin that emits a single MIDI note. Pattern A and Pattern B are combined using boolean logic with per-pattern inversion.

## Features

- Single note output (MIDI note selectable from UI edit box)
- Two Euclidean pattern blocks (A/B): Beats, Offset, Length, Random
- Logic block: Invert A, Invert B, and operator (AND, OR, XOR, NAND, NOR, XNOR)
- Global controls: Steps (8-24), Swing, Seed
- Per-block randomize (`R`) plus global randomize-all
- Host transport sync via `time:Position` and MIDI Clock fallback (F8/FA/FB/FC + SPP)

## Persistence

All controls are LV2 control ports, so host session save/restore persists:

- Pattern sliders
- Logic controls (invert flags and operator)
- MIDI note
- Global controls

## Build

```bash
cd lv2/euclid-mono
cmake -S . -B build
cmake --build build
cmake --install build --prefix ~/.lv2
```

Or from repo root:

```bash
./install-euclid-mono.sh
```
