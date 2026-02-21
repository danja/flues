# E-Mix

E-Mix is a transport-synced Euclidean mixer LV2 effect. It applies play/silence blocks over a bar-length cycle, with optional fade-in/out inside each active block.

## Controls

- Total Bars (default 128)
  Pattern cycle length in bars.
- Division (default 16)
  Number of equal blocks in the cycle.
- Steps (default 8)
  Number of active Euclidean blocks.
- Offset (default 0)
  Rotation of the Euclidean pattern.
- Fade (default 0)
  Fade length in bars, applied at start/end of active blocks.

Notes:
- Fade bars are part of each active block.
- If transport is stopped or host time is unavailable, E-Mix passes audio through.
- Parameters are persisted with LV2 state.

## Build

```sh
cmake -S lv2/e-mix -B lv2/e-mix/build
cmake --build lv2/e-mix/build
cmake --install lv2/e-mix/build --prefix ~/.lv2
```

## Files

- DSP: `lv2/e-mix/src/e_mix_plugin.cpp`
- UI: `lv2/e-mix/src/ui/e_mix_ui_x11.c`
- Metadata: `lv2/e-mix/e-mix.lv2/e-mix.ttl`
