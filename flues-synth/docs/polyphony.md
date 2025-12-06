# Flues-Synth Polyphony Notes

## Status
- Engine is already 4-voice polyphonic. `MAX_VOICES` defaults to 4 via `include/config.h` / Meson option.
- Oldest-note voice stealing is in place (free voice first, then steal the lowest `note_on_age`).
- Velocity is applied (`velocity / 127`) to the envelope gain in `voice_process_sample()`.
- All parameter setters broadcast to every voice; Disyn/Noise/Delay/Filter/Formant settings stay in sync across the pool.
- Energy-preserving mix is used: active voices are summed, then scaled by `1 / sqrt(active_count)` to avoid clipping when stacking chords.
- Polyphony limit is tunable at build time: `meson setup builddir -Dmax_voices=2` updates `MAX_VOICES` for constrained targets.

## Implementation Snapshot (2025-12)
- **Voice model** (`src/synth_engine.c`): Each `Voice` carries `active`, `midi_note`, `note_on_age`, `velocity`, per-voice DSP modules, dual DC blockers, and vibrato state.
- **Voice pool** (`SynthEngine`): `voices[MAX_VOICES]` plus `global_note_counter` for age tracking. Defaults (Disyn alg 0, noise 0.15, neutral formants, reed interface, etc.) are applied to every voice during `synth_engine_create()`.
- **Allocation**: `find_free_voice()` → `find_oldest_voice()`. Duplicate notes are allowed; `note_off` releases all voices whose `midi_note` matches the event.
- **Processing**: `synth_engine_process()` iterates all voices per sample, accumulates outputs, applies the sqrt mix scaling, then master gain. DC blockers live per voice (Disyn output and feedback path).
- **MIDI path** (`src/main.c` → `midi_backend_alsa.c`): Note-on events forward velocity to `synth_engine_note_on()`. Note-on with velocity 0 is treated as note-off in the ALSA backend.

## Known Cleanup / TODO
- Update user-facing docs that still claim “Phase 1: single voice” (`flues-synth/README.md`, `flues-synth/docs/flues-synth-handover.md`, comment headers in `include/synth_engine.h`) so messaging matches the polyphonic reality.
- Reverb setters remain no-ops; either wire them to a real block or mark them intentionally unimplemented in the UI/README.
- Fry mode is still marked TODO inside `synth_engine_set_fry()`; decide whether to implement the f₀/2 subharmonic in Disyn or document it as deferred.
- Extend polyphony-focused tests beyond the current `polyphony-smoke` (e.g., per-voice release timing assertions, repeated steal churn) alongside the single-voice suites.
- Consider optional debug logging for voice steals to aid regression tracing on the Pi build.

## Verification Checklist
- `meson test -C builddir` (existing suites: engine-smoke, envelope-test, polyphony-smoke, disyn-levels, noise-isolation).
- Manual MIDI check: play 2–4 note chords, confirm even gain due to sqrt mix; trigger a 5th note to confirm oldest voice is stolen without zipper noise; compare soft vs hard velocity for loudness change; send CCs during a chord to verify parameter broadcast.

## 2025-12-06 Status (Pi + Reaper troubleshooting)
- Reverb cancelled/stubbed; envelope max release capped to 1.0s to shorten tails when hosts send max release.
- Feedback clamp added: delay1/2 ≤ 0.75, filter feedback ≤ 0.5 to reduce latch-up from high resonance CCs.
- Control notes 36–42 (debug toggles) **disabled by default** to avoid consuming low notes from DAW; enable via `FLUES_CONTROL_NOTES=1`.
- MIDI debug logging added via `FLUES_MIDI_DEBUG=1` (note/CC/All Notes Off with ALSA source port) to verify Reaper → Pi delivery.
- Tests: full `meson test -C builddir` passing locally (engine-smoke, envelope-test, polyphony-smoke, disyn-levels, noise-isolation).
- Issue still open: on Pi, notes often hold excessively long (inconsistent), even with release cap. Need to confirm whether missing note-offs/All Notes Off are being received. Next steps:
  - Run synth with `FLUES_MIDI_DEBUG=1` to capture incoming note on/off from Reaper path.
  - If note-offs are present but release still long, consider further reducing MAX_RELEASE or making CC72 mapping sub-linear.
  - Add optional voice-steal/active-voice logging to catch stuck envelopes.
