# Flues Control LV2 Plugin

A MIDI CC controller plugin specifically designed for controlling [flues-synth](../../flues-synth/). Provides 31 program presets and 9 parameter sliders that output MIDI Program Changes and CCs to control the synthesizer.

## Features

- **MIDI Pass-Through**: All incoming MIDI note messages pass through unchanged
- **31 Program Presets**: Switch between different synthesis modes in flues-synth (0-30)
- **9 Parameter Sliders**: Control context-dependent parameters that remap per program
- **Hardware Controller Emulation**: Mirrors the behavior of a physical MIDI controller
- **Custom UI**: X11/Cairo panel with dynamic labels per program (falls back to generic UI)
- **Efficient MIDI Output**: Sends messages only when controls change

## Controls

### Program Selector
Dropdown menu with 31 synthesis programs:
- **0-7**: Basic programs (Disyn Echo, Disyn + Filter, Trajectory, Formant Voice, Hybrid, Physical Model, Direct)
- **8-17**: Algorithm showcase (ModFM, DSF, PAF, Tanh, Hybrid, Feedback, Dirichlet, Multi-Algorithm, Spectral Sculptor)
- **18-24**: Combination algorithms (Hybrid Formant, Cascaded, Parallel Bank, Feedback, Morphing, Inharmonic, Adaptive Filter)
- **25-27**: Novel algorithms (Multi-Stage, Freq Asymmetry, Cross-Mod)
- **28**: Vocal Morph - Complete vocal synthesis with formants + vocal modes
- **29**: Taylor Series (Alg 17) - Educational Taylor series approximation with fundamental + 2nd harmonic blend
- **30**: Disyn + Delays (Legacy) - Original delay-augmented distortion program

For complete program descriptions, see [flues-synth/docs/PROGRAM_CHANGE.md](../../flues-synth/docs/PROGRAM_CHANGE.md).

### 9 Parameter Sliders
Each slider outputs a fixed MIDI CC but controls different parameters per program:

| Control Label      | CC  | Description |
|--------------------|-----|-------------|
| Slider 1 (CC 73)   | 73  | Program-dependent: Algorithm/Feedback/Filter Freq/F1 Jaw/etc. |
| Slider 2 (CC 72)   | 72  | Program-dependent: Param1/Feedback/Filter Q/F2 Tongue/etc. |
| Slider 3 (CC 28)   | 28  | Program-dependent: Param2/Filter Feedback/Filter Shape/F3 Lips/etc. |
| Slider 4 (CC 30)   | 30  | Program-dependent: Interface Type/Disyn Level/F4 Quality/Jitter/etc. |
| Slider 5 (CC 74)   | 74  | Program-dependent: Intensity/Noise Level/Disyn Level/etc. |
| Slider 6 (CC 71)   | 71  | Program-dependent: Tuning/Nasal/etc. |
| Slider 7 (CC 1)    | 1   | Program-dependent: Delay Feedback/Ratio/Jitter/etc. |
| Attack (CC 27)     | 27  | Envelope attack time (1-1000ms, fixed across all programs) |
| Release (CC 7)     | 7   | Envelope release time (10-3000ms, fixed across all programs) |

## MIDI Input/Output

The plugin processes MIDI on **MIDI Channel 1**:

**Input**:
- **Note messages**: Pass through unchanged (Note On, Note Off, etc.)
- All other MIDI events pass through unchanged

**Output**:
- **Pass-through**: All incoming MIDI events (notes, etc.)
- **Program Change** (0xC0): Sent when program selector changes (0-30)
- **Control Change** (0xB0): Sent when sliders change (CCs: 73, 72, 28, 30, 74, 71, 1, 27, 7)

## Usage

### DAW Setup

1. **Add flues-control** to a MIDI or Instrument track
2. **Add flues-synth** to the same or another track
3. **Route MIDI**:
   - MIDI source (keyboard/sequencer) → flues-control MIDI input
   - flues-control MIDI output → flues-synth MIDI input
