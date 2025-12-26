# Quadrangle

**Performance instrument LV2 plugin for Novation Launchpad Mini MK3**

Quadrangle transforms the Launchpad into a powerful standalone performance instrument with four distinct zones optimized for electronic music production and live performance.

## Concept

The 8×8 grid is divided into four quadrants, each serving a specific musical function:

```
┌─────────────┬─────────────┐
│   DRUMS     │   MELODY    │  Top Row: Transport & Pattern Controls
│  Sequencer  │  Sequencer  │
│  16-step    │  16-step    │
│  8 voices   │  Quantized  │
│  🔴 Red     │  🔵 Blue    │
├─────────────┼─────────────┤
│    LIVE     │   PARAMS    │  Side Buttons: Voice/Mode Selection
│  Pads (16)  │ Controls(16)│
│  One-shots  │  Context    │
│  🟢 Green   │  🟣 Purple  │
└─────────────┴─────────────┘
```

### Quadrant Details

**Northwest - Drum Sequencer** (Rows 4-7, Cols 0-3)
- 16-step sequencer with 8 selectable drum voices
- Each voice maps to GM drum notes (36-43: kick, snare, hats, toms, etc.)
- Side buttons (0-7) select active voice for editing
- Visual step display with playhead animation
- Red color scheme for visibility

**Northeast - Melody Sequencer** (Rows 4-7, Cols 4-7)
- 16-step melodic sequencer with scale quantization
- Vertical position = pitch (higher = higher note)
- Horizontal position = time (left to right)
- Supports multiple scales: Major, Minor, Pentatonic, Blues, Dorian, Chromatic
- Blue color scheme

**Southwest - Live Performance Pads** (Rows 0-3, Cols 0-3)
- 16 velocity-sensitive trigger pads
- Instant triggering for live performance (MIDI channel 2)
- Green color scheme for easy identification

**Southeast - Parameter Controls** (Rows 0-3, Cols 4-7)
- Two rows of 2-bit controls per column (binary 00/01/10/11)
- Bottom row CCs: 74, 71, 1, 27
- Top row CCs: 73, 72, 28, 30
- Purple color scheme

## Features

- **Dual Sequencers:** Independent drum and melody patterns running simultaneously
- **Pattern Storage:** 2 pattern slots (A/B) accessible via top buttons
- **Edge Dynamics:** Inspired by the design brief - changes near center are subtle, edges are extreme (though currently uniform for MVP)
- **Tempo Control:** 60-240 BPM with MIDI clock sync
- **Scale Quantization:** Multiple musical scales for melody sequencer
- **Visual Feedback:** Color-coded zones with playhead animation and state indicators
- **Modular Architecture:** Clean separation allows easy extension and hardware swapping
- **UI Parity:** X11/Cairo UI mirrors hardware state and supports mouse pad clicks + tooltips

## Building

### Dependencies

```bash
# Ubuntu/Debian
sudo apt install build-essential cmake pkg-config lv2-dev

# Arch Linux
sudo pacman -S base-devel cmake lv2

# Fedora
sudo dnf install gcc gcc-c++ cmake lv2-devel
```

### Compile

```bash
cd lv2/quadrangle
cmake -S . -B build
cmake --build build
cmake --install build --prefix ~/.lv2
```

### Verify Installation

```bash
lv2ls | grep quadrangle
# Should output: https://danja.github.io/flues/plugins/quadrangle

lv2info https://danja.github.io/flues/plugins/quadrangle
```

## Usage

### DAW Setup (Reaper Example)

1. **Insert Plugin:**
   - Create new track
   - Insert LV2: Quadrangle
   - Plugin window will open

2. **Connect Launchpad:**
   - Ensure Launchpad Mini MK3 is connected via USB
   - In Reaper: Options → Preferences → MIDI Devices
   - Enable "LPMiniMK3 MIDI In/Out" for control input/output

3. **Route MIDI:**
   ```
   [Launchpad MIDI Out] → [Quadrangle Control In]
   [Quadrangle Control Out] → [Launchpad MIDI In]
   [Quadrangle MIDI Out] → [Your Synth/Sampler]
   ```

4. **Automatic Initialization:**
   - Plugin automatically sends Programmer Mode command to Launchpad
   - Grid lights up showing current pattern state
   - Ready to perform!

### Controls

**Top Row (L to R):**
- Button 0: Program Up (sends MIDI Program Change +1 on channel 1)
- Button 1: Program Down (sends MIDI Program Change -1 on channel 1)
- Button 2: Euclid Beats Down (current drum voice)
- Button 3: Euclid Beats Up (current drum voice)
- Button 4: Euclid Offset Up (current drum voice)
- Button 5: Pattern A
- Button 6: Pattern B
- Button 7: ▶️ Play/Stop (Green when playing)
- Button 8: 🗑️ Clear current pattern

**Side Buttons (Top to Bottom):**
- Buttons 0-7: Select drum voice for editing (voice 0-7)
- Selected voice is highlighted white

**Grid Interaction:**

*Drum Sequencer (NW):*
- Tap pad: Toggle step on/off
- Lit pad = step active
- Bright green = current playhead position
- Use side buttons to switch between 8 drum voices

*Melody Sequencer (NE):*
- Tap pad: Toggle note at that step
- Vertical = pitch (top = high, bottom = low)
- Horizontal = step position (0-15)
- Notes auto-quantize to selected scale

