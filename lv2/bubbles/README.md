# Bubbles LV2 Plugin

Physical-model inspired water synthesizer for flowing, bubbling, and dripping textures.

## Build

```bash
cd lv2/bubbles
cmake -S . -B build
cmake --build build
cmake --install build --prefix ~/.lv2
```

## Verify

```bash
lv2ls | grep bubbles
lv2info https://danja.github.io/flues/plugins/bubbles
```

## Host Usage
- Insert `Flues Bubbles` as an instrument plugin.
- Send MIDI note-on to start water generation.
- Send note-off to release/stop.
- MIDI pitch maps to effective bubble `Size` (lower notes = larger bubbles, higher notes = smaller bubbles).
- MIDI velocity maps directly to output loudness.

## Controls
- `Intensity`: overall synthesis energy.
- `Density`: event rate for bubbles and drips.
- `Size`: bubble size distribution (bigger = lower resonances).
- `Flow Rate`: continuous turbulence level.
- `Brightness`: source spectral brightness.
- `Resonance`: body modal Q.
- `Depth`: underwater/muffled character blend.
- `Space`: stereo feedback-space amount.
- `Randomness`: timing/size variation.
- `Heat`: boiling tendency (bubble clustering).
- `Output`: final gain.
- `Mode`: `Flow`, `Bubble`, `Drip`, `Underwater`, `Hybrid`.

## Current Model (MVP)
- Turbulence noise source (flow bed)
- Stochastic bubble event voices
- Stochastic drip/splash micro-events
- 4-mode resonant body filter bank
- Stereo feedback delay space stage
- Nonlinear output soft clipping
- Per-mode source and event-rate routing

## Mode Notes
- `Flow`: strongest continuous stream bed, lighter events.
- `Bubble`: dense bubble cloud with reduced flow hiss.
- `Drip`: sparse background with stronger droplets/splashes.
- `Underwater`: muffled low-mid movement and softened highs.
- `Hybrid`: balanced blend of all components.

This is the first implementation pass based on `docs/bubbles-plan.md`.