4. **Route Audio**: flues-synth audio output → Master bus

Example track setup:
```
[MIDI Keyboard] → [flues-control] → MIDI → [flues-synth] → Audio Out
```

The plugin passes through note messages while adding CC and Program Change messages.

### Typical Workflow

1. **Select a program** from the dropdown (e.g., "Formant Voice")
2. **Play a MIDI note** to flues-synth (external controller or sequencer)
3. **Adjust sliders** to control synthesis parameters
4. **Switch programs** to explore different sound palettes

### Parameter Mapping Examples

**Program 3: Formant Voice**
- Slider 1 → F1 (Jaw position, 200-1000 Hz)
- Slider 2 → F2 (Tongue position, 500-3000 Hz)
- Slider 3 → F3 (Lip rounding, 1500-4000 Hz)
- Slider 4 → F4 (Voice quality, 2500-4500 Hz)
- Slider 5 → Noise Level
- Slider 6 → Nasal resonance toggle
- Slider 7 → Jitter (0-10 degrees)
- Slider 8 → Attack
- Slider 9 → Release

**Program 0: Disyn Echo**
- Slider 1 → Disyn Algorithm (0-6)
- Slider 2 → Algorithm Parameter 1
- Slider 3 → Algorithm Parameter 2
- Slider 4 → Interface Type (0-11)
- Slider 5 → Interface Intensity
- Slider 6 → Tuning (-12 to +12 semitones)
- Slider 7 → Delay Feedback
- Slider 8 → Attack
- Slider 9 → Release

**Program 2: Trajectory Polygon**
- Slider 1 → Sides (3-12)
- Slider 2 → Start Position (0-360 degrees)
- Slider 3 → Start Angle (0-360 degrees)
- Slider 4 → Jitter (0-10 degrees)
- Slider 5 → Clip Drive (tanh soft clip, 1.0x-5.0x)
- Slider 6 → Mix X (0-100%)
- Slider 7 → Mix Y (0-100%)
- Slider 8 → Attack
- Slider 9 → Release

**Program 1: Disyn + Filter**
- Slider 1 → Filter Frequency
- Slider 2 → Filter Q
- Slider 3 → Filter Shape
- Slider 4 → Disyn Level
- Slider 5 → Intensity
- Slider 6 → Tuning
- Slider 7 → Ratio
- Slider 8 → Attack
- Slider 9 → Release

For complete parameter mappings, see [flues-synth/docs/midi.md](../../flues-synth/docs/midi.md).

## Building

### Dependencies

- CMake 3.16+
- C++17 compiler
- LV2 development files

Install on Ubuntu/Debian:
```bash
sudo apt install build-essential cmake liblv2-dev
```

### Build Commands

```bash
cd lv2/flues-control
cmake -S . -B build
cmake --build build
cmake --install build --prefix ~/.lv2
```

### Verify Installation

```bash
# List installed plugins
lv2ls | grep flues-control

# Show plugin info
lv2info https://danja.github.io/flues/plugins/flues-control
```

Expected output:
```
https://danja.github.io/flues/plugins/flues-control
    Plugin: Flues Control
    Class: Utility
    Port 0: midi_in (MIDI Input)
    Port 1: midi_out (MIDI Output)
    Port 2: program (Program)
    Port 3-11: slider1-9 (Slider 1-9)
```

## Testing

### Standalone Testing (with jalv)

```bash
jalv.gtk https://danja.github.io/flues/plugins/flues-control
```

You should see:
- Program dropdown with 8 labeled options
- Sliders 1-7 labeled with CC numbers (e.g., "Slider 1 (CC 73)")
- Sliders 8-9 labeled "Attack (CC 27)" and "Release (CC 7)"

Change controls and check terminal output:
```
Flues Control: Program Change → 3
Flues Control: Slider 1 (CC 73) → 64
Flues Control: Slider 2 (CC 72) → 80
```

### Integration Testing (with flues-synth)

**Terminal 1: Start flues-synth**
```bash
cd ../../flues-synth
meson compile -C builddir
./builddir/flues-synth hw:2,0
```

