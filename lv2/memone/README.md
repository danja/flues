# Memone LV2

Memone is a stereo LV2 audio effect that performs online time-series prediction using a lightweight LSTM. It listens for a tempo-synced warmup period and then outputs the predicted audio stream (fully wet).

## Features
- Stereo in/out audio effect (inputs are summed to mono for prediction, output duplicated to L/R).
- Online learning with truncated backpropagation through time (BPTT) on the LSTM.
- Tempo sync via LV2 Time extension with manual BPM fallback.
- Warmup period in beats (default 4) before predictions are output.
- Minimal X11/Cairo UI for core controls.

## Controls
- **BPM**: Manual tempo fallback when host transport data is missing.
- **Warmup Beats**: Number of beats to learn before outputting predictions.
- **Predict Gain**: Output gain for the predicted stream.
- **Predict Horizon**: Short-horizon delay in samples for training/preview.
- **Learning Rate**: Online BPTT update rate.
- **BPTT Length**: Number of steps used for truncated backprop.
- **Grad Clip**: Gradient clamp threshold for stability.
- **LR Clamp**: Toggle clamping the learning rate.
- **LR Min**: Lower bound for clamped learning rate.
- **LR Max**: Upper bound for clamped learning rate.
- **Hidden Size**: Active LSTM hidden units (reinitializes model).
- **Reset**: Clears model state and warmup counter.

## Build
```bash
cd lv2/memone
cmake -S . -B build
cmake --build build
cmake --install build --prefix ~/.lv2
```

## Notes
- The model state is not persisted between runs.
- If the host provides LV2 Time, Memone prefers that tempo; otherwise it uses the manual BPM control.
