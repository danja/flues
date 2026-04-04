# Flues Gremlin

Gremlin is a monophonic LV2 instrument built around unstable digital source circuits, chaotic modulation, and a deliberately temperamental delay core. It is designed as a live-tweakable malfunction instrument rather than a clean subtractive synth.

## Sound

The engine combines:

- A pitchable digital source with four unstable modes
- Chaotic control signals derived from logistic, tent, and Henon-style maps
- Sample-rate reduction and quantisation for broken converter textures
- Foldback and saturation stages for collapse / overload behavior
- A stereo delay network with modulation, stutter grabs, cross-feedback, and damping

It is intended to sit somewhere between glitch synth, broken delay box, and small feedback instrument.

## Build

```bash
cd lv2/gremlin
cmake -S . -B build
cmake --build build
cmake --install build --prefix ~/.lv2
```

Verify:

```bash
lv2ls | grep gremlin
lv2info https://danja.github.io/flues/plugins/gremlin
```

## Controls

- `Mode`: `Shard`, `Servo`, `Spray`, `Collapse`
- `Damage`: overall instability, drive, and edge
- `Chaos`: depth of chaotic modulation
- `Noise`: extra excitation / hiss / grit
- `Drift`: slower pitch wander and phase slop
- `Crunch`: sample-rate reduction and bit depth loss
- `Fold`: foldback amount before the delay network
- `Delay Time`: base unstable delay time
- `Feedback`: near-runaway feedback amount
- `Warp`: chaotic delay-time motion
- `Stutter`: short buffer grabs and repeat glitches
- `Tone`: brightness before the delay
- `Damping`: high-frequency loss inside the feedback loop
- `Space`: stereo offset and cross-feedback
- `Attack`: envelope attack
- `Release`: envelope release
- `Output`: final gain

## MIDImix Layout

The parameter set is arranged to be easy to learn onto an Akai MIDImix:

- Top row: `Damage`, `Chaos`, `Noise`, `Drift`, `Crunch`, `Fold`, `Attack`, `Release`
- Second row: `Delay Time`, `Feedback`, `Warp`, `Stutter`, `Tone`, `Damping`, `Space`, `Output`

`Mode` is the odd one out and works best on a spare knob or button mapping in the host.

This first pass does not hardcode MIDImix CC numbers. The idea is to let the host do MIDI learn, which is more flexible if you already use different MIDImix presets elsewhere.
