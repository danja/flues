# Cadence

Cadence is a transport-synced LV2 MIDI harmonizer for monophonic lines. It listens to one cycle of incoming notes, scores chord candidates per segment, and emits a voiced chord progression on the following cycle.

It is designed for loops such as `lv2/bassgen`, but it will also work with other MIDI bass or melody sources as long as the host provides `time:Position`.

The bundle now includes a small X11/Cairo UI for the core controls, a `Learn` trigger, and a `Ready` indicator.

## Quick Start

1. Put Cadence after a monophonic MIDI source.
2. Match `Key`, `Scale`, and `Cycle Bars` to the source.
3. Start transport and let one full cycle play.
4. Cadence sets `Ready` to `1` once it has a learned progression.
5. Leave `Pass Input` on to layer chords with the source line, or turn it off to hear chords only.

## Controls

- `Key`: tonic pitch class.
- `Scale`: used as a harmonic bias, not a hard quantizer.
- `Cycle Bars`: loop length used for capture and playback.
- `Granularity`: chord decision rate (`Beat`, `Half Bar`, `Bar`).
- `Complexity`: low values prefer plain triads; high values allow more suspended, borrowed, and seventh-like choices.
- `Chord Size`: triads or sevenths.
- `Register`: overall voicing height.
- `Spread`: close, open, or drop-2 voicing.
- `Pass Input`: forwards the source notes alongside the harmony.
- `Output Channel`: MIDI channel used for generated chords.
- `Learn`: bump this value to clear the learned progression and recapture.
- `Ready`: output status, `1` when a learned progression is available.

## How It Works

- Incoming notes are weighted by duration and onset emphasis inside each segment.
- Every segment scores a bank of chord candidates across all roots and common chord qualities.
- A dynamic-programming pass chooses the full progression, preferring good local note fit plus smoother harmonic movement.
- The chosen chords are voiced with inversion search and light voice-leading against the previous chord.

## Build

```bash
cd lv2/cadence
cmake -S . -B build
cmake --build build
cmake --install build --prefix ~/.lv2
```

## Notes

- Cadence currently expects host transport. Without `time:Position`, it falls back to MIDI pass-through and does not learn.
- Non-note MIDI messages always pass through unchanged.
- Learned chord progressions are persisted via LV2 state.
