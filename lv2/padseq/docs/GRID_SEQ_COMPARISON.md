# PadSeq Implementation Notes (Legacy Filename)

This file previously compared other grid sequencers. It now documents PadSeq's core behavior so the filename does not mislead new readers.

## Core Behavior

- **Grid mapping:** 8x8 pads map to steps 0-63 (row-major order).
- **Voices:** 8 drum voices, selected via side buttons.
- **Patterns:** Two slots (A/B) with per-voice step storage.
- **Euclid:** Per-voice pulses + offset, regenerated when length changes.
- **Length:** Active columns control sequence length (1-8 columns = 8-64 steps).

## Transport + MIDI

- **Transport sync:** Uses `time:Position` for BPM and play/stop.
- **MIDI output:** Notes emitted on channel 10 with per-step velocity.
- **LED updates:** Launchpad LEDs refreshed via SysEx bulk updates and per-step colors.

## UI

- **X11/Cairo UI** mirrors the Launchpad LED state.
- UI receives state via `notify_out` atom chunks and direct control ports.
