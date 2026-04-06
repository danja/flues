# GremlinDriver

GremlinDriver is a companion LV2 MIDI utility for [Gremlin](../gremlin/). It passes incoming MIDI through unchanged, while adding tempo-aware CC and action-note modulation that speaks Gremlin's existing MIDImix-oriented control mapping.

## Practical use

The simplest chain is:

`[BassGen] -> [GremlinDriver] -> [Gremlin]`

That lets note data flow straight through while GremlinDriver injects:

- macro fader CCs for Gremlin macros 1-8
- master trim CC
- Gremlin action notes such as burst, reseed, scene step, randomize, and panic

No special routing is needed in that setup because Gremlin already accepts controller MIDI on its main MIDI path.

## Build

```bash
cmake -S lv2/gremlin-driver -B lv2/gremlin-driver/build
cmake --build lv2/gremlin-driver/build
cmake --install lv2/gremlin-driver/build --prefix ~/.lv2
```

Or from the repo root:

```bash
./install-gremlin-driver.sh
```

## Controls

### Global

- `Clock Mode`: `Transport` follows host play/stop when the host provides `time:Position`; `Free` ignores transport and runs from the local BPM port.
- `BPM`: fallback clock in `Free` mode, and fallback tempo when the host does not provide BPM.
- `Randomise`: one-shot Gremlin patch scramble. This sends a fresh random set of Gremlin direct-knob CCs, so it changes the underlying patch rather than just jolting the macro lanes for one block.

### Continuous lanes

There are four continuous modulation lanes. Each lane has:

- `Target`: `Off`, `Macro 1` to `Macro 8`, or `Master`
- `Shape`: `Sine`, `Triangle`, `Ramp`, `SampleHold`, `RandomWalk`, `Logistic`
- `Rate`: exponential mapping from very slow multi-bar movement to rapid per-step wobble
- `Depth`: modulation width around the chosen center
- `Center`: midpoint of the lane output

If multiple lanes hit the same target, GremlinDriver averages them before sending the CC.

### Trigger lanes

There are two trigger lanes for discrete Gremlin actions:

- `Reseed`
- `Burst`
- `Rand Source`
- `Rand Delay`
- `Rand All`
- `Scene Down`
- `Scene Up`
- `Panic`
- `Mode Down`
- `Mode Up`

Each trigger lane has:

- `Action`
- `Rate`
- `Chance`

When a trigger step lands and passes its probability check, GremlinDriver emits the corresponding Gremlin button note.

## Suggested starting setup

This is a good first patch against Gremlin defaults:

- `Lane 1`: `Macro 1`, `Sine`, medium rate, medium-high depth
- `Lane 2`: `Macro 3`, `RandomWalk`, slower rate
- `Lane 3`: `Macro 6`, `SampleHold`, slow rate, deeper movement
- `Lane 4`: `Macro 8`, `Logistic`, medium rate, modest depth
- `Trigger 1`: `Burst`, slow rate, low chance
- `Trigger 2`: `Scene Up`, very slow rate, moderate chance

## Notes

- The current build is deliberately macro-first. It does not try to automate Gremlin's 24 direct knobs.
- `Randomise` is the exception: it emits a one-shot burst across Gremlin's 24 direct controller CCs so the sound actually lands somewhere new.
- Modulation is MIDI-rate rather than audio-rate, which is the practical choice for Gremlin's performance layer.
- The status output ports expose the current lane values, trigger flashes, transport state, and effective BPM for hosts or a future custom UI.
- In generic host UIs, `Randomise` may appear as a toggle. If so, click it off and on again to retrigger it.
