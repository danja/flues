# Flues Euclid

Euclidean/stochastic rhythm generator LV2 plugin that outputs MIDI note-on events for the Flues Drumkit mapping. Each instrument has Beats, Offset, Length, and Random controls. Global controls set the step grid, swing, and seed.

## Mapping (Drumkit Compatible)

| Voice | MIDI Note |
| --- | --- |
| Kick | 36 |
| Snare | 40 |
| Clap | 39 |
| Closed Hi-Hat | 42 |
| Open Hi-Hat | 46 |
| Lo Tom | 45 |
| Hi Tom | 50 |
| Crash | 41 |
| Bash | 51 |
| Cowbell | 52 |
| Clave | 53 |

## Timing

- 8-24 step grid (default 16)
- MIDI Clock (F8) takes priority when present (Start/Stop/Continue + Song Position Pointer supported)
- Falls back to `time:Position` tempo when no MIDI clock
- Time signature from `time:beatsPerBar`/`time:beatUnit` controls MIDI clock sync
- Swing stretches odd steps (0-1)
- Transport stop resets the pattern and reseeds

## MIDI CC Control

The control port accepts MIDI CC messages. CC values are mapped to parameters and override the UI ports after first use:

- Beats: CC 20-28 (Kick..Bash), Cowbell=90, Clave=91
- Offset: CC 30-38 (Kick..Bash), Cowbell=92, Clave=93
- Random: CC 40-48 (Kick..Bash), Cowbell=94, Clave=95
- Length: CC 50-58 (Kick..Bash), Cowbell=96, Clave=97
- Steps: CC 70
- Swing: CC 71
- Seed: CC 72 (scaled to 0-65535)

## Build

```bash
cd lv2/euclid
cmake -S . -B build
cmake --build build
cmake --install build --prefix ~/.lv2
```

## Usage

Route MIDI from Euclid into Drumkit on the same track or via MIDI routing in your DAW.
