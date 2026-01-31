# P-Mix Implementation Plan

## Goals
- LV2 audio effect that applies probabilistic gain changes on bar boundaries.
- Five user parameters with persisted values: Granularity, Maintain, Fade, Cut, Fade Duration Max.
- Works on any channel count for the track.
- UI mirrors `lv2/padseq` patterns (X11/Cairo knobs + edit boxes).

## Assumptions (confirmed)
- Uses LV2 Time/Position to track bar count and transport state.
- Gain is applied uniformly to all channels.
- “Maintain/Fade/Cut” probabilities are interpreted as percentages and normalized if they do not sum to 100.
- Strictly an audio effect (no MIDI).
- Fade duration is a fraction of granularity (0.125 to 1.0), selected uniformly.
- Fade Duration knob sets the upper bound of the uniform range; min is fixed at 0.125.

## Open Questions
- None.

## Step-by-step Plan
1. Review `lv2/padseq` for LV2 + UI patterns and state persistence.
2. Define LV2 ports and parameters:
   - Audio in/out (support up to N channels, optional extra ports; process all connected).
     Proposed: N=8, ports `in_1..in_8`, `out_1..out_8`, mark optional in TTL.
   - Control ports: Granularity, Maintain, Fade, Cut, Fade Duration
   - State persistence for the five values
   - Implement LV2 State extension (`LV2_State_Interface`) to persist parameters.
   - Store each parameter as a state property keyed by a URI; use atom types (Float) and
     set `LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE`.
   - Wire `extension_data` to return the `LV2_State_Interface` when asked for
     `LV2_STATE__interface`, implement `save`/`restore` with store/retrieve callbacks.
3. Implement transport/bar tracking:
   - Read LV2 Time/Position
   - Detect bar boundaries and apply granularity gating
   - Handle missing/invalid time info gracefully
4. Implement probabilistic transition engine:
   - Keep current gain state (0 or 1; ramp when fading)
   - At each eligible boundary, choose Maintain/Fade/Cut per probabilities
   - Enforce valid transitions (fade/cut only if state allows)
   - If gain is between 0 and 1 at a boundary, resolve to nearest state before choosing
5. Implement DSP gain processing:
   - Apply gain per sample to all channels
   - Fade ramp handling with deterministic time base
6. Build UI (match `lv2/padseq` pattern):
   - Five knobs + edit boxes, same look/feel
   - Value display, editing, and bounds
7. Persist state:
   - Store/restore parameters between sessions
8. Validate behavior:
   - Manual testing in a host with transport
   - Check edge cases (no time info, looping, tempo change)

## Status
- [x] Step 1: Review `lv2/padseq`
- [x] Step 2: Define ports and state
- [x] Step 3: Transport/bar tracking
- [x] Step 4: Probability engine
- [x] Step 5: Gain processing
- [x] Step 6: UI
- [x] Step 7: State persistence
- [ ] Step 8: Validation
