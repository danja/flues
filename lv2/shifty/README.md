# Shifty

Shifty is a transport-synchronised LV2 audio effect for block-based pitch shifting. It tracks the active division from host transport, exposes editable semitone values per division, and now includes a first real-time granular/resampling pitch-shift core with an explicit `Wet %` control and transition smoothing.

## Build

```bash
cd lv2/shifty
cmake -S . -B build
cmake --build build
cmake --install build --prefix ~/.lv2
```

From the repo root:

```bash
./install-shifty.sh
```

## Current Status

- Stereo audio in/out LV2 effect
- Host `time:Position` parsing
- Block-bars and division-count controls
- `Wet %` defaults to `100` for fully shifted output
- Sixteen editable semitone division controls
- UI highlighting of the active transport-selected division
- First granular pitch-shift implementation with smoothed transport-selected shift changes
- Pass-through safety when transport timing is unavailable or stopped

## Notes

- `active_division` and `active_shift` are exposed as output control ports so the UI can reflect transport state.
- The current pitch-shift core is intentionally simple and pragmatic. It should be treated as a first-pass real-time engine, not a high-quality spectral shifter.
