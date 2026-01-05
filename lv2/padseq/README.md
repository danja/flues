# PadSeq

**64-step drum sequencer LV2 plugin for Novation Launchpad Mini MK3**

PadSeq turns the full 8x8 Launchpad grid into a 64-step drum sequencer with 8 voices, per-step velocity, Euclidean pattern generation, and two pattern slots (A/B). It follows the host transport and outputs MIDI notes on channel 10 for routing into any drum instrument.

## Features

- **64-step grid**: every pad is a step (rows 0-7, cols 0-7)
- **8 drum voices**: selectable via side buttons, each with its own pattern
- **Velocity steps**: step velocity uses incoming pad velocity
- **Euclidean sequencing**: pulses + offset per voice
- **Pattern A/B**: instant switching with preserved state
- **Active columns**: shrink/expand sequence length (1-8 columns)
- **Launchpad LED feedback**: playhead + active steps + mode buttons
- **X11/Cairo UI**: mirrors Launchpad state and allows mouse interaction

## Building

```bash
cmake -S lv2/padseq -B lv2/padseq/build
cmake --build lv2/padseq/build
cmake --install lv2/padseq/build --prefix ~/.lv2
```

Verify:
```bash
lv2ls | grep padseq
lv2info https://danja.github.io/flues/plugins/padseq
```

## Usage

### DAW Routing (Reaper Example)

1. Insert **PadSeq** on a MIDI track.
2. Route Launchpad Mini MK3 **DAW** port:
   - Launchpad DAW Out → PadSeq **Control In**
   - PadSeq **Launchpad Control** → Launchpad DAW In
3. Route PadSeq **MIDI Out** to your drum instrument.
4. Start the host transport; PadSeq follows tempo and play/stop.

### Controls

**Top row (CC 91-99):**
- 91: Euclid pulses up (current voice)
- 92: Euclid offset up (current voice)
- 93: Active columns down (1-8)
- 94: Active columns up (1-8)
- 95: Pattern A
- 96: Pattern B
- 97: Clear current voice + Euclid
- 98: Clear pattern + Euclid
- 99 (logo): Clear pattern + Euclid

**Side buttons (CC 19-89):**
- Select drum voice 0-7 (top to bottom)
- Default MIDI notes per voice: 36, 40, 39, 50, 42, 46, 53, 51

**Grid (8x8):**
- Tap to toggle steps for the selected voice
- Yellow = active step, Gray = inactive columns, Green = playhead

## Architecture

```
lv2/padseq/
├── include/                # Launchpad + engine headers
├── src/                    # Engine + LV2 wrapper + X11/Cairo UI
├── padseq.lv2/             # LV2 metadata
├── docs/                   # User manual + test guides
└── README.md
```

## Notes

- PadSeq outputs **MIDI only** (audio outputs are silent).
- Transport sync uses `time:Position` from the host.
- If the Launchpad is not routed to the control ports, LEDs will stay dark.
