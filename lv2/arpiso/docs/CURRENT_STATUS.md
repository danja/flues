# ArpIso Plugin - Current Status

**Plugin:** ArpIso (Novation Launchpad Mini MK3 drum sequencer)
**Status:** Functional core sequencer with Launchpad + UI mirroring

## What Works ✅

- **Launchpad programmer mode** initialization via SysEx
- **64-step grid** editing for the selected voice
- **8 drum voices** with per-voice patterns
- **Euclidean pulses/offset** per voice
- **Pattern A/B** save + recall
- **Active column length** control (1-8 columns)
- **Host transport sync** via `time:Position` (tempo + play/stop)
- **MIDI output** (channel 10) for drum instruments
- **X11/Cairo UI** mirroring grid/side/top LEDs

## Known Limitations ⚠️

- **Audio outputs are silent** (MIDI-only plugin)
- **Swing** is not implemented (stored but not applied)
- **No per-voice mute/solo** controls yet

## Next Ideas

- Add per-voice mute/solo and velocity scaling
- Optional internal click track for hosts without transport
- Preset save/restore for patterns
