# Speculate

**Spectral modulation LV2 effect**

Speculate takes stereo audio, performs a short-time FFT, reshapes the spectrum, then reconstructs the signal. It is designed for shimmering shifts, smeared textures, and evolving freezes with tempo-synced motion.

## Features

- **Stereo FFT processing** (1024 samples, 50% overlap)
- **Shift**: moves spectral bins up/down
- **Blur**: smooths adjacent bins for a smeared timbre
- **Freeze**: blends in the previous spectrum to hold texture
- **Tempo-synced shift modulation**
- **Dry/Wet**: mix between original and processed signal

## Build

```bash
cmake -S lv2/speculate -B lv2/speculate/build
cmake --build lv2/speculate/build
cmake --install lv2/speculate/build --prefix ~/.lv2
```

Verify:
```bash
lv2ls | grep speculate
lv2info https://danja.github.io/flues/plugins/speculate
```

## Controls

- **Dry/Wet** (0.0-1.0): processed mix
- **Shift** (-0.2 to 0.2): spectral bin shift (down/up)
- **Blur** (0.0-1.0): spectral smoothing
- **Freeze** (0.0-1.0): hold magnitude from previous frames
- **Mod Depth** (0.0-1.0): tempo-synced shift modulation depth
- **Mod Factor** (0.02-8.0): modulation rate multiplier vs host tempo

## Notes

- The modulation uses host tempo via `time:Position`.
- Extreme Shift + Freeze values can be very glitchy.
- Output level can rise with heavy spectral buildup, so keep an eye on gain staging.
