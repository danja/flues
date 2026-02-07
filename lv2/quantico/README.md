# Quantico

Quantico is an LV2 MIDI utility plugin that snaps incoming notes to the nearest pitch in a selected key and scale.

## Quick Start

1. Insert Quantico on a MIDI track.
2. Choose Key (C–B) and Scale.
3. Play or route MIDI into the track.

## Controls

- Key (0–11)
  Root note used for the scale.
- Scale (Chromatic, Major, Natural Minor, Harmonic Minor, Melodic Minor, Pentatonic Major, Pentatonic Minor, Blues, Dorian, Mixolydian)
  Notes are quantized to the nearest pitch in the selected scale.

Notes:
- Note On/Off events are quantized.
- Other MIDI events pass through unchanged.

## Build & Install

From the repo root:

```sh
cmake -S lv2/quantico -B lv2/quantico/build
cmake --build lv2/quantico/build
cmake --install lv2/quantico/build --prefix ~/.lv2
```

Or use the helper script:

```sh
./quantico-install.sh
```

Dependencies:
- LV2 headers
- X11 + Cairo (for the UI)
- CMake + a C/C++ toolchain

## Development Notes

- DSP: `lv2/quantico/src/quantico_plugin.cpp`
- UI: `lv2/quantico/src/ui/quantico_ui_x11.c`
- Metadata: `lv2/quantico/quantico.lv2/*.ttl`
