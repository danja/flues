# grid-seq

Grid-based MIDI step sequencer LV2 plugin with full MIDI range support and Novation Launchpad Mini Mk3 hardware integration.

## Features

### Sequencing
- **16-step sequencer** with adjustable length (1-16 steps)
- **Full MIDI range** (128 notes, 0-127) with 8-note visible window
- **Pitch shifting** - shift the visible window up/down across entire MIDI range
- **50% gate length** for punchy, rhythmic patterns
- **Host transport sync** - follows DAW tempo and play/stop

### Control Interfaces
- **X11/Cairo GUI** - visual grid editor with pattern controls
- **Novation Launchpad Mini Mk3** - hardware grid controller with LED feedback
  - 8x8 grid for pattern editing
  - Real-time LED updates showing current step and active notes
  - Hardware page switching (view steps 0-7 or 8-15)
  - Pitch shift buttons with LED indicators

### GUI Controls
- **Grid editing** - click cells to toggle steps
- **Settings dialog** - adjust sequence length (1-16) and MIDI filter
- **Pitch shift buttons** - shift note range up/down
- **Re-center button** - reset pitch to default (C2/MIDI 36)
- **Clear button** - erase current pattern
- **Reset button** - re-initialize Launchpad connection
- **Query button** - MIDI device inquiry

## Requirements

### Runtime
- **LV2 host** (tested with Reaper on Ubuntu x64)
- **Cairo** graphics library
- **X11** window system
- **(Optional) Novation Launchpad Mini Mk3** for hardware control

### Build Tools
- **Meson** >= 0.56.0
- **Ninja** build system
- **GCC** or **Clang** with C99 support
- **pkg-config**

### Development Headers
- lv2 >= 1.18.0
- cairo >= 1.16.0
- x11

## Building

```bash
# Configure build directory
meson setup build

# Compile plugin and UI
meson compile -C build
```

## Installation

### User Installation (Recommended for Development)

Install to your user LV2 directory:

```bash
# Copy plugin binaries to user LV2 directory
cp build/grid_seq.so ~/.lv2/grid-seq.lv2/
cp build/grid_seq_ui.so ~/.lv2/grid-seq.lv2/

# Verify installation
lv2ls | grep grid-seq
```

### System Installation

Requires root privileges:

```bash
sudo meson install -C build
```

This installs to `/usr/local/lib/x86_64-linux-gnu/lv2/grid-seq.lv2/` or equivalent.

## Quick Start

### Basic Setup (GUI Only)

1. **Load plugin** in your DAW (e.g., Reaper)
2. **Route MIDI output** to an instrument/synth track
3. **Open plugin UI** to see the grid editor
4. **Click cells** to create a pattern
5. **Press play** in your DAW - sequencer follows transport

### With Launchpad Mini Mk3

1. **Connect Launchpad** via USB
2. **Load plugin** in your DAW
3. **Configure MIDI routing**:
   - Route **Launchpad MIDI output** → **Track input** (for pad control)
   - Route **Track "Launchpad Control" output** → **Launchpad MIDI input** (for LEDs)
   - Route **Track "MIDI Out"** → **Instrument track** (for notes)
4. **Press pads** on Launchpad to program patterns
5. **Use arrow buttons** for navigation:
   - Left/Right arrows: Switch between steps 0-7 and 8-15
   - Up/Down arrows: Shift pitch range

## Usage Guide

### GUI Interface

#### Main Grid
- **Click cells** to toggle steps on/off
- **Current step** highlighted during playback
- **Yellow** = current step active, **Green** = step active, **Dark gray** = step inactive
- Grid shows 8 notes vertically (visible window into 128-note range)
- Sequence length adjustable from 1-16 steps horizontally

#### Button Panel (Right Side)
```
[S] - Settings    Open settings dialog (sequence length, MIDI filter)
[R] - Reset       Re-initialize Launchpad connection
[?] - Query       Send MIDI device inquiry
[C] - Clear       Erase entire pattern
[H] - Home        Reset pitch to C2 (MIDI note 36)
[+] - Up          Shift pitch range up by 1 semitone
[-] - Down        Shift pitch range down by 1 semitone
```

#### Settings Dialog
- **Sequence Length slider**: Set active steps (1-16)
- **MIDI Filter checkbox**: Enable Note-On only mode (no Note-Off events)

### Launchpad Controls

#### Grid Pads (8x8)
- Press pads to toggle steps
- **Green** = step active
- **Yellow** = current step active
- **Dim green** = current step inactive
- **Off** = step inactive

#### Control Buttons
- **Left arrow (CC 93)**: View steps 0-7 (page 0)
- **Right arrow (CC 94)**: View steps 8-15 (page 1, if sequence > 8)
- **Down arrow (CC 91)**: Shift pitch down 1 semitone
- **Up arrow (CC 92)**: Shift pitch up 1 semitone

#### LED Indicators
- Arrow buttons **light white** when available
- Current page buttons stay lit
- Pitch shift buttons show available range

### Pitch Range Example

