# Flues-Synth Handover (Raspberry Pi / UI notes)

## What’s here (engine)
- **Project:** `flues-synth/` – headless C/C++ synth combining Disyn oscillators, Chatterbox formants, and PM-Synth interface/pipe/filter chain.
- **Polyphony:** 4 voices by default (set at configure via `-Dmax_voices=`). Oldest-note stealing; duplicate notes allowed; velocity scales envelope.
- **No reverb:** Setters are stubs; hide/gray in UI.
- **Audio:** ALSA direct (32 kHz default, 512 frames). Auto device fallback prefers Pi headphones (`hw:Headphones`, `plughw:Headphones`, `hw:2,0`, `plughw:2,0`, `hw:1,0`, `default`). CLI override: `./builddir/flues-synth hw:2,0`.
- **MIDI:** ALSA sequencer auto-connect to best external port; CC map is fixed (29 controls). Control notes for debugging default **OFF**.
- **DSP defaults:** master 0.8, Disyn level 0.8, noise 0.15, DC 0, feedback 0.2/0.2/0.1, tuning 0, ratio 1.0, filter 2 kHz LP, envelope exp-mapped with default ~10 ms attack / ~50 ms release, neutral formants (F1 500 / F2 1500 / F3 2500 / F4 3500), Reed interface. **Release capped to 1.0 s** to avoid runaway tails from host max-release.
- **Feedback safety:** Delay1/2 feedback clamped ≤ 0.75; filter feedback ≤ 0.5; soft saturation on mix to avoid latch-up.
- **Mix:** Energy-preserving sum (1/sqrt(active voices)); dual DC blockers per voice (Disyn output + feedback path).

## Build/run quickstart (Pi)
```bash
cd flues-synth
meson setup builddir --reconfigure    # -Dmax_voices=4 (default)
meson compile -C builddir
meson test -C builddir                # engine-smoke, envelope-test, polyphony-smoke, disyn-levels, noise-isolation

# Run (auto device)
./builddir/flues-synth

# Force device (example Pi headphones)
./builddir/flues-synth hw:2,0

# MIDI debug logging
FLUES_MIDI_DEBUG=1 ./builddir/flues-synth hw:2,0
```

## Environment toggles
- `FLUES_CONTROL_NOTES=1` – enable debug control notes 36-42 (default OFF to avoid stealing low notes from DAWs). 36 noise, 37 Disyn, 38 feedback, 39 formants bypass, 40 filter bypass, 41 hard mute, 42 reset.
- `FLUES_MK449_MAP=0` – disable MK-449 remap (defaults: 91→28, 92→29, 93→30, 84→27, 12→20, 13→19, 5→1).
- `FLUES_MIDI_DEBUG=1` – verbose ALSA MIDI logging (note on/off/CC/All Notes Off with source client:port).

## MIDI CC map (29 controls)
- Standard: `1=intensity`, `7=master`, `10=F2`.
- Formants: `71=F1`, `72=release`, `73=attack`, `74=F3`, `75=F4`.
- Vocal modes (≥64=ON): `80=nasal`, `81=sing`, `82=shout`, `83=fry` (fry still stubbed in Disyn).
- Disyn: `16=alg`, `17=param1`, `18=param2`, `19=level`, `20=noise`, `21=dc`.
- Interface & delay: `24=type`, `26=tuning (-12..+12 semitones)`, `27=ratio (0.5..2.0 abs)`.
- Feedback: `28=delay1`, `29=delay2`, `30=filter return` (all clamped to safe ranges).
- Filter: `32=freq`, `33=Q`, `34=shape (0=LP, 0.5=BP, 1=HP)`.
- Modulation: `36=LFO freq`, `37=AM↔FM depth`.

## UI-facing notes
- **Hide reverb**: stubs only.
- **Respect limits**: feedback clamped; release capped to 1.0 s; show ranges in UI to prevent latch-up.
- **Polyphony info**: expose active voice count (API: `synth_engine_get_active_voice_count`). Voice stealing is oldest-note.
- **Device selection**: CLI arg selects ALSA device; UI can offer dropdown of `aplay -L` or manual entry.
- **Debug aids**: toggle control notes (opt-in), MIDI debug flag for field diagnostics.
- **Tests**: run `meson test -C builddir` before shipping UI builds.
- **Known issue (field)**: On Pi via Reaper routing, occasional long holds reported even with release cap. Next steps: run with `FLUES_MIDI_DEBUG=1` to confirm note-offs/All Notes Off; if present, further reduce release cap or add voice/steal logging in the engine.

## Troubleshooting
- No audio: force device `./builddir/flues-synth hw:2,0`; verify `aplay -Dhw:2 /usr/share/sounds/alsa/Front_Center.wav`.
- MIDI mapping: `FLUES_MIDI_DEBUG=1 ./builddir/flues-synth hw:2,0` then move sliders; watch CC/channel printouts.
- Long holds: confirm incoming note-offs; if they arrive, tighten release mapping or log voice state.
