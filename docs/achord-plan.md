# Achord Plan

## Goal

Build `Achord` as a Launchpad Mini MK3 driven LV2 MIDI instrument that reuses the hardware-routing and LED ideas from `lv2/arpiso`, but turns the 8x8 grid into a chord button table. Pressing a grid pad should emit a full chord, not a sequenced note. The feel should be closer to a compact accordion accompaniment board than a piano keyboard:

- columns should imply harmonic movement
- rows should imply chord family
- top buttons should handle slower global setup
- side buttons should handle fast performance modifiers

This should be a MIDI-only performance plugin with Launchpad LED feedback and an X11/Cairo mirror UI in the same general style as `arpiso`.

## Core Design

### Performance model

- The full 8x8 grid is reserved for chord pads.
- Top row (`CC 91-99`) and right-side buttons (`CC 19-89`) are dedicated controls, matching the physical convention already used by `arpiso`.
- Default behavior is immediate "latest chord wins" output so the instrument stays tight and accordion-like.
- Host transport is optional. Direct play should work without clock. If `time:Position` is available, Achord can also offer quantized, strummed, and repeated trigger modes.

### Harmonic idea

Use a modernized Stradella-style map:

- **Columns** move in perfect fifths.
- **Rows** select chord families and shell voicings.
- **Side buttons** add color tones or alter voicing without forcing the player to leave the root column.
- **Top buttons** shift the visible root bank, octave/register, and trigger behavior.

This is musically useful because:

- I, IV, V, ii, vi and secondary-dominant motion sit physically close together.
- The same shape works in every key.
- The player gets accordion-like functional harmony, but with a few extra jazz/synth rows.

## Grid Mapping

### Columns: roots in fifths

Each column represents a root. Left-to-right motion is by ascending fifth.

The visible 8-column bank is movable. It should be tonic-centered rather than hard-coded to C so the middle of the grid contains the most common functions around the chosen key center.

Example default bank for tonic C:

| Column | Root |
| --- | --- |
| 0 | Eb |
| 1 | Bb |
| 2 | F |
| 3 | C |
| 4 | G |
| 5 | D |
| 6 | A |
| 7 | E |

Shifting the bank left or right moves the whole window by one fifth.

### Rows: chord families bottom to top

Rows should keep stable meanings across all banks.

| Row | Name | Formula | Use |
| --- | --- | --- | --- |
| 0 | Bass Shell | `1 5 8` | Accordion-like bass support that is still chordal |
| 1 | Counterbass Shell | `3/b3 5 8` | First-inversion helper for smoother bass motion |
| 2 | Major | `1 3 5` | Core consonant triad |
| 3 | Minor | `1 b3 5` | Parallel minor triad |
| 4 | Dominant 7 | `1 3 5 b7` | Cadences, blues, secondary dominants |
| 5 | Diminished | `1 b3 b5 bb7/b7` | Leading-tone tension, use half-diminished when scale suggests it |
| 6 | Major 7 | `1 3 5 7` | Stable color, pop/jazz pads |
| 7 | Minor 7 | `1 b3 5 b7` | Modal, soul, jazz harmony |

Notes:

- Row 1 is the main accordion nod. It behaves like a slash-chord helper rather than a plain triad row.
- The selected scale should influence the adaptive rows/modifiers only:
  - Row 1 uses `3` or `b3` based on the current scale context.
  - Row 5 chooses diminished or half-diminished spelling when needed.
  - `Add9` and `Sus` modifiers use the scale for note choice.
- The main triad/seventh rows stay literal so the layout keeps muscle memory.

## Controls

### Top row: global controls (`CC 91-99`)

| CC | Function | Behavior |
| --- | --- | --- |
| 91 | Bank Left | Shift visible root bank one fifth left |
| 92 | Bank Right | Shift visible root bank one fifth right |
| 93 | Octave Down | Lower the base chord register by one octave |
| 94 | Octave Up | Raise the base chord register by one octave |
| 95 | Scale | Cycle scale context: Major, Natural Minor, Dorian, Mixolydian, Harmonic Minor, Blues, Chromatic |
| 96 | Register | Cycle output layering: `8'`, `16'+8'`, `8'+8'+4'`, `16'+8'+4'` |
| 97 | Trigger Mode | Cycle: Direct, Quantized 1/16, Strum Down, Strum Up, Repeat |
| 98 | Hold Mode | Cycle: Momentary, Latch, Stack Latch |
| 99 | Panic | All notes off, clear latches, reset transient modifiers |

Rationale:

- Top buttons are for slower, global choices the player changes between phrases.
- `Register` borrows directly from accordion thinking: the harmonic map stays the same while the output thickness changes.
- `Trigger Mode` makes the plugin useful both as a direct accompaniment surface and as a beat-aware chord machine inside a DAW.

### Right side: performance modifiers (`CC 19-89`, bottom to top)

| CC | Function | Behavior |
| --- | --- | --- |
| 19 | Bass | Add root one octave below the resolved chord |
| 29 | Add9 | Add a scale-aware ninth above the chord |
| 39 | Sus | Cycle `off -> sus2 -> sus4 -> off` |
| 49 | Inv- | Move inversion down one step |
| 59 | Inv+ | Move inversion up one step |
| 69 | Spread | Cycle voicing width: Close, Open, Drop-2 |
| 79 | Accent | Boost velocity and apply a slight upper-note delay for a stronger stab |
| 89 | Voice Lead | Prefer nearest-note voicing from the previous chord |

