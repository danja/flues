# Chordant Implementation Plan

## Goal
Build `Chordant` as an LV2 stereo audio effect that is transport-synced to host beat timing and uses a Euclidean rhythm pattern to:
- Continuously capture input audio during Euclidean low segments.
- On each Euclidean high segment, mix the captured low-segment buffers, normalize by the number of captured segments, and output the result.
- Produce rhythmic, chord-like layering from a melodic input line.

## Scope
- Follow `lv2/e-mix` plugin architecture and coding style.
- Stereo in/out (2 channels only).
- X11/Cairo UI with controls following the `e-mix` pattern.
- LV2 State persistence for all user parameters.

## Proposed Control Set
- `Total Bars` (cycle length in bars)
- `Division` (Euclidean step count per cycle)
- `Steps` (Euclidean active/high count)
- `Offset` (rotation)
- `Fade` (crossfade in bars for smoother transitions)
- `Max Capture Segments` (cap for buffered low segments to bound memory/CPU)
- `No Capture Pass-Through` (checkbox, default ON; if OFF outputs silence when nothing captured)
- `Clear After Trigger` (checkbox, default ON)
- `Capture Length Mode` (discrete selector, default `1 Step`)
  - `1 Step` (your requested default: each captured segment is one Euclidean beat/step)
  - `2 Steps` (captures short motifs across adjacent steps)
  - `4 Steps` (captures phrase fragments)
  - `1 Bar` (captures a full-bar phrase for chordal stabs)

## Architecture
- Plugin path: `lv2/chordant/`
- DSP: `src/chordant_plugin.cpp`
- UI: `src/ui/chordant_ui_x11.c`
- Metadata: `chordant.lv2/chordant.ttl`, `chordant.lv2/manifest.ttl`
- Build: `CMakeLists.txt` mirroring `lv2/e-mix`

## DSP Design
1. Time Sync Layer
- Parse `time:Position` atom events (`bar`, `barBeat`, `beatsPerBar`, `beatsPerMinute`, `speed`).
- If host transport data is missing or stopped, pass-through audio and do not update capture/mix state.

2. Euclidean Scheduler
- Reuse deterministic Euclidean hit test logic from `e-mix` style.
- Convert bar position -> cycle phase -> division index.
- Detect step transitions sample-accurately within each run block.

3. Capture Engine (low segments)
- During low steps, append incoming stereo samples to current segment buffer.
- Default segment length is exactly one Euclidean step (`Capture Length Mode = 1 Step`).
- Optional longer capture lengths (`2 Steps`, `4 Steps`, `1 Bar`) are supported via control.
- Store completed low segments in a ring/list up to `Max Capture Segments`.

4. Mix Engine (high steps)
- At entry to a high step, freeze capture list for the trigger.
- For each output sample in high step: sum aligned samples from all captured segments.
- Normalize by active segment count (`1 / N`) and optional soft limit to avoid clipping.
- If no captured segments exist, behavior is controlled by `No Capture Pass-Through`:
  - ON (default): pass input through
  - OFF: output silence
- After a high-step trigger, clear or retain captured segments based on `Clear After Trigger`:
  - ON (default): clear capture store
  - OFF: retain capture store for repeated reuse

5. Transition Handling
- Apply optional fade at low->high and high->low boundaries to reduce clicks.
- Fade unit is bars and converted to samples via host tempo.

6. State Persistence
- Persist all control values through LV2 State extension (`save`/`restore`) with POD float values.

## UI Plan
- Single-row (or two-row if needed) rotary controls plus numeric fields.
- Add two toggles (`No Capture Pass-Through`, `Clear After Trigger`) and one discrete selector (`Capture Length Mode`).
- Reuse interaction model from existing X11/Cairo UIs:
  - drag knob to change
  - direct numeric text entry
  - host->UI value updates via `port_event`
- Keep label/port naming consistent with TTL symbols.

## Validation Plan
1. Build and load
- `cmake -S lv2/chordant -B lv2/chordant/build`
- `cmake --build lv2/chordant/build`
- `cmake --install lv2/chordant/build --prefix ~/.lv2`

2. Functional checks in host
- Confirm beat-synced switching and deterministic Euclidean pattern.
- Verify low-segment capture accumulates and high-segment mix triggers correctly.
- Verify normalization tracks captured segment count.
- Verify behavior on transport stop/start and loop boundaries.

3. Audio quality checks
- Listen for clicks at transitions; tune fade behavior.
- Check clipping behavior under dense capture conditions.

4. Persistence checks
- Save DAW session and reopen; verify parameters restored.

## Implementation Steps
- [x] Step 1: Scaffold plugin folder and build files (`CMakeLists.txt`, TTL, manifest).
- [x] Step 2: Implement core LV2 plugin skeleton (ports, instantiate, connect, run, cleanup).
- [x] Step 3: Implement host time parsing and Euclidean step scheduling.
- [x] Step 4: Implement low-segment capture buffers and bounded segment store.
- [x] Step 5: Implement high-step normalized mix playback plus `No Capture Pass-Through` and `Clear After Trigger`.
- [x] Step 6: Add fade/crossfade transition processing.
- [x] Step 7: Add LV2 State save/restore for all controls.
- [x] Step 8: Build X11/Cairo UI with knobs + numeric entry + toggles + capture-length selector.
- [x] Step 9: Wire install script (`install-chordant.sh`) and smoke test.
- [ ] Step 10: Manual host verification and final tuning.

## Open Questions
- None. Defaults and options are defined.
