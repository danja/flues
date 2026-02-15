# ArpIso

**Euclidean gravity arpeggiator LV2 plugin for Novation Launchpad Mini MK3**

ArpIso uses the full 8x8 Launchpad grid as a playable isomorphic note field. Held pads become gravity wells (max 5, one-hand design), and each well drives a playhead with Euclidean timing derived from pad position.

## Current Scaffold Features

- 8x8 grid reserved for performance notes (no control overlays)
- Up to 5 simultaneous held wells
- Per-well pitch mapping (isomorphic + scale quantization)
- Per-well Euclidean pulses/offset from row/column
- Moving playheads that retarget between active wells
- Top row + right column dedicated to controls
- Launchpad LED feedback via Programmer mode + bulk updates
- Host transport/BPM sync via `time:Position`
- MIDI note output for downstream synths

## Build

```bash
cmake -S lv2/arpiso -B lv2/arpiso/build
cmake --build lv2/arpiso/build
cmake --install lv2/arpiso/build --prefix ~/.lv2
```

## Control Map (Current)

Top row (CC 91-98):
- 91: Start/Stop
- 92: Clock division
- 93: Cycle length
- 94: Root note
- 95: Scale
- 96: Motion mode
- 97: Clear wells
- 98/99: Panic + clear

Right side (CC 19-89):
- Density bias, phase bias, gravity strength, travel scale
- Gate length, velocity curve, humanize, pattern A/B

## Notes

This is an implementation scaffold: architecture and control flow are in place, with room for deeper motion physics and richer UI behavior in the next pass.
