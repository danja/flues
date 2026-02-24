# Chordant

Chordant is a transport-synced Euclidean capture/mix LV2 effect.

## Controls

- Total Bars: cycle length in bars.
- Division: number of Euclidean steps in the cycle.
- Steps: number of Euclidean high steps.
- Offset: Euclidean rotation.
- Fade: crossfade bars around high-step boundaries.
- Max Segments: max stored capture segments.
- NoCap Pass: when ON and no captures exist, high step outputs pass-through.
- Clear Trig: when ON, captured segments clear after each high trigger.
- Cap Mode: capture length mode (0=1 step, 1=2 steps, 2=4 steps, 3=1 bar).

## Build

```sh
cmake -S lv2/chordant -B lv2/chordant/build
cmake --build lv2/chordant/build
cmake --install lv2/chordant/build --prefix ~/.lv2
```