Rationale:

- Side buttons should reward live use while the hand is already on the grid.
- Harmonic color lives lower on the side strip (`Bass`, `Add9`, `Sus`).
- Voicing lives in the middle (`Inv-`, `Inv+`, `Spread`).
- Articulation lives near the top (`Accent`, `Voice Lead`).

## Output Behavior

### Chord triggering

- Pressing a grid pad emits Note On for the resolved chord notes.
- Releasing it emits matching Note Off unless the current hold mode latches it.
- Pad velocity becomes chord velocity.
- Bass-added notes should be slightly softer than the main chord by default so they support rather than dominate.

### Hold modes

- **Momentary**: latest grid pad wins. Pressing a new pad releases the previous chord.
- **Latch**: a chord stays active until the same pad is pressed again or a new pad replaces it.
- **Stack Latch**: up to 4 latched chords may coexist for drones or layered harmony. If a 5th is added, steal the oldest.

### Trigger modes

- **Direct**: notes sound immediately, no transport required.
- **Quantized 1/16**: note-on happens on the next sixteenth-note boundary when host transport is available; otherwise fall back to Direct.
- **Strum Down / Strum Up**: notes are slightly staggered low-to-high or high-to-low.
- **Repeat**: while the pad is held or latched, re-trigger the chord on the selected beat division.

### Modifier application rules

- Modifier changes should apply to newly triggered chords immediately.
- In repeat or latched modes, modifier changes may revoice on the next trigger boundary rather than mutating currently sounding notes mid-event.
- `Voice Lead` should work after chord resolution and before register doubling so the nearest inversion is chosen on the playable chord, not on the final doubled stack.

## LED Feedback

The hardware needs a clear visual language because the Launchpad cannot display text labels.

### Grid

- **Row hue** indicates chord family.
- **Brightness** indicates scale relation:
  - tonic/root of current bank center: brightest
  - in-scale roots: bright
  - chromatic/borrowed roots: dim
- **Held chord**: pulse
- **Latched chord**: slower pulse
- **Currently repeated/quantized chord**: white flash on trigger

Suggested row colors:

- Row 0: amber
- Row 1: warm white
- Row 2: green
- Row 3: blue
- Row 4: red
- Row 5: purple
- Row 6: cyan
- Row 7: sky blue

### Controls

- Active side modifiers stay lit.
- Current top-row mode selections use bright colors; inactive states stay dim.
- Panic flashes red/white when used.

## UI Plan

Use the same raw X11/Cairo approach as the current Launchpad-oriented plugins.

The UI should show:

- 8x8 grid mirror
- root names above columns
- row labels at the side
- current scale, register, trigger mode, hold mode
- active modifiers
- current octave and inversion

The UI should also allow mouse clicking on grid pads and buttons so the plugin remains usable without the hardware attached.

## Architecture

Achord should follow the general `arpiso` split, but with a simpler event-driven engine.

### Proposed files

- `lv2/achord/CMakeLists.txt`
- `lv2/achord/README.md`
- `lv2/achord/achord.lv2/achord.ttl`
- `lv2/achord/achord.lv2/manifest.ttl`
- `lv2/achord/include/launchpad_config.h`
- `lv2/achord/include/achord_engine.h`
- `lv2/achord/include/achord_ui_state.h`
- `lv2/achord/include/chord_map.h`
- `lv2/achord/include/voicing_engine.h`
- `lv2/achord/include/midi_comm.h`
- `lv2/achord/src/achord_engine.c`
- `lv2/achord/src/chord_map.c`
- `lv2/achord/src/voicing_engine.c`
- `lv2/achord/src/midi_comm.c`
- `lv2/achord/src/achord_plugin.cpp`
- `lv2/achord/src/ui/achord_ui_x11.c`

### LV2 ports

Keep the same basic port pattern as `arpiso`:

- `control_in` atom sequence for Launchpad MIDI and optional `time:Position`
- `midi_out` atom sequence for generated chord notes
- `launchpad_out` atom sequence for Launchpad LED/SysEx output
- optional silent stereo audio outs for host compatibility
- `notify_out` atom sequence for UI state updates

### Internal modules

- **Chord map**: root-bank math, row formulas, scale-aware modifier spelling
- **Voicing engine**: inversions, spread, register doubling, voice leading
- **Engine**: active pad tracking, hold/latch logic, note-off safety, panic
- **MIDI comm**: Launchpad programmer-mode init and LED bulk updates

## State Persistence

Persist:

- bank offset
- base octave
- scale
- register
- trigger mode
- hold mode
- modifier states
- inversion and spread state
- voice-leading state

Do not persist currently sounding notes. Session restore should come back silent but configured.

## Implementation Order

1. Fork the `arpiso` scaffold and strip out the Euclidean/well logic.
2. Implement grid-to-root/row resolution and direct chord note output.
3. Add top-row and side-button state machines.
4. Add voicing engine, inversion logic, and register doubling.
5. Add LED rendering and UI labels.
6. Add transport-aware trigger modes.
7. Test note-off safety, panic behavior, latch edge cases, and Launchpad reconnect behavior.

## Summary

Achord should be a compact, performance-oriented "modern Stradella" for the Launchpad:

- fifths across
- chord families up the rows
- fast modifiers on the side
- global setup on the top

That gives a mapping that is easy to memorize, harmonically useful, and clearly differentiated from `arpiso` while still fitting the same Flues Launchpad plugin family.
