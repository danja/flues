# Memone LV2 Implementation Plan

## Objectives
- Build a new LV2 audio effect at `lv2/memone` that predicts an input audio time series using an LSTM and outputs the prediction as an audio stream after a tempo-synced warmup period.
- Follow the established LV2 patterns in this repo (CMake build, C++ DSP plugin, optional X11/Cairo UI, `.lv2` bundle with TTL metadata).
- Keep dependencies minimal and self-contained like the other LV2 plugins.

## Scope (MVP)
- **Audio in/out effect**: stereo-in/stereo-out (summed to mono for prediction, duplicated output).
- **Tempo sync** via LV2 Time extension with a manual BPM fallback control.
- **Warmup beats** control: number of beats to listen/train before outputting predictions (default 4).
- **Predict-only output** (fully wet); silence during warmup is acceptable.
- **LSTM inference + online learning** running in the audio thread with bounded CPU usage.
  - Use truncated BPTT (short window) for gate + readout updates.

## Non-goals (initial release)
- Heavy ML frameworks or GPU acceleration.
- Multi-model presets or large model loading.
- Offline training and model export pipeline.

## Project Layout (LV2 pattern)
```
lv2/memone/
├── CMakeLists.txt
├── README.md
├── memone.lv2/
│   ├── manifest.ttl
│   └── memone.ttl
└── src/
    ├── memone_plugin.cpp        # LV2 plugin entry (audio + params)
    ├── MemoneEngine.hpp         # DSP coordinator
    ├── modules/
    │   ├── LstmModule.hpp        # LSTM forward (and optional online update)
    │   ├── TempoSync.hpp         # Beat/tempo tracking (LV2 time)
    │   ├── RingBuffer.hpp        # Input history and prediction queue
    │   └── DspUtils.hpp          # sigmoid/tanh helpers, clamps
    └── ui/
        └── memone_ui_x11.c       # Optional X11/Cairo UI
```

## Audio Signal Flow
```
Audio In -> Preprocess (gain/normalize?) -> LSTM -> Prediction Queue
                     |                       |
                     +------ Warmup gate ----+

Warmup Phase: output silence
Predict Phase: output predicted samples (fully wet)
```

## LV2 Ports (draft)
Audio:
- `in_l`, `in_r`
- `out_l`, `out_r`

Controls:
- `bpm` (manual BPM fallback)
- `beats_warmup` (number of beats to learn before output)
- `predict_gain` (output gain for predictions)
- `latency_ms` (optional informational or fixed value)
- `reset` (momentary trigger to clear state)
- `predict_horizon` (short-horizon delay in samples)
- `learning_rate` (online update rate)
- `bptt_length` (truncated backprop steps)
- `grad_clip` (gradient clamp threshold)
- `lr_clamp` (toggle learning rate clamp)
- `lr_min` (clamped learning rate floor)
- `lr_max` (clamped learning rate ceiling)
- `hidden_size` (active LSTM hidden units)

Outputs:
- `status` (0=Warmup, 1=Predict)

Atom/Time:
- `lv2:time` input for tempo/beat position sync (via `LV2_Time`).

## Tempo Sync Strategy
- Use LV2 Time extension to read `beatsPerMinute` and `barBeat`.
- Maintain a sample counter and compute samples-per-beat.
- Warmup completes when accumulated beats >= `beats_warmup`.
- Fallback to manual `bpm` when host does not send time info, but prefer transport when available.

## LSTM Module Design
- **Model size**: fixed dimensions for input/hidden/output to avoid dynamic allocations in the audio thread.
- **Inference**: compute `i, f, o, g` gates and hidden/output per sample (or per block if downsampling).
- **Numerical stability**: clamp activations, limit state magnitudes.
- **Online learning**: small-step gradient update with bounded cost; no persistence required.
  - Default to MSE loss, per-block update, and a conservative learning rate (tunable).
  - Start with a short prediction horizon (tunable).

### State Handling
- Ring buffer for input history and for predicted samples (short horizon).
- Clear state on `reset` or transport stop (if desired).
- No state persistence required across runs.

## UI (optional but consistent with repo)
- X11/Cairo UI similar to other LV2 plugins.
- Controls: Warmup Beats, Predict Gain, Manual BPM, Predict Horizon, Learning Rate, BPTT Length, Grad Clip, Reset button, Status LED (Warmup/Predict).
- Controls: Warmup Beats, Predict Gain, Manual BPM, Predict Horizon, Learning Rate, BPTT Length, Grad Clip, LR Clamp, LR Min, LR Max, Hidden Size, Reset button, Status LED (Warmup/Predict).
- Minimal display of current tempo and beat counter.

## Build & Install (pattern match)
- CMake target for DSP plugin (`memone`), plus optional UI (`memone_ui`).
- Install to `~/.lv2/memone.lv2` with TTLs.
- Dependencies: `lv2`, `x11`, `cairo`, `pthread`, `m` (UI only).

## Testing & Validation
- Unit-style harness for `LstmModule` (deterministic input -> expected output for fixed weights).
- Tempo sync tests: confirm beat counting with simulated BPM.
- Real-time smoke test in an LV2 host (Ardour/Reaper/Carla).

## Open Questions / Decisions Needed
- Confirm initial hyperparameters (learning rate, horizon length, block size) after first listening tests.

## Implementation Order
1. Scaffold `lv2/memone` with CMake, TTLs, and README mirroring other LV2 projects.
2. Implement `TempoSync` and ring buffer utilities.
3. Implement `LstmModule` (online learning) with fixed initial weights and tests.
4. Build `MemoneEngine` to glue input -> LSTM -> output with warmup gating.
5. Integrate LV2 plugin wrapper and control ports.
6. Add X11/Cairo UI if desired.
7. Validate in a host, iterate on CPU usage and sound quality.
