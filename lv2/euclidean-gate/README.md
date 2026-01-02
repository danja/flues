# Euclidean Gate

**Tempo-synced Euclidean gating LV2 effect**

Euclidean Gate applies a Euclidean rhythm to an input signal. In Gate mode, the rhythm opens the signal with an AD envelope; in Mute mode, the rhythm briefly silences the signal with the same envelope shape.

## Features

- **Stereo audio in/out**
- **Euclidean pattern** synced to host tempo (16 steps per bar)
- **AD envelope** per hit (attack + decay)
- **Gate or Mute mode** for creative rhythmic shaping
- **Random add/subtract** for evolving patterns

## Build

```bash
cmake -S lv2/euclidean-gate -B lv2/euclidean-gate/build
cmake --build lv2/euclidean-gate/build
cmake --install lv2/euclidean-gate/build --prefix ~/.lv2
```

Verify:
```bash
lv2ls | grep euclidean-gate
lv2info https://danja.github.io/flues/plugins/euclidean-gate
```

## Controls

- **Beats** (0-16): Euclidean pulses per 16-step cycle
- **Offset** (0-15): rotation offset
- **Attack (ms)**: envelope attack time
- **Decay (ms)**: envelope decay time
- **Mode**: Gate (open on beats) or Mute (mute on beats)
- **Random Add** (0.0-1.0): probability to insert extra beats
- **Random Subtract** (0.0-1.0): probability to skip beats

## Notes

- The plugin follows host transport via `time:Position`.
- In Gate mode, silence between hits is intentional; use Mute mode if you want the inverse.
