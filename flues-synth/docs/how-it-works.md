# Flues-Synth: How It Works (Overview)

This is a headless, polyphonic synth for Raspberry Pi that fuses three signal families:
Disyn distortion oscillators → Chatterbox formant shaping → PM-Synth style delay/interface/feedback.
It runs as ALSA audio/MIDI with a single executable (`flues-synth`), controlled via MIDI notes/CCs.

## High-Level Signal Flow

```
MIDI (notes + CC) → Voice allocator (4 voices) → Per-voice chain:
    Disyn Oscillator (7 algos, velocity-scaled envelope)
    + Noise/DC sources
    → DC Blocker 1 (source)
    → Envelope (attack/release)
    → Formant Cascade (F1–F4 + optional nasal)
    → Feedback Mixer (Delay1/Delay2/Filter returns) + DC Blocker 2
    → Interface Strategy (12 physical models: reed, pluck, hit, flute, brass, bow, bell, drum, crystal, vapor, quantum, plasma)
    → Dual Delay Lines (pitch-tracked, tunable ratio)
    → State-Variable Filter (LP/BP/HP morph)
    → AM Modulation (LFO depth/freq)
    → Global pad + soft clip
    → Master gain → ALSA out
```

## Key Blocks and Algorithms

- **Disyn Oscillator (C++ wrapper)**: Seven distortion-style algorithms. Frequency comes from MIDI note; user parameters: Algorithm (select), Param1, Param2, Level (mix).
  - **Dirichlet Pulse**: Bandlimited impulse train with adjustable harmonic count (Param1) and phase skew (Param2) to control brightness/edge.
  - **DSF Single**: Discrete Summation Formula (Yeh/Chowning) generating a harmonic series with geometric amplitude decay; Param1 = index/decay, Param2 = inharmonicity skew.
  - **DSF Double**: Two mirrored DSF spectra summed for richer even/odd balance; Param1 drives overall index, Param2 sets the inharmonic offset between the two lobes.
  - **Tanh Square**: Sawtooth passed through `tanh` waveshaping; Param1 = pre-shape drive (harmonic density), Param2 = bias/asymmetry for more odd harmonics.
  - **Tanh Saw**: Similar to Tanh Square but preserves more saw content; Param1 = drive, Param2 = bias tilt; yields buzzy but controlled spectra.
  - **PAF (Phase-Aligned Formant-like)**: Dual-sideband structure around a carrier; Param1 = formant offset (sideband spacing), Param2 = sideband balance, useful for vowel-ish timbres before formants.
  - **Modified FM**: FM with constrained index to keep the spectrum stable; Param1 = modulation index, Param2 = modulator ratio; voiced, metallic tones without runaway sidebands.
- **Sources (Noise/DC/Tone)**: White noise and DC source (DC usually 0). Tone is unused in the current flow; noise drives formants/PM paths.
- **Envelope (AR)**: Attack/Release envelope; gate-driven; velocity scales excitation.
- **Formant Bank (Chatterbox)**: Four bandpass biquads (F1–F4) plus optional nasal. Exponential mapping for frequencies; makeup gain ~2×; shout/nasal toggles.
- **Interface Strategies (PM-Synth)**: Strategy pattern implements nonlinear “excitation/pipe” behavior:
  - Reed (nonlinear flow), Pluck/Hit (struck/plectrum envelopes), Flute/Brass/Bow (pressure/jet behaviors), Bell/Drum (resonant hits), Crystal/Vapor/Quantum/Plasma (hypothetical timbres).
  - Intensity sets nonlinearity/drive; gate follows note on/off.
- **Dual Delay Lines**: Delay lengths track note frequency; ratio control sets second line length. Outputs feed feedback mixer; tuned comb/echo behavior.
- **Feedback Mixer**: Independent returns for Delay1, Delay2, Filter. Soft-limited and DC-blocked to prevent runaway/offset.
- **Filter (SVF)**: State-variable filter with morph between low/band/high; exponential freq mapping; Q control.
- **Modulation (LFO)**: Bipolar AM↔FM depth; frequency 0.1–20 Hz.
- **DC Blockers**: Two blockers (after Disyn, on feedback path) to prevent DC accumulation; R≈0.999.
- **Output Safety**: Global pad + soft clip before master gain; calibrated to keep peak ~0.28.

## MIDI and Programs

- **CC Mapping**: Nine hardware sliders expected on CCs `73, 72, 28, 30, 74, 71, 1, 27, 7`; per-program maps in `src/midi_mapping.c`.
- **Programs (0–7)**: Preconfigured chains for common use (e.g., Disyn Echo, Physical Model, Formant Voice, Hybrid Speech, Disyn Direct).
- **Control Notes (optional)**: 36–42 can toggle blocks/mute/reset when `FLUES_CONTROL_NOTES=1`.
- **Autostart**: See `docs/startup.md` for a systemd user service.

## Safety and Stability

- **Polyphony**: 4 voices by default (configurable at build); energy-preserving mix (1/√N) to avoid clipping when voices stack.
- **Watchdogs**: Optional note timeout/fixed duration via env vars (`FLUES_NOTE_TIMEOUT_MS`, `FLUES_FIXED_NOTE_MS`).
- **Strategy Caching**: Interface strategies are cached per type to avoid freeing while audio runs, preventing race-triggered crashes when changing interface types live.

## Build/Run (Pi)

```bash
cd flues-synth
meson setup builddir
meson compile -C builddir
./builddir/flues-synth      # auto-picks ALSA device or pass hw:x,y
```

## Where to Look in Code

- `src/main.c` – MIDI handling, program setup, parameter application.
- `src/midi_mapping.c` – Slider → parameter maps per program.
- `src/synth_engine.c` – Voice processing, signal flow glue.
- `src/audio/modules/` – DSP blocks (delay, feedback, filter, modulation, envelope, interface strategies).
- `src/audio/modules/disyn_wrapper.*` – Disyn oscillator bridge.
- `docs/startup.md` – Autostart instructions on Raspberry Pi.
