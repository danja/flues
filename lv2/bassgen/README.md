# Flues BassGen

BassGen is a monophonic LV2 MIDI bassline generator. It creates host-synced phrases from root, scale, genre, length, subdivision, density, and related controls, and persists the generated pattern with LV2 state.

Current genre list includes `Techno`, `Acid`, `House`, `Electro`, `Dub`, `Ambient`, `Funk`, and `Sabbath`. Scale choices include the original modal/pentatonic set plus `Locrian` and `Phrygian Dominant` for darker material.

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
- Modular DSP internals split into pattern, variation, transport, and state helpers
- UI for the main controls and action buttons
- Dropdown selectors for scale, genre, channel, and subdivision
- Root-note name display and compact phrase preview panel
- Exact pattern persistence through LV2 state
- Loop-aware `Vary` control for gradual phrase evolution

## Notes

- `New` regenerates the whole phrase.
- `Notes` mutates pitch while keeping the current rhythm shape.
- `Rhythm` mutates note timing while preserving the current note set as much as possible.
- `Vary` runs from `0-100%`: low values make occasional small note nudges, higher values progressively introduce note/rhythm regeneration, and `100%` fully regenerates every loop.
- `Funk` biases toward syncopated, clipped, octave-friendly lines.
- `Sabbath` biases toward heavier downbeats, longer holds, and darker riff movement.
- The preview panel is schematic: it reflects the current controls closely, but it is not yet a direct rendering of the persisted DSP pattern.
