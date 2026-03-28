# Flues BassGen

BassGen is a monophonic LV2 MIDI bassline generator. It creates host-synced phrases from root, scale, genre, length, subdivision, density, and related controls, and persists the generated pattern with LV2 state.

## Build

```bash
cd lv2/bassgen
cmake -S . -B build
cmake --build build
cmake --install build --prefix ~/.lv2
```

From the repo root:

```bash
./install-bassgen.sh
```

## Current Status

- Monophonic MIDI note generation
- Host `time:Position` sync
- Bar-relative playback and rewind reset
- UI for the main controls and action buttons
- Dropdown selectors for scale, genre, channel, and subdivision
- Root-note name display and compact phrase preview panel
- Exact pattern persistence through LV2 state

## Notes

- `New` regenerates the whole phrase.
- `Notes` mutates pitch while keeping the current rhythm shape.
- `Rhythm` mutates note timing while preserving the current note set as much as possible.
- The preview panel is schematic: it reflects the current controls closely, but it is not yet a direct rendering of the persisted DSP pattern.
