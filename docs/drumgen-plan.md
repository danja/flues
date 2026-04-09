# DrumGen Plugin Plan

## Summary

`drumgen` should be an LV2 MIDI drum-pattern generator in `lv2/drumgen/`, conceptually related to `lv2/bassgen` but polyphonic and lane-based. It should follow the existing Flues pattern:

- host-synced transport handling via `time:Position`
- raw X11/Cairo UI
- LV2 metadata in TTL
- exact pattern persistence through LV2 State

The key practical design decision is to make it a **hybrid preset + Euclidean generator** rather than a pure step sequencer or a lane-by-lane clone of `euclid-mono`.

- Pure preset playback would feel too static.
- Pure Euclidean generation on every lane would lose recognizable drum-machine genre identity.
- The useful middle ground is:
  - genre/style templates provide anchor hits
  - Euclidean masks add, remove, or rotate hits around those anchors
  - the final result stays musically legible while still generating variation

## Goals

- Generate polyphonic drum MIDI patterns, synchronized to host transport.
- Default MIDI output to channel `10`, with user-selectable channel `1-16`.
- Default note mapping to the existing `lv2/drumkit` voice layout.
- Support genre/style selection with recognizable rhythmic bias.
- Produce deterministic patterns from a seed.
- Persist the exact generated pattern, not just the current control values.
- Provide a compact UI with genre/channel selectors, macro controls, and a pattern preview.

## Recommended First-Version Scope

The first version should stay focused and avoid turning into a full drum workstation.

- MIDI output only, no audio synthesis.
- Host `time:Position` sync is required.
- Optimize for `4/4` first; read host meter, but tune the style heuristics mainly for `4/4`.
- Pattern length `1-4` bars.
- Resolution options:
  - `1/8`
  - `1/16`
  - `1/16T`
- Eight drum lanes in v1:
  - Kick
  - Clap
  - Snare
  - Crash
  - Closed Hat
  - Low Tom
  - Open Hat
  - High Tom
- Default note mapping should match `lv2/drumkit`:
  - Kick `36`
  - Clap `39`
  - Snare `40`
  - Crash `41`
  - Closed Hat `42`
  - Low Tom `45`
  - Open Hat `46`
  - High Tom `50`
- Accessory `drumkit` voices can be added later without changing the basic architecture:
  - Bash `51`
  - Cowbell `52`
  - Clave `53`
- If `drumgen` needs a mapping selector, the default should be `Flues Drumkit`, with `GM` as an alternate preset rather than the default.

I would not make every lane expose the full `euclid-mono` A/B logic block in v1. That would explode the UI and make the plugin feel more like a generic programming tool than a genre drum generator.

## Compatibility With Flues Drumkit

The actual `lv2/drumkit` engine mapping is:

- Kick `36`
- Clap `39`
- Snare `40`
- Crash `41`
- Closed Hat `42`
- Low Tom `45`
- Open Hat `46`
- High Tom `50`
- Bash `51`
- Cowbell `52`
- Clave `53`

That mapping is defined in the engine switch table, so `drumgen` should target it directly by default.

One implementation detail worth noting: `drumkit` documentation and TTL text emphasize MIDI channel `10`, but its current wrapper accepts note events from any incoming MIDI channel because it ignores the lower channel nibble on note-on. Even so, `drumgen` should still default its output to channel `10` because that is the expected drum-routing convention and matches the requested behavior.

## Practical Core Design

### 1. Lane-Based Pattern Grid

Unlike `bassgen`, which stores monophonic note events, `drumgen` should store a fixed lane/step grid.

Suggested representation:

```c
struct DrumStepCell {
    uint8_t velocity;   // 0 means rest
    uint8_t flags;      // accent, fill, choke, etc.
};

struct DrumLaneState {
    int32_t midi_note;
    DrumStepCell steps[kMaxPatternSteps];
};

struct DrumPatternStateBlob {
    int32_t version;
    int32_t bars;
    int32_t steps_per_bar;
    int32_t total_steps;
    int32_t generation_serial;
    DrumLaneState lanes[kLaneCount];
};
```

This is simpler than an event list, easier to preview in the UI, and easier to persist exactly.

### 2. Genre Template Layer

Each genre should define a template describing the expected role of each lane.

Recommended initial genre list:

- Rock
- Disco
- Shuffle
- Electro
- Dub
- Motorik
- Bossa
- Afro