*Live Pads (SW):*
- Tap: Trigger note immediately (channel 2)
- Brightness indicates velocity
- Green flash on trigger
- Perfect for fills, accents, and improvisation

*Parameter Controls (SE):*
- Two-bit CC toggles per column (binary 00/01/10/11)
- Bottom row CCs: 74, 71, 1, 27
- Top row CCs: 73, 72, 28, 30

## Architecture

### File Structure

```
lv2/quadrangle/
├── include/
│   ├── launchpad_config.h     # Launchpad MIDI constants
│   ├── grid_state.h           # 8×8 grid memory map
│   ├── midi_comm.h            # SysEx/MIDI communication
│   └── quadrangle_engine.h    # Main engine logic
├── src/
│   ├── grid_state.c           # State management
│   ├── midi_comm.c            # MIDI I/O implementation
│   ├── quadrangle_engine.c    # Engine implementation
│   └── quadrangle_plugin.cpp  # LV2 wrapper
├── quadrangle.lv2/
│   ├── manifest.ttl           # LV2 manifest
│   └── quadrangle.ttl         # LV2 metadata
├── CMakeLists.txt
└── README.md
```

### Key Design Patterns

**Modular MIDI Layer:**
- `launchpad_config.h` contains all hardware-specific constants
- To support a different controller, create new config header
- No changes needed to core engine

**Clean Separation:**
- State management (grid_state)
- MIDI communication (midi_comm)
- Musical logic (quadrangle_engine)
- LV2 interface (quadrangle_plugin)

**Memory Mapped Grid:**
- Direct 8×8 array maps to physical pads
- Efficient LED bulk updates via SysEx
- Quadrant helpers for spatial logic

## MIDI Protocol

Quadrangle communicates with the Launchpad using standard MIDI messages:

### Outgoing (Plugin → Launchpad)

**Initialization:**
```
F0 00 20 29 02 0D 0E 01 F7  // Enter Programmer Mode
```

**LED Control (Note On):**
```
90 <note> <color>  // Static color
91 <note> <color>  // Flashing color
92 <note> <color>  // Pulsing color
```

**Bulk LED Update (SysEx):**
```
F0 00 20 29 02 0D 03 <colorspec>... F7
```

### Incoming (Launchpad → Plugin)

**Pad Press:**
```
90 <note> <velocity>  // Note On (grid pad)
B0 <cc> <value>       // Control Change (side/top buttons)
```

**Pad Release:**
```
80 <note> 00  // Note Off
90 <note> 00  // Note On with velocity 0
```

## Future Enhancements

- **Euclidean Sequencer:** Algorithmic rhythm generation
- **Step Probability:** Per-step trigger probability for generative patterns
- **Ratcheting:** Retriggering steps for rolls and fills
- **Parameter Automation:** Record parameter movements into patterns
- **MIDI Learn:** Assign parameters to external controllers
- **Pattern Chaining:** Create song structures
- **Live Recording:** Capture live pad performance into patterns
- **Scale Modes:** More scale options and custom scale editor
- **Swing/Groove:** Per-voice swing and groove templates
- **Voice Management:** Save/load drum kit configurations
- **Velocity Curves:** Adjustable velocity response
- **NSEW Dynamics:** Implement center-calm, edge-extreme parameter scaling
- **X11/Cairo UI:** Visual monitor showing grid state for hosts without MIDI routing display

## Technical Notes

### Sample-Accurate Sequencing

The engine maintains a sample counter that advances with each audio buffer. When `sample_counter >= samples_per_step`, the sequencer advances to the next step and triggers any active notes. This ensures rock-solid timing independent of buffer size.

### LED Update Throttling

LED updates are sent every 64 samples (~1.3ms @ 48kHz) to avoid flooding the MIDI bus while maintaining responsive visual feedback.

### MIDI Output

Currently, the plugin outputs MIDI events on the `midi_out` port. Connect this to a synthesizer or sampler (like DrumKit, Disyn, or external instruments) to hear the sequence.

## Troubleshooting

**Launchpad not responding:**
- Check MIDI routing in DAW
- Ensure Launchpad is in standalone mode (not connected to Ableton Live)
- Try unplugging and reconnecting
- Check that "LPMiniMK3 MIDI" appears in MIDI device list

**LEDs not lighting:**
- Plugin may not have initialized
- Check that Control Out is routed to Launchpad MIDI In
- Reload plugin to send initialization sequence

**No sound:**
- Quadrangle is a MIDI sequencer, not a synthesizer
- Connect `midi_out` port to a synth/sampler
- Check that receiving instrument is on correct MIDI channel (Ch 1)

**Wrong pads lighting:**
- Launchpad might be in wrong mode
- Reload plugin to force Programmer Mode
- Check Launchpad firmware version (should be recent)

## Contributing

Quadrangle is part of the Flues synthesizer collection. Contributions welcome!

**Areas for contribution:**
- Additional scales and quantization modes
- Euclidean rhythm generator
- Parameter automation recording
- X11/Cairo visual UI
- Support for other Launchpad models (Pro, X, etc.)
- Documentation improvements

## License

MIT License - see repository root

## Credits

**Design:** Inspired by modular synthesis, TR-808/909 workflow, and generative music concepts

**Implementation:** Part of the Flues physical modeling synthesizer suite

**Hardware:** Designed for Novation Launchpad Mini MK3 (8×8 RGB grid)

---

**Built with:** C/C++, LV2, CMake, MIDI, SysEx, and a passion for expressive electronic instruments 🎹✨
