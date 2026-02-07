# MIDI Flip

MIDI Flip is an LV2 utility plugin that mirrors incoming MIDI note events around a pivot note. An ascending line becomes descending, and vice versa.

## Quick Start

1. Insert MIDI Flip on a MIDI track.
2. Set Pivot (default C4 / 60).
3. Play or route MIDI into the track.

## Controls

- Pivot (0–127, default 60/C4)
  Notes are reflected around this value: `flipped = 2*pivot - note`.

Notes:
- Note On/Off events are flipped.
- Other MIDI events pass through unchanged.

## Build & Install

From the repo root:

```sh
cmake -S lv2/midi-flip -B lv2/midi-flip/build
cmake --build lv2/midi-flip/build
cmake --install lv2/midi-flip/build --prefix ~/.lv2
```

Or use the helper script:

```sh
./flip-install.sh
```

Dependencies:
- LV2 headers
- X11 + Cairo (for the UI)
- CMake + a C/C++ toolchain

## Development Notes

- DSP: `lv2/midi-flip/src/midi_flip_plugin.cpp`
- UI: `lv2/midi-flip/src/ui/midi_flip_ui_x11.c`
- Metadata: `lv2/midi-flip/midi-flip.lv2/*.ttl`