Each template should include:

- anchor probabilities per lane on a normalized bar grid
- preferred density ranges
- preferred swing/triplet bias
- Euclidean defaults per lane:
  - pulses
  - length
  - offset range
  - combine mode

Example genre behavior:

- `Rock`
  - preserve kick downbeats and snare backbeats
  - hats mostly straight
  - low Euclidean influence on kick/snare, moderate on hats/toms
- `Disco`
  - four-on-the-floor kick
  - strong backbeat clap/snare
  - open-hat lift on offbeats
- `Shuffle`
  - triplet or swung hat feel
  - looser snare placement
  - lighter Euclidean variation, more velocity play
- `Electro`
  - sparser anchors
  - stronger Euclidean rotation on hats, crash, and tom lanes
- `Dub`
  - lots of space
  - sparse kick and delayed-feel auxiliary hits

### 3. Euclidean Variation Layer

The Euclidean side should not replace the template entirely. It should act as a structured mutation source.

Per lane, generation can follow this order:

1. Build the template anchor mask.
2. Build a Euclidean mask using the current seed, density, variation, and lane defaults.
3. Combine them using a lane-specific rule.

Recommended combine rules:

- Kick: anchor-preserving `OR`
- Clap: usually derived from or layered against the snare lane, with optional offbeat additions
- Snare: anchor-preserving `OR`, with backbeat protection in relevant genres
- Crash: sparse `OR`, mostly phrase starts and fill boundaries
- Closed Hat: `OR` or `XOR` depending on variation amount
- Open Hat: sparse `OR`, filtered to avoid collisions
- Low Tom / High Tom: mainly fill-oriented, with conservative base usage outside fills

This is the main mechanism that creates the “Rhythm Ace meets Euclidean” result.

### 4. Musical Cleanup Pass

After the combined masks are built, the pattern should go through a cleanup pass:

- prevent open and closed hat from landing together unless explicitly allowed
- keep essential genre anchors when density is reduced
- keep snare/clap relationships musically sensible
- cap over-busy kick/snare behavior
- keep crash usage sparse and tom usage mostly fill-oriented
- add fill behavior in the last bar of the cycle when `fill` is high
- assign velocity accents on strong beats and phrase starts
- optionally add small random velocity/hit omission when `humanize` is non-zero

## Proposed Controls

### Global Controls

- `genre`
  - enum selector
- `channel`
  - enum selector `1-16`
  - default `10`
- `kit_map`
  - enum selector
  - default `Flues Drumkit`
  - optional alternate: `GM`
- `bars`
  - integer `1-4`
- `resolution`
  - enum selector: `1/8`, `1/16`, `1/16T`
- `density`
  - overall hit amount
- `variation`
  - how far generation can move away from anchors
- `euclid_mix`
  - how strongly Euclidean masks affect the result
- `swing`
  - timing feel for straight resolutions
- `fill`
  - intensity of end-of-cycle fill behavior
- `humanize`
  - velocity/dropout randomness
- `seed`
  - deterministic generation

### Lane Macro Controls

To keep the UI usable, I would start with four lane-group macro controls instead of exposing every per-lane Euclidean parameter.

- `kick_amt`
- `backbeat_amt`
- `hat_amt`
- `aux_amt`

These macros bias the internal lane probabilities:

- `backbeat_amt` affects snare and clap strength
- `hat_amt` affects both closed/open hat behavior
- `aux_amt` affects crash and tom activity

### Action Controls

Recommended action buttons, following the `bassgen` pattern:

- `new`
  - regenerate the whole pattern
- `mutate`
  - keep the broad genre feel, but reroll Euclidean offsets and local variation
- `fill_refresh`
  - reroll the final-bar fill material only

## Transport and Scheduling

The transport model should follow `bassgen` more closely than `euclid-mono`.

- Read `time:Position`, `time:bar`, `time:barBeat`, `time:beatsPerBar`, and `time:beatsPerMinute`.
- Compute absolute beat position and convert to pattern step position.
- Playback should be bar-relative, so rewinding to the same bar position reproduces the same pattern position.
- On transport stop or rewind, pending note state must be cleared.

Suggested scheduler flow:

1. Read host time info.
2. Compute `abs_steps_start` and `abs_steps_end`.
3. For each crossed integer step boundary:
   - find `local_step = boundary % total_steps`
   - emit all lane hits scheduled at that step
