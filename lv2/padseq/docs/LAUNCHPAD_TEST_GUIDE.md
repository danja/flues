# PadSeq Launchpad MIDI Routing Test Guide

## Goal

Verify that Launchpad MIDI reaches PadSeq, LEDs update, and MIDI notes are emitted.

## Test 1: Verify Launchpad Hardware

```bash
aconnect -l
```

Expected:
- `Launchpad Mini MK3 DAW` port should be listed.

## Test 2: Quick MIDI Monitor

```bash
aseqdump -p "Launchpad Mini MK3 DAW"
```

Press pads and confirm Note On/Off messages appear.

## Test 3: PadSeq in jalv

```bash
jalv https://danja.github.io/flues/plugins/padseq
```

Check console for:
- `padseq: Initializing Launchpad...`
- `padseq: Received Note On - note=XX`

## Test 4: PadSeq in Reaper

1. Insert **PadSeq** on a MIDI track.
2. Routing Matrix:
   - Launchpad DAW → PadSeq **Control In**
   - PadSeq **Launchpad Control** → Launchpad DAW
   - PadSeq **MIDI Out** → drum instrument
3. Start transport and press pads.

### Expected Results

- Launchpad grid lights show step states.
- Pad presses toggle steps (yellow on/off).
- Playhead moves (green) when transport runs.
- Drum instrument receives notes (channel 10).

## If LEDs Stay Dark

- Confirm Launchpad DAW routing to PadSeq control ports.
- Ensure the Launchpad is in DAW mode.
- Try reloading the plugin to resend Programmer Mode SysEx.

## If No Notes Are Heard

- Verify PadSeq MIDI Out is routed to a drum instrument.
- Confirm the host transport is running.
- Use a MIDI monitor on PadSeq MIDI Out to confirm note output.
