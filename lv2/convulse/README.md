# Convulse

Convulse is an experimental LV2 effect that treats a small built-in synth as a convolution-kernel generator. Instead of loading an impulse response from disk, it renders a short stereo FIR kernel from an oscillator mode, then convolves incoming audio against that kernel.

The result sits somewhere between resonator, spectral filter, smearer, and weird hybrid reverb. It works with mono or stereo inputs, and stereo width comes from generating slightly different left/right kernels.

## Features

- Stereo audio in/out with mono fallback
- Internal kernel synth with six modes:
  - `0` Sine
  - `1` Saw
  - `2` Pulse
  - `3` Noise
  - `4` FM
  - `5` Chirp
- Short RT-safe direct FIR convolution
- Kernel refresh with smooth crossfade
- Optional tempo-aware refresh via `time:Position`
- Wet feedback for unstable resonant textures
- Raw X11/Cairo UI matching the other LV2 plugins in this repo

## Controls

- `Dry/Wet`: dry versus convolved signal
- `Kernel Size`: FIR length in samples
- `Mode`: selects the internal waveform family
- `Pitch`: base frequency for kernel rendering
- `Shape`: mode-dependent timbre control
- `Decay`: short versus long kernel tail
- `Refresh`: zero freezes the current kernel; otherwise the kernel is periodically regenerated
- `Stereo Width`: phase and detune difference between left and right kernels
- `Feedback`: wet signal fed back into the convolver input

When host tempo is available and transport is running, `Refresh` is interpreted as regenerations per quarter note. Without tempo information it falls back to Hz.

## Build

```sh
cmake -S lv2/convulse -B lv2/convulse/build
cmake --build lv2/convulse/build
cmake --install lv2/convulse/build --prefix ~/.lv2
```

Verify:

```sh
lv2ls | grep convulse
lv2info https://danja.github.io/flues/plugins/convulse
```

## Notes

- This first implementation uses direct short-kernel convolution, not partitioned FFT convolution.
- Large kernel sizes plus high feedback can get aggressive quickly.
- `Mode` is intentionally numeric in the custom UI for now; hosts that read scale points can show the labels.