**Terminal 2: Start flues-control in jalv**
```bash
jalv.gtk https://danja.github.io/flues/plugins/flues-control
```

**Terminal 3: Connect MIDI routing**
```bash
# List MIDI ports
aconnect -l

# Connect jalv output to flues-synth input
aconnect <jalv-port> <flues-synth-port>
```

**Test**:
1. Change program in flues-control → flues-synth should print "Program X"
2. Move sliders → flues-synth should print CC messages
3. Play MIDI note → sound should respond to slider changes

### DAW Testing

Load in Reaper/Ardour/Carla:
1. Add flues-control plugin to track
2. Add flues-synth plugin to same or different track
3. Route MIDI: flues-control → flues-synth
4. Verify parameter control works

## Technical Details

### Architecture

- **Plugin Type**: LV2 Utility Plugin
- **MIDI Ports**:
  - Input: Atom sequence (receives note messages)
  - Output: Atom sequence (passes through + adds CCs)
- **Control Ports**: 1 enumeration (program) + 9 floats (sliders)
- **Change Detection**: Only sends MIDI when values change (efficient)
- **Channel**: Fixed to MIDI Channel 1

### Signal Flow

```
MIDI Input (notes, etc.)
    ↓
Pass through all events to MIDI Output
    ↓
User adjusts controls
    ↓
LV2 control ports updated by host
    ↓
run() callback detects changes
    ↓
Convert float (0.0-1.0) → CC value (0-127)
    ↓
Append Program Change and CC messages to atom sequence
    ↓
MIDI output port (notes + CCs) → flues-synth input
    ↓
flues-synth remaps CCs based on active program
    ↓
Synthesis parameters updated
```

### Design Philosophy

This plugin mirrors a **hardware MIDI controller**:
- Fixed CC assignments per slider
- Generic slider labels (not parameter-specific)
- Target device (flues-synth) handles remapping
- Simple, predictable behavior

This approach:
- Keeps plugin code simple and maintainable
- Avoids duplicating flues-synth's mapping logic
- Matches user expectations from hardware workflow
- Works with any future flues-synth parameter changes

## Troubleshooting

**Problem**: Plugin not listed in host
- Solution: Verify installation with `lv2ls | grep flues-control`
- Check: `~/.lv2/flues-control.lv2/` exists and contains `.so` + `.ttl` files

**Problem**: No MIDI output
- Solution: Check MIDI routing in host (some hosts require explicit MIDI connections)
- Enable: Plugin debug output to verify CC messages are generated

**Problem**: Sliders don't affect sound
- Solution: Verify flues-synth is receiving MIDI CCs (check terminal output with `FLUES_MIDI_DEBUG=1`)
- Check: Correct program is selected (different programs use different parameters)
- Verify: MIDI routing is correct (flues-control → flues-synth)

**Problem**: Program changes don't work
- Solution: Ensure flues-synth supports MIDI Program Change (all versions since 2025-12)
- Check: Terminal output shows "Program Change → X" message

## Future Enhancements

Possible additions for Phase 2:
- **Control Notes**: 6 toggle buttons for module on/off (notes 36-41)
- **MIDI Channel Selector**: Control port to set output channel (1-16)
- **Preset System**: Save/recall slider positions via LV2 state extension
- **Custom UI**: Dynamic parameter labels per program (requires X11/Cairo UI)

## See Also

- [flues-synth README](../../flues-synth/README.md) - Synthesis engine documentation
- [MIDI Reference](../../flues-synth/docs/midi.md) - Complete MIDI CC specification
- [Program Change Guide](../../flues-synth/docs/PROGRAM_CHANGE.md) - Program configurations
- [LV2 Specification](https://lv2plug.in/ns/lv2core/lv2core.html) - LV2 plugin standard

## License

MIT License - See repository root for details

## Version History

- **0.1** (2025-12-11): Initial release
  - 31 programs, 9 sliders
  - MIDI Program Change and CC output
  - Default LV2 UI
