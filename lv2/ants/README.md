# Ants

Ants is a cellular-automata MIDI generator. A swarm of ants moves across a 16x16 grid mapped to scale degrees. Ants move in steps and emit notes with round-robin selection, using pheromone trails and randomness to steer the swarm.

## Quick Start

1. Insert Ants on a MIDI track.
2. Start transport (Ants uses host tempo).
3. Set Ants, Voices, Scale, Root, and movement parameters.

## Controls

- Ants (2–8)
- Voices (1–4 notes per tick)
- Scale (Chromatic, Major, Natural Minor, Harmonic Minor, Melodic Minor, Pentatonic Major/Minor, Blues, Dorian, Mixolydian)
- Root (C–B)
- Steps (1–4 per beat)
- Speed (0.25x, 0.5x, 1x, 2x, 4x)
- Random (0–1)
- Trail (0–1)
- Decay (0–1)
- Note Len (0.1–1.0 beats)
- Gap (0–4 beats between triggers, per ant)
- Velocity (0–1)
- Density (0–1, chance a candidate ant emits on a tick)

## Build & Install

From the repo root:

```sh
cmake -S lv2/ants -B lv2/ants/build
cmake --build lv2/ants/build
cmake --install lv2/ants/build --prefix ~/.lv2
```

Or:

```sh
./ants-install.sh
```

Dependencies:
- LV2 headers
- X11 + Cairo (UI)
- CMake + a C/C++ toolchain

## Development Notes

- DSP: `lv2/ants/src/ants_plugin.cpp`
- UI: `lv2/ants/src/ui/ants_ui_x11.c`
- Metadata: `lv2/ants/ants.lv2/*.ttl`
