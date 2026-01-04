# grid-seq

Grid-based MIDI step sequencer LV2 plugin with full MIDI range support and Novation Launchpad Mini Mk3 integration.

## Quick Start

### GUI Only
1. Load the plugin in your DAW.
2. Route **MIDI Out** to an instrument track.
3. Open the UI and click cells to create a pattern.
4. Press play - the sequencer follows host transport.

### With Launchpad Mini Mk3 (Reaper)
1. Connect the Launchpad via USB.
2. Route **Launchpad MIDI output** → **Track input** (pad control).
3. Route **Launchpad Control** → **Launchpad MIDI input** (LEDs).
4. Route **MIDI Out** → **Instrument track** (notes).

## Controls

### Launchpad Grid (8x8)
- Tap pads to toggle steps in the current 8-note window.
- **Green** = playhead step (empty).
- **Yellow** = active step.
- **Red** = playhead over an active step.
- **Off** = inactive step.

### Launchpad Right Side (CC 19–89) — Scale Select
Bottom → top:
1) Diatonic  
2) Chromatic  
3) Blues  
4) Minor Pentatonic  
5) Dorian  
6) Phrygian  
7) Harmonic Minor  
8) Hijaz (Arabic)

Selected scale lights white. Scales affect **MIDI output only**; pad rows remain chromatic.

### Launchpad Top Row (CC 91–99)
- **CC 93**: Page left (steps 0–7)
- **CC 94**: Page right (steps 8–15, when length > 8)
- **CC 91**: Pitch down (semitone)
- **CC 92**: Pitch up (semitone)

Arrow LEDs light white when available.

### UI Controls
Grid:
- Click cells to toggle steps.

Right button panel:
```
[S] Settings  Sequence length + MIDI filter
[R] Reset     Re-initialize Launchpad connection
[?] Query     MIDI device inquiry
[C] Clear     Clear pattern
[H] Home      Reset pitch to C2 (MIDI 36)
[+] Up        Pitch +1 semitone
[-] Down      Pitch -1 semitone
```

Settings dialog:
- **Sequence length**: 2–16 steps
- **MIDI filter**: Note-On only (no Note-Offs)

## Sequencing Notes

- **Sequence length**: 2–16 steps (default 8).
- **Pitch offset** shifts the visible 8-row window across the 0–127 MIDI range.
- **Scale selection** remaps output notes, not the grid layout.

## Ports
- **MIDI In** (Atom): Launchpad input + UI button events
- **MIDI Out** (Atom): Note output to instruments
- **Launchpad Control** (Atom): LED commands to Launchpad
- **Grid X/Y** (Control): UI click coordinates
- **Current Step** (Control): Playhead position
- **Grid Row 0–15** (Control): Bit-packed pattern state
- **Sequence Length** (Control): Active steps
- **MIDI Filter** (Control): Note-On only mode

## Requirements

Runtime:
- LV2 host (tested with Reaper on Ubuntu x64)
- Cairo
- X11
- Optional Launchpad Mini Mk3

Build:
- Meson, Ninja, C99 compiler, pkg-config
- lv2, cairo, x11 headers

## Building

```bash
meson setup lv2/grid-seq/builddir lv2/grid-seq
meson compile -C lv2/grid-seq/builddir
```

## Installation

```bash
./install-grid-seq.sh
lv2ls | grep grid-seq
```

## Troubleshooting

### No LEDs
- Ensure **Launchpad Control** is routed to the Launchpad MIDI input.
- Use **[R] Reset** in the UI to re-enter Programmer Mode.

### No notes
- Route **MIDI Out** to your instrument.
- Verify transport is running.
- Confirm active steps exist in the grid.

### Launchpad not responding
- Verify Launchpad MIDI is routed to the track input.
- Use **[R] Reset** if it stopped responding.

## License

ISC License

```
Copyright (C) 2025 Danny

Permission to use, copy, modify, and/or distribute this software
for any purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE.
```
