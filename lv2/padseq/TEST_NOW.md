# PadSeq Quick Test Commands

## Verify Install

```bash
lv2ls | grep padseq
lv2info https://danja.github.io/flues/plugins/padseq
```

## Test in Reaper

1. Insert **PadSeq** on a MIDI track.
2. Routing Matrix:
   - Launchpad DAW → PadSeq **Control In**
   - PadSeq **Launchpad Control** → Launchpad DAW
   - PadSeq **MIDI Out** → drum instrument
3. Start transport and press pads.

Expected:
- Grid LEDs respond to pad presses.
- Playhead moves when transport runs.
- Drum instrument receives notes.

## Quick MIDI Monitor

```bash
aseqdump -p "Launchpad Mini MK3 DAW"
```

Press pads and confirm Note On/Off messages.