4. Queue short note-offs for compatibility with instruments that care about note duration.

Important difference from `bassgen`:

- `bassgen` only tracks one active note.
- `drumgen` must allow multiple simultaneous drum notes, so it needs a small pending note-off queue rather than a single `active_note`.

For v1, MIDI clock fallback from `euclid-mono` should be considered optional. Host `time:Position` is enough for the first version.

## Persistence

Like `bassgen`, `drumgen` should persist both:

- control values
- exact generated pattern data

This matters because the generated drum grid is not a direct function of the current port values alone. If the plugin only persisted ports, reopening a session could produce a different pattern from the one the user actually saved.

Recommended LV2 state approach:

- control ports for normal automation and host persistence
- `LV2_State_Interface` for a `DrumPatternStateBlob`
- include:
  - bars
  - steps per bar
  - exact lane grid
  - lane note mapping
  - generation serial
  - current seed

## UI Plan

The UI should follow the existing Flues raw X11/Cairo pattern used by `bassgen` and other LV2 plugins.

Suggested layout:

- `Global`
  - genre, channel, kit map, bars, resolution
- `Feel`
  - density, variation, euclid mix, swing, fill, humanize, seed
- `Lanes`
  - kick/backbeat/hat/aux macro controls
- `Actions`
  - new, mutate, fill refresh
- `Preview`
  - lane-vs-step grid with bar separators

The preview should be more useful than a decorative sketch. Since the generator uses a stored lane grid, the UI can render the actual persisted pattern structure directly.

## Implementation Plan

### 1. Scaffold the Plugin

- Create `lv2/drumgen/`
- Add:
  - `CMakeLists.txt`
  - `README.md`
  - `drumgen.lv2/manifest.ttl`
  - `drumgen.lv2/drumgen.ttl`
  - `src/drumgen_plugin.cpp`
  - `src/ui/drumgen_ui_x11.c`

### 2. Define Ports and State

- atom input port for host transport
- atom MIDI output port
- control ports for the v1 parameters
- LV2 State interface for exact pattern persistence

### 3. Implement Pattern Generator

- mapping presets for `Flues Drumkit` and optional `GM`
- `GenreTemplate` definitions
- Euclidean mask builder
- template + Euclidean combiner
- cleanup/fill/accent pass
- deterministic RNG seeded from `seed`

### 4. Implement Scheduler

- bar-relative step scheduling
- polyphonic note emission
- short note-off queue
- restart/rewind cleanup

### 5. Build the UI

- selector widgets for genre/channel/kit-map/resolution
- knobs for macro controls
- action buttons
- step-grid preview

### 6. Validate Behavior

Manual validation in a host should cover:

- channel defaulting to `10`
- channel changes to `1-16`
- `Flues Drumkit` note compatibility
- transport start/stop/rewind
- deterministic regeneration from fixed seed
- state save/restore
- sensible genre differences
- no stuck notes

## Risks and Mitigations

### Risk: Too Many Controls

If every lane gets full Euclidean A/B editing, the plugin becomes unwieldy.

Mitigation:

- start with global and macro lane controls
- keep full per-lane editing for a later version if needed

### Risk: The Result Sounds Too Random

If Euclidean logic dominates, genre identity disappears.

Mitigation:

- protect anchor hits for kick/snare
- keep `euclid_mix` explicit
- make genre templates responsible for the base feel

### Risk: Drum Note Mappings Differ Across Targets

`lv2/drumkit` is the most relevant local target, but other drum plugins may expect General MIDI or something else.

Mitigation:

- default to `Flues Drumkit`
- provide a `kit_map` selector so `GM` can be used when needed

### Risk: Meter Support Gets Messy

Some styles assume `4/4`, while others want `3/4` or triplet-heavy phrasing.

Mitigation:

- tune v1 for `4/4`
- keep the transport code meter-aware
- add dedicated non-`4/4` styles only after the base architecture is stable

## Recommended Direction

The most practical path is:

1. Treat `bassgen` as the main structural reference for transport sync, state persistence, selectors, and “generate/mutate” workflow.
2. Borrow only the Euclidean math and feel concepts from `euclid-mono`, not the entire lane-by-lane UI model.
3. Build `drumgen` around a persisted multi-lane step grid with genre anchors and Euclidean mutation.

That should produce a plugin that feels like a real musical drum generator rather than a stripped-down step editor or an opaque randomizer.
