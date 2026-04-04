# Flues Gremlin

Gremlin is a monophonic LV2 instrument built around unstable digital source circuits, chaotic modulation, and a deliberately temperamental delay core. It is designed as a live-tweakable malfunction instrument rather than a clean subtractive synth.

## Sound

The engine combines:

- A pitchable digital source with four unstable modes
- Chaotic control signals derived from logistic, tent, and Henon-style maps
- Sample-rate reduction and quantisation for broken converter textures
- Foldback and saturation stages for collapse / overload behavior
- A stereo delay network with modulation, stutter grabs, cross-feedback, and damping
- A dedicated direct-MIDImix performance layer on top of the regular LV2 ports

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

Gremlin now ships with a raw X11/Cairo UI for hosts such as Reaper. The panel mirrors the live effective values from the internal MIDImix layer rather than only the host automation ports, and the layout now follows the MIDImix strip arrangement more closely.

## Host Controls

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
- `Source Gain`: source level before downstream destruction
- `Burst`: transient/strike emphasis
- `Pitch Spread`: detune / interval spread between unstable source components
- `Delay Mix`: wetness of the delay network
- `Cross Feedback`: left/right feedback bleed
- `Glitch Length`: average duration of the short repeat grabs
- `Chaos Rate`: how quickly the chaotic control maps evolve
- `Duck`: how much the dry injection backs off when the feedback loop gets loud

## MIDImix Layout

Gremlin now has a built-in mapping for the Akai MIDImix factory control layout, so you do not need host MIDI learn for normal use.

### Routing

- `MIDI In`: note performance input
- `Controller In`: optional second MIDI input intended for the MIDImix
- `Controller Out`: MIDI feedback output for MIDImix LEDs

If your host only delivers one MIDI source per plugin input, connect your keyboard or sequencer to `MIDI In` and the MIDImix to `Controller In`.
If you want LED feedback, route `Controller Out` back to the MIDImix device.

### Knobs

- Top row: `Damage`, `Chaos`, `Noise`, `Drift`, `Crunch`, `Fold`, `Attack`, `Release`
- Middle row: `Delay Time`, `Feedback`, `Warp`, `Stutter`, `Tone`, `Damping`, `Space`, `Output`
- Bottom row: `Source Gain`, `Burst`, `Pitch Spread`, `Delay Mix`, `Cross Feedback`, `Glitch Length`, `Chaos Rate`, `Duck`

### Faders

- Channel faders 1-8 are live macros for: Source, Pitch, Breakage, Delay, Space, Stutter, Tone, Output
- Master fader is a final trim after the engine

### Buttons

- `REC ARM` 1-4: select modes `Shard`, `Servo`, `Spray`, `Collapse`
- `REC ARM` 5-8: load four scenes: `Splinter`, `Melt`, `Rust`, `Tunnel`
- `MUTE` row is momentary performance control while held:
  `Freeze`, `Stutter Max`, `Chaos Boost`, `Crunch Blast`, `Feedback Push`, `Warp Push`, `Noise Burst`, `Duck Kill`
- `SOLO` + `MUTE` combinations trigger actions:
  `Reseed`, `Burst`, `Random Source`, `Random Delay`, `Random All`, `Prev Scene`, `Next Scene`, `Reset/Panic`
- `BANK LEFT/RIGHT`: step backward/forward through the four source modes

### Notes

- The direct MIDImix mapping assumes the controller is using the factory/default identifiers.
- Gremlin drives `REC ARM` LEDs for mode/scene state and `MUTE` LEDs for the momentary layer. While `SOLO` is held, the alternate `SOLO+MUTE` action bank is lit.
- All third-row MIDImix controls are also normal LV2 control ports now, so Gremlin is fully host-automatable.
- Host automation and the standard LV2 ports still work. Once you touch a directly-mapped MIDImix control, that parameter becomes locally owned by the controller until you hit the `Reset/Panic` combo.
- The X11 UI includes a MIDImix status panel showing controller activity, scene, master trim, macro faders, and the momentary button layer, so you still have visual feedback if the hardware LEDs are dead.
