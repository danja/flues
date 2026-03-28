# BassGen Plugin Requirements

## Summary

BassGen is an LV2 MIDI generator plugin that creates monophonic bassline patterns as MIDI note on/off events. It follows the existing Flues LV2 pattern: host-synced transport handling, X11/Cairo UI, LV2 metadata in TTL, and session persistence for both controls and generated pattern data.

The plugin is intended to generate musically usable bass phrases from a small set of user-selected characteristics, randomness, and genre-oriented heuristics. It should support full regeneration, pitch-only mutation, and rhythm-only mutation without forcing the user to rebuild a pattern from scratch.

## Core Behavior

- Generates a monophonic MIDI bassline pattern.
- Pattern length is `8-32` beats.
- Pattern playback is synchronized to the host DAW transport.
- Pattern position is bar-relative.
- Pattern restarts predictably when transport starts.
- Pattern resets correctly when the host transport is rewound.
- Output pattern and relevant control state are persisted across sessions.

## Timing Model

- Pattern length is measured in beats, not just abstract steps.
- A separate subdivision control determines rhythmic resolution.
- Initial supported subdivisions should be:
  - `1/8`
  - `1/16`
  - `1/16T`
- The generated internal pattern grid should be derived from:
  - `length_beats`
  - `subdivision`

## Monophonic Output Rules

- The output is strictly monophonic.
- Note lengths are derived from the rhythm and phrase structure, not set directly by a dedicated note-length knob.
- If a note is followed by another note, the note-off for the first note must be emitted before the next note-on.
- A small safety gap should be enforced between adjacent notes so downstream monophonic instruments do not receive overlapping note-ons.
- Tied or held notes should be represented as a single sustained note where possible rather than repeated retriggers.

## Transport Rules

- BassGen should use host `time:Position` when available.
- Playback should be bar-relative, so the same bar position always produces the same phrase position.
- If transport is stopped, note state should be cleaned up so stuck notes are avoided.
- If the transport jumps backwards or is rewound to an earlier position, BassGen should reset its playback state and resume from the correct pattern position for that bar-relative time.
- MIDI clock fallback is optional for the first version; host `time:Position` support is the primary requirement.

## Controls

### Required Musical Controls

- `root note`
  - Knob
  - Integer MIDI note or root-class + octave representation
- `scale`
  - Dropdown list
- `genre`
  - Dropdown list
  - Used to bias phrase heuristics
- `channel`
  - Dropdown list
  - MIDI channels `1-16`
  - Default `1`
- `length`
  - Knob
  - Range `8-32` beats
- `subdivision`
  - Dropdown list
  - Initial values: `1/8`, `1/16`, `1/16T`
- `density`
  - Knob
  - Controls proportion of beats/subdivisions that contain notes

### Recommended Additional Controls

- `register`
  - Knob
  - Biases note range/octave for the bassline
- `hold`
  - Knob
  - Biases sustained notes vs shorter repeated notes
- `accent`
  - Knob
  - Biases velocity emphasis on strong beats
- `seed`
  - Knob
  - Allows deterministic generation and repeatable mutations

### Action Controls

- `new`
  - Button
  - Generate an entirely new bassline
- `notes`
  - Button
  - Make minor pitch changes while preserving rhythmic structure as much as possible
- `rhythm`
  - Button
  - Make minor rhythmic changes while preserving melodic contour/order as much as possible

## Genre Heuristics

Genres should not fully determine the phrase; they should bias generation. Initial genres should be simple but musically distinct, for example:

- `Techno`
- `Acid`
- `House`
- `Electro`
- `Dub`
- `Ambient`

Genre heuristics may influence:

- onset density
- likelihood of syncopation
- preference for repeated notes
- preference for root/fifth/octave emphasis
- phrase movement vs static ostinato behavior
- average note sustain
- accent placement

## Generation Model

Generation should be done in two passes:

1. Generate rhythm
   - decide note starts, rests, ties, and durations
2. Generate pitch
   - assign notes based on root, scale, genre, register, and phrase heuristics

### Rhythm Heuristics

- Strong beats should be favored, but not always occupied.
- Density should affect onset probability rather than simply truncating a fixed number of notes.
- Consecutive onsets should be limited to avoid machine-gun phrasing unless genre bias suggests otherwise.
- Sustains/ties should be possible and influenced by `hold`.
- Phrase boundaries should be recognized at bar starts and pattern ends.

### Pitch Heuristics

- Root note should be favored on phrase starts and strong positions.
- Fifth and octave should be common stable targets.
- Passing motion should be mostly stepwise on weak beats.
- Large leaps should be rare and usually resolved.
- Repeated notes should be allowed and often desirable in basslines.
- Output notes should stay within a constrained bass register.

## Mutation Rules

- `new`
  - Rebuild rhythm and pitch entirely from current controls.
- `notes`
  - Keep note timing structure where possible.
  - Re-pitch existing note events using the same genre/root/scale constraints.
- `rhythm`
  - Keep overall phrase identity where possible.
  - Alter onset placement and durations while preserving enough pitch material to sound related.

## Persistence

Persistence should cover both:

- control values
- generated pattern data

This is important because the pattern is not just a direct reflection of control port values. A regenerated pattern on reload would be incorrect if the user expects the exact previously generated phrase to return.

The recommended implementation is:

- expose user controls as LV2 control ports
- store generated pattern data using LV2 State extension

Persisted pattern data should include at minimum:

- step count
- note start positions
- note pitches
- note durations or gate structure
- velocities or accent values if used
- currently selected genre and seed

## UI Requirements

The plugin should follow the established Flues raw X11/Cairo UI pattern rather than a toolkit-heavy UI. The layout should group generation controls clearly and make the action buttons prominent.

Suggested blocks:

- `Global`
  - root note, scale, genre, channel
- `Pattern`
  - length, subdivision, density, register, hold, accent, seed
- `Actions`
  - new, notes, rhythm

Future enhancement:

- a simple phrase preview panel showing note positions and relative pitch

## MIDI Output Requirements

- MIDI output port emits note on/off events only.
- Output channel is user-selectable from `1-16`.
- Default channel is `1`.
- Velocity may be fixed initially, but support for accent-derived velocity is desirable in v1.
- Note-offs must always be sent reliably when notes end, transport stops, or transport rewinds.

## Implementation Notes

The most suitable existing implementation references in this repository are:

- `lv2/euclid-mono`
  - for transport-synced MIDI generation and compact control-panel UI
- `lv2/chatgen`
  - for MIDI event scheduling patterns
- `lv2/e-mix`
  - for explicit LV2 state save/restore

Recommended internal structure:

- generator module for phrase creation
- pattern representation struct/class
- transport/playback scheduler
- LV2 wrapper
- X11/Cairo UI

## First Version Scope

The first version should include:

- host-synced monophonic bassline playback
- root, scale, genre, channel, length, subdivision, density
- `new`, `notes`, `rhythm`
- deterministic seed
- session persistence of generated phrase
- strict non-overlapping note scheduling

The first version does not need:

- incoming chord detection
- external MIDI input analysis
- pattern drawing/editing
- polyphony
- preset browser
