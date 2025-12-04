# Flues-Synth Handover (Raspberry Pi)

## What’s here
- **Project:** `flues-synth/` – headless C/C++ synth combining Disyn oscillators, Chatterbox formants, and PM-Synth interface/pipe/filter chain.
- **Phase:** 1 (single voice, no reverb yet; polyphony planned later).
- **Audio:** ALSA direct (48 kHz, default 256 frames). Auto device fallback prefers Pi headphones (`hw:Headphones`, `plughw:Headphones`, `hw:2,0`, `plughw:2,0`, `hw:1,0`, `default`). CLI override still works (e.g., `hw:2,0`).
- **MIDI:** ALSA sequencer auto-connect to best external port; CC map baked in (29 CCs). Debug logging available.
- **DSP defaults:** Master gain 0.35, Disyn level 0.5, noise 0.02, DC 0, feedback 0.2/0.2/0.1, tuning 0 semitones, ratio 1.0, filter 2 kHz lowpass, envelope ~10 ms attack / ~100 ms release, neutral formants (F1 500 / F2 1500 / F3 2500 / F4 3500), Reed interface.

## Build steps on Pi
```bash
cd flues-synth
meson setup builddir --reconfigure
meson compile -C builddir
meson test -C builddir engine-smoke   # ensures default path produces non-zero audio
```

## Run
```bash
# Auto device selection
./builddir/flues-synth

# Force Pi headphones (works on your setup)
./builddir/flues-synth hw:2,0

# Debug MIDI logging (shows ch/cc/value + ALSA src client:port)
FLUES_MIDI_DEBUG=1 ./builddir/flues-synth hw:2,0
```

## MIDI CC map (29 controls)
- Standard: `1=intensity`, `7=master gain`, `10=F2`.
- Formants: `71=F1`, `72=release`, `73=attack`, `74=F3`, `75=F4`.
- Vocal modes (≥64=ON): `80=nasal`, `81=sing`, `82=shout`, `83=fry`.
- Disyn source: `16=algorithm`, `17=param1`, `18=param2`, `19=disyn level`, `20=noise`, `21=dc`.
- Interface & delay: `24=interface type`, `26=tuning (-12..+12 semitones)`, `27=ratio (0.5..2.0 absolute)`.
- Feedback: `28=delay1`, `29=delay2`, `30=filter return`.
- Filter: `32=freq`, `33=Q`, `34=shape (0=LP, 0.5=BP, 1=HP)`.
- Modulation: `36=LFO freq`, `37=AM↔FM depth`.

## Recent fixes / status
- **Audio stability:** Fixed delay tuning/ratio mapping (was double-mapped and could explode). Defaults reduced to avoid runaway feedback.
- **Feedback DC blocking:** DC blocker now applied once on the combined feedback mix.
- **Feedback taming:** Combined delay/filter feedback now soft-clipped before feeding the interface to quell runaway spikes.
- **Global pad:** Final signal is scaled by 0.5 before master gain to reduce accumulated level (helps broadband noise).
- **MIDI logging:** Channel-aware CC logging; `FLUES_MIDI_DEBUG=1` adds per-event diagnostics.
- **Smoke test:** `engine-smoke` renders a buffer after note-on and fails if RMS < 1e-4.
- **Defaults tuned for Pi headphones:** `./builddir/flues-synth hw:2,0` produces a clean tone + formants; noise defaults low.
- **Control notes:** Note-ons on 36-41 toggle parts of the chain for debugging hiss (Note On with vel>0 flips state): 36 noise, 37 Disyn, 38 feedback, 39 formants (bypass), 40 filter (bypass), 41 hard mute. Disabling feedback clears delay buffers/DC blocker; disabling filter resets its state; hard mute clears feedback/filter state too.

## Known gaps / next steps
- Polyphony and reverb not implemented yet (Phase 1 single voice).
- Fry mode stub (TODO: add f0/2 in Disyn).
- No GTK/UI; headless only.
- If CCs from the MK-449C don’t match expectations, capture with `aseqdump -p <port>` and we can add a remap layer.
- MK-449 mapping: defaults now remap 91→Delay1 FB (28), 92→Delay2 FB (29), 93→Filter FB (30), 84→Delay Ratio (27), 12→Noise Level (20), 13→Disyn Level (19), 5→Intensity (1). Disable with `FLUES_MK449_MAP=0`.

## Troubleshooting quicklist
- No audio: force device `./builddir/flues-synth hw:2,0`; verify `aplay -Dhw:2 /usr/share/sounds/alsa/Front_Center.wav`.
- XRUNs/lockups: ensure memlock limits (`ulimit -l` → unlimited); bump buffer `DEFAULT_BUFFER_SIZE` in `include/config.h` and rebuild if needed.
- MIDI mapping: `FLUES_MIDI_DEBUG=1 ./builddir/flues-synth hw:2,0` then move sliders; watch CC/channel printouts.