Default pitch range: **C2 to G2** (MIDI notes 36-43)

```
Press [+] once  → C#2 to G#2 (MIDI notes 37-44)
Press [+] again → D2 to A2   (MIDI notes 38-45)
Press [-]       → C#2 to G#2 (MIDI notes 37-44)
Press [H]       → C2 to G2   (MIDI notes 36-43) [reset to default]
```

This allows accessing the full MIDI range (0-127) with an 8-note visible window.

### Hardware Page Switching

With 16-step sequences:

```
Page 0 [Left Arrow]:  Shows steps 0-7  on Launchpad
Page 1 [Right Arrow]: Shows steps 8-15 on Launchpad
```

The GUI always shows all active steps (up to 16), while the Launchpad shows 8 at a time.

## Configuration in Reaper

### Track Setup
1. Insert **grid-seq** on a MIDI track
2. Set track input to **Launchpad Mini MK3** (for hardware control)
3. Route track output to **instrument track** (for musical notes)

### Launchpad LED Feedback
Add a hardware output send:
1. Click **Route** button on grid-seq track
2. Under **Sends**, add new send
3. Select **Launchpad Mini MK3** as destination
4. Choose **"Launchpad Control"** as source (not "MIDI Out")

This sends LED commands back to the Launchpad for visual feedback.

## Technical Details

### Architecture
- **Plugin**: `grid_seq.so` - LV2 plugin (MIDI sequencer engine)
- **UI**: `grid_seq_ui.so` - X11/Cairo visual interface
- **Manifest**: `manifest.ttl`, `grid_seq.ttl` - LV2 metadata

### Ports
- **MIDI In** (Atom): Receives MIDI from Launchpad and UI button events
- **MIDI Out** (Atom): Sends note events to instruments
- **Launchpad Control** (Atom): Sends LED commands to Launchpad
- **Grid X/Y** (Control): Cell coordinates from UI
- **Current Step** (Control): Playback position indicator
- **Grid Row 0-15** (Control): Bit-packed pattern state
- **Sequence Length** (Control): Active step count (1-16)
- **MIDI Filter** (Control): Note-On only mode toggle

### State Format
- **Grid**: 16 columns × 128 rows (steps × MIDI notes)
- **Pitch Offset**: Base MIDI note for visible window (0-120)
- **Hardware Page**: Launchpad view (0 or 1)
- **Sequence Length**: Active steps (1-16)

## Development

### Coding Standards
See [CLAUDE.md](CLAUDE.md) for:
- C99 coding conventions
- LV2 plugin patterns
- UI/plugin communication best practices
- Launchpad protocol details
- Build system configuration

### File Structure
```
src/
├── grid_seq.c       Main plugin (LV2 callbacks, MIDI I/O)
├── sequencer.c/h    Sequencer engine (timing, note generation)
├── state.c/h        State management (grid, tempo, playback)
├── launchpad.c/h    Launchpad protocol (MIDI mapping, LEDs)
└── gui_x11.c        X11/Cairo UI implementation

include/grid_seq/
└── common.h         Shared constants

grid-seq.lv2/
├── manifest.ttl     LV2 bundle manifest
└── grid_seq.ttl     Plugin description
```

### Testing
```bash
# Compile and install
meson compile -C build
cp build/*.so ~/.lv2/grid-seq.lv2/

# Verify installation
lv2ls | grep grid-seq

# Test in DAW
# - Load plugin in Reaper
# - Add MIDI monitor on output
# - Verify pattern playback and timing
```

## Troubleshooting

### Plugin doesn't appear in DAW
- Check installation: `lv2ls | grep grid-seq`
- Verify files exist in `~/.lv2/grid-seq.lv2/`
- Restart DAW after installation

### Launchpad not responding
1. Check MIDI routing in DAW
2. Click **[R]** (Reset) button in plugin UI
3. Verify Launchpad is in Programmer Mode (plugin sends SysEx on startup)
4. Check that both input and output are routed

### LEDs not updating
- Ensure "Launchpad Control" output is routed to Launchpad MIDI input
- Verify routing sends LED commands, not musical notes
- Try **[R]** (Reset) button to re-initialize

### Notes not playing
- Check MIDI routing: "MIDI Out" port should go to instrument
- Verify transport is playing
- Check sequence has active steps (green cells)
- Ensure pitch range includes notes your instrument can play

### UI buttons not working
- Pitch shift buttons (+/-) send MIDI CC internally - should work immediately
- If unresponsive, check console output (`stderr`) for debug messages
- Try reloading plugin

## License

ISC License

```
Copyright (C) 2025 Danny

Permission to use, copy, modify, and/or distribute this software
for any purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE.
```

## Contributing

This project follows C99 standards with strict compiler warnings. See [CLAUDE.md](CLAUDE.md) for detailed development guidelines.

## Credits

- Built with LV2 plugin framework
- UI rendering with Cairo and X11
- Designed for Novation Launchpad Mini Mk3
- Tested on Ubuntu x64 with Reaper
