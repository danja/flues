# Slimmer

Slimmer is a monophonic MIDI filter that reduces chords to a single note with controlled spacing. It is intended for feeding mono synths while keeping musical intent via selectable note selection strategies.

## Quick Start

1. Insert Slimmer on a MIDI track feeding a monophonic synth.
2. Choose a Mode (selection strategy).
3. Set Gap and Min Hold to enforce space between notes.

## Controls

- Mode: Highest, Lowest, Nearest, Farthest, Velocity, Most Recent, Oldest, Center, Round Robin, Random.
- Gap (ms): Silence enforced after note-off before next note-on.
- Min Hold (ms): Minimum time a note must stay on before it can be released.
- Retrigger: If on, re-triggers the current note when the chosen note is the same.

Notes:
- Only Note On/Off events are transformed. Other MIDI events pass through.
- The filter tracks held notes and chooses the next output note using the selected strategy.

## Build & Install

From the repo root:

```sh
cmake -S lv2/slimmer -B lv2/slimmer/build
cmake --build lv2/slimmer/build
cmake --install lv2/slimmer/build --prefix ~/.lv2
```

Or use the helper script:

```sh
./slimmer-install.sh
```

Dependencies:
- LV2 headers
- X11 + Cairo (for the UI)
- CMake + a C/C++ toolchain

## Development Notes

- DSP: `lv2/slimmer/src/slimmer_plugin.cpp`
- UI: `lv2/slimmer/src/ui/slimmer_ui_x11.c`
- Metadata: `lv2/slimmer/slimmer.lv2/*.ttl`
