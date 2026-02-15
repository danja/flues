# ArpIso Quick Test Commands

## Verify Install

```bash
lv2ls | grep arpiso
lv2info https://danja.github.io/flues/plugins/arpiso
```

## Test in Reaper

1. Insert **ArpIso** on a MIDI track.
2. Routing Matrix:
   - Launchpad DAW → ArpIso **Control In**
   - ArpIso **Launchpad Control** → Launchpad DAW
   - ArpIso **MIDI Out** → any synth/instrument
3. Start transport and press pads.

Expected:
- Grid LEDs respond to pad presses.
- Playhead moves when transport runs.
- Downstream instrument receives notes.

## Quick MIDI Monitor

```bash
aseqdump -p "Launchpad Mini MK3 DAW"
```

Press pads and confirm Note On/Off messages.
