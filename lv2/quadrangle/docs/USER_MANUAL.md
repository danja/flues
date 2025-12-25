# Quadrangle User Manual

**Version:** 1.0
**Plugin Type:** LV2 Instrument
**Hardware:** Novation Launchpad Mini MK3
**Author:** Danny Ayers

---

## Table of Contents

1. [Introduction](#introduction)
2. [Concept](#concept)
3. [Quick Start](#quick-start)
4. [The Four Quadrants](#the-four-quadrants)
5. [User Interface](#user-interface)
6. [Hardware Interface](#hardware-interface)
7. [Transport & Timing](#transport--timing)
8. [Performance Workflow](#performance-workflow)
9. [Technical Details](#technical-details)
10. [Troubleshooting](#troubleshooting)

---

## Introduction

Quadrangle is a performance instrument plugin designed specifically for the Novation Launchpad Mini MK3. It transforms the 8×8 grid into a powerful, expressive performance tool by dividing it into four functional quadrants, each serving a different musical role.

### What Makes Quadrangle Different?

- **Hardware-first design**: Built around the Launchpad's physical interface
- **Quadrant paradigm**: Four specialized zones instead of one-size-fits-all
- **Real-time performance**: Designed for live playing, not just programming
- **Visual feedback**: Multicolor LEDs provide immediate state visualization
- **DAW integration**: Syncs to transport, generates MIDI notes

---

## Concept

### The Quadrant Philosophy

The 8×8 Launchpad grid is divided into four 4×4 quadrants:

```
┌─────────────┬─────────────┐
│  TOP-LEFT   │  TOP-RIGHT  │
│   (GREEN)   │  (PURPLE)   │
│             │             │
│   MELODY    │  PARAMETERS │
│  SEQUENCER  │  CONTROLS   │
│             │             │
├─────────────┼─────────────┤
│ BOTTOM-LEFT │BOTTOM-RIGHT │
│    (RED)    │   (BLUE)    │
│             │             │
│    DRUM     │    LIVE     │
│  SEQUENCER  │    PADS     │
│             │             │
└─────────────┴─────────────┘
```

Each quadrant has its own:
- **Visual identity** (color scheme)
- **Musical function** (drums, melody, live, parameters)
- **Interaction model** (step programming vs. real-time)
- **MIDI output behavior**

### Cardinal Directions Concept

Within each quadrant, pad position has meaning:

- **Center**: Calm, neutral values
- **Edges**: Extreme values
- **Vertical**: Pitch or intensity
- **Horizontal**: Time or modulation

---

## Quick Start

### 1. Load the Plugin

**In Reaper:**
1. Create a new MIDI track
2. Insert virtual instrument: "Quadrangle"
3. Open routing matrix: View → Routing Matrix
4. Connect MIDI:
   - `Launchpad Mini MK3 DAW out` → `Quadrangle Control In`
   - `Quadrangle Launchpad Control` → `Launchpad Mini MK3 DAW in`

**In jalv (standalone):**
```bash
jalv https://danja.github.io/flues/plugins/quadrangle
# Then use QjackCtl to route MIDI
```

### 2. Initialize Hardware

When Quadrangle loads:
1. Launchpad automatically enters **Programmer Mode**
2. All LEDs initialize (brief flash)
3. Current quadrant pattern appears
4. Ready for input!

### 3. Start Playing

1. **Press Play** in your DAW
2. **Bottom-left (red)**: Tap pads to program drum pattern
3. **Top-left (green)**: Program melody sequence
4. **Bottom-right (blue)**: Play pads live for instant triggers
5. **Top-right (purple)**: Adjust parameters in real-time

---

## The Four Quadrants

### Bottom-Left: Drum Sequencer (RED)

**Purpose:** Step sequencer for percussive elements

**Layout:**
```
Row 3: [Hi-Hat] [Clap] [Snare] [Kick]
Row 2: [Tom 1 ] [Tom 2] [Ride] [Crash]
Row 1: [Perc 1] [Perc 2][Perc 3][Perc 4]
Row 0: [Step 1][Step 2][Step 3][Step 4]
```

**How It Works:**
- **Columns**: Each column represents a 16th note step
- **Rows**: Each row is a different drum sound
- **Press once**: Enable step (LED bright red)
- **Press again**: Disable step (LED dim red or off)
- **Playhead**: Yellow LED follows the beat

**MIDI Output:**
- Channel 10 (drums)
- Note velocity: Fixed 96
- Note numbers: C2-D3 (36-50)
- Duration: 50ms (short, punchy)

**Performance Tips:**
- Build patterns by tapping in rhythm
- Use bottom row for kick patterns
- Top rows for hats and percussion
- Current step highlighted in yellow

---

### Top-Left: Melody Sequencer (GREEN)

**Purpose:** Step sequencer for melodic/harmonic content

**Layout:**
```
Row 7: [C5 ] [D5 ] [E5 ] [F5 ]
Row 6: [G4 ] [A4 ] [B4 ] [C5 ]
Row 5: [E4 ] [F4 ] [G4 ] [A4 ]
Row 4: [C4 ] [D4 ] [E4 ] [F4 ]
```

**How It Works:**
- **Columns**: 16th note steps (same timing as drums)
- **Rows**: Musical pitches (chromatic or scale-locked)
- **Press once**: Enable note (LED bright green)
- **Press again**: Disable note (LED off)
- **Multiple notes per step**: Chords possible

**MIDI Output:**
- Channel 1 (melodic)
- Note velocity: Derived from vertical position (64-127)
- Note duration: Tied to pattern length (legato or staccato)
- Pitch range: C3-C6 (configurable)

**Performance Tips:**
- Start with simple melodies
- Add harmony by enabling multiple rows per column
- Higher rows = higher notes = more intensity
- Experiment with polyrhythmic patterns

---

### Bottom-Right: Live Pads (BLUE)

**Purpose:** Real-time trigger pads for immediate response

**Layout:**
```
Row 3: [Synth 1][Synth 2][Synth 3][Synth 4]
Row 2: [Bass 1 ][Bass 2 ][Bass 3 ][Bass 4 ]
Row 1: [Lead 1 ][Lead 2 ][Lead 3 ][Lead 4 ]
Row 0: [FX 1   ][FX 2   ][FX 3   ][FX 4   ]
```

**How It Works:**
- **Press and hold**: Note On (LED bright blue)
- **Release**: Note Off (LED off)
- **Position = pitch**: Bottom = low, top = high
- **Position = timbre**: Left = soft, right = bright

**MIDI Output:**
- Channel 2 (instruments)
- Note velocity: Pressure-sensitive (if supported, else 96)
- Note duration: While held (polyphonic)
- CC 74 (brightness): Horizontal position 0-127

**Performance Tips:**
- Use for basslines and lead fills
- Corner pads are extreme (very low/high, dark/bright)
- Center pads are balanced and safe
- Great for improvisation over sequenced patterns

---

### Top-Right: Parameter Controls (PURPLE)

**Purpose:** Real-time parameter modulation and effects

**Layout:**
```
Row 7: [Cutoff][Resonance][Attack][Release]
Row 6: [Reverb][Delay   ][Drive][Mix    ]
Row 5: [LFO 1 ][LFO 2   ][Mod  ][Depth  ]
Row 4: [Macro ][Scene   ][Mute ][Solo   ]
```

**How It Works:**
- **Press**: Latch parameter (stays on)
- **Hold + move**: Adjust value in real-time
- **Vertical position**: Parameter value 0-127
- **LED brightness**: Shows current value

**MIDI Output:**
- Continuous Controllers (CC)
- CC 74: Filter Cutoff
- CC 71: Filter Resonance
- CC 73: Attack
- CC 72: Release
- CC 91: Reverb
- CC 94: Delay
- CC 1: Mod Wheel (LFO depth)

**Performance Tips:**
- Assign to synth parameters in your DAW
- Use for filter sweeps during builds
- Top row = extreme values
- Center = neutral starting point

---

## User Interface

### On-Screen Display

The Quadrangle UI shows:

```
┌──────────────────────────────────────────────────┐
│  Quadrangle - Launchpad Performance Instrument   │
├──────────────────────────────────────────────────┤
│                                                  │
│   8×8 Grid Visualization                         │
│   ┌─────────────┬─────────────┐                 │
│   │   MELODY    │  PARAMETERS │  ← Top row       │
│   │   (green)   │  (purple)   │                  │
│   ├─────────────┼─────────────┤                 │
│   │    DRUMS    │    LIVE     │  ← Bottom row    │
│   │    (red)    │   (blue)    │                  │
│   └─────────────┴─────────────┘                 │
│                                                  │
│   Current Playhead: [████████          ] Step 8  │
│                                                  │
├──────────────────────────────────────────────────┤
│  Status:                                         │
│  ▶ Playing | Pattern A | Voice 1 | 120 BPM      │
│  Step 8/16                                       │
└──────────────────────────────────────────────────┘
```

**Visual Feedback:**
- **Grid colors**: Match hardware LED colors
- **Playhead**: Yellow moving indicator
- **Active steps**: Bright pads
- **Current pattern**: Letter indicator (A-D)
- **Transport state**: Play/Stop icon

### Color Coding

| Quadrant | Function | Color | LED Code |
|----------|----------|-------|----------|
| Top-Left | Melody Sequencer | Green | 21 (bright) |
| Top-Right | Parameters | Purple | 53 (bright) |
| Bottom-Left | Drum Sequencer | Red | 5 (bright) |
| Bottom-Right | Live Pads | Blue | 45 (bright) |
| Playhead | Current Step | Yellow | 13 (bright) |
| Off | Inactive | Black | 0 |

---

## Hardware Interface

### Launchpad Layout

```
        [Top Buttons: 91-98]
        ┌───────────────────────┐
     89 │  MELODY  │ PARAMETERS │ 79
     88 │  (GREEN) │ (PURPLE)   │ 69
        ├──────────┼────────────┤
     87 │  DRUMS   │   LIVE     │ 59
     86 │  (RED)   │  (BLUE)    │ 49
        └──────────┴────────────┘
     [Scene Buttons: 89-86-87-86...]
```

### Button Functions

**Top Row (CC 91-98):**
- CC 91: Pattern A
- CC 92: Pattern B
- CC 93: Pattern C
- CC 94: Pattern D
- CC 95: Tap Tempo
- CC 96: Record
- CC 97: Clear
- CC 98: Copy

**Side Buttons (CC 89-79):**
- CC 89: Voice 1 (top)
- CC 79: Voice 2
- CC 69: Voice 3
- CC 59: Voice 4
- CC 49: Voice 5
- CC 39: Voice 6
- CC 29: Voice 7
- CC 19: Voice 8 (bottom)

### LED Feedback

**Programmer Mode (Active):**
- All LEDs controllable
- RGB palette available
- Multicolor mixing
- Brightness levels: 0-127

**State Indicators:**
- **Flashing**: Waiting for input
- **Pulsing**: Modulation active
- **Solid**: Enabled/latched
- **Off**: Disabled

---

## Transport & Timing

### DAW Sync

Quadrangle syncs to your DAW's transport:

**Receives:**
- `time:Position` - Current playback position
- `time:beatsPerMinute` - Tempo (BPM)
- `time:speed` - Play/Stop state (0 = stopped, 1 = playing)

**Timing Resolution:**
- 16 steps per pattern
- 16th note quantization
- Adjustable pattern length: 4, 8, 12, 16 steps

**Behavior:**
- **Play**: Sequencers advance, playhead moves
- **Stop**: All sequences pause, LEDs hold state
- **Loop**: Pattern repeats automatically

### Tempo Changes

- BPM from DAW applied in real-time
- No glitches on tempo change
- Pattern length in bars stays constant
- Recommended range: 60-180 BPM

---

## Performance Workflow

### Basic Session

1. **Load & Connect:**
   - Insert Quadrangle on MIDI track
   - Route Launchpad MIDI (see Quick Start)
   - Verify LED initialization

2. **Build Drum Pattern:**
   - Press Play in DAW
   - Tap pads in bottom-left (red) quadrant
   - Listen to pattern loop
   - Add/remove steps to taste

3. **Add Melody:**
   - Switch to top-left (green) quadrant
   - Program simple melody
   - Try chords (multiple rows per column)

4. **Live Performance:**
   - Use bottom-right (blue) for bassline
   - Hold pads while sequence plays
   - Jam over the pattern

5. **Modulate:**
   - Touch top-right (purple) pads
   - Sweep filters, adjust reverb
   - Create builds and breakdowns

### Advanced Techniques

**Pattern Chaining:**
- Use top buttons to switch patterns (A/B/C/D)
- Build verse/chorus/bridge structures
- Copy patterns with CC 98

**Voice Layering:**
- Select different voices with side buttons
- Each voice gets independent MIDI channel
- Layer drums + melody + bass

**Live Looping:**
- Record live pad performance
- Clear patterns with CC 97
- Tap tempo with CC 95

**Parameter Automation:**
- Latch purple quadrant controls
- Record parameter changes in DAW
- Create evolving textures

---

## Technical Details

### MIDI Implementation

**Input Ports:**
- **Control In (Port 0)**: Atom Sequence
  - MIDI from Launchpad (Note On/Off, CC)
  - Transport sync (time:Position)

**Output Ports:**
- **MIDI Out (Port 1)**: Atom Sequence
  - Musical notes from sequencers and live pads
  - Channels 1-10 depending on quadrant

- **Launchpad Control (Port 2)**: Atom Sequence
  - LED control messages (Note On for grid LEDs)
  - SysEx for mode switching

**Audio Ports:**
- **Audio Out L (Port 3)**: Silent (MIDI plugin)
- **Audio Out R (Port 4)**: Silent (MIDI plugin)

### SysEx Messages

**Enter Programmer Mode:**
```
F0 00 20 29 02 0D 0E 01 F7
```

**Exit Programmer Mode:**
```
F0 00 20 29 02 0D 0E 00 F7
```

**LED Control:**
- Individual Note On messages (0x90)
- Note = grid position (11-88)
- Velocity = color (0-127)

### Grid Mapping

**Note to Grid Position:**
```c
uint8_t row = (note - 11) / 10;
uint8_t col = (note - 11) % 10;
```

**Grid to Note Position:**
```c
uint8_t note = 11 + (row * 10) + col;
```

**Valid Notes:** 11-88 (8×8 grid, rows 0-7, cols 0-7)

### Resource Usage

- **CPU**: ~1-2% (modern CPU)
- **Memory**: ~10 MB
- **Latency**: <5ms (typical)
- **Polyphony**: 32 voices (live pads)
- **Sequencer steps**: 16 per pattern × 4 patterns

---

## Troubleshooting

### LEDs Don't Light Up

**Symptom:** Launchpad stays dark when plugin loads

**Solutions:**
1. Check MIDI routing:
   - `Quadrangle Launchpad Control` → `Launchpad DA in`
2. Verify Launchpad is in DAW mode (not standalone)
3. Look for console message: "Sent Programmer Mode to BOTH outputs"
4. Try unplugging/replugging Launchpad USB

### No Response to Pad Presses

**Symptom:** Pressing pads doesn't change grid or trigger sounds

**Solutions:**
1. Check MIDI routing:
   - `Launchpad DA out` → `Quadrangle Control In`
2. Watch console for: "Received Note On - note=XX"
3. Verify DAW is armed for MIDI recording
4. Check track is not muted

### LEDs Flash Randomly

**Symptom:** LEDs flicker or show wrong colors

**Solutions:**
1. Disable other Launchpad software (Novation Components)
2. Check for duplicate MIDI connections
3. Verify only one instance of Quadrangle is using Launchpad
4. Reduce MIDI routing complexity

### Plugin Crashes on Close

**Symptom:** DAW freezes when closing Quadrangle

**Solutions:**
1. Close plugin UI before removing plugin
2. Update to latest Quadrangle version
3. Check DAW console for error messages
4. Report bug with crash log

### MIDI Notes Not Heard

**Symptom:** Visual feedback works, but no sound

**Solutions:**
1. Check MIDI routing:
   - `Quadrangle MIDI Out` → `Synth/Sampler MIDI In`
2. Ensure destination instrument is loaded
3. Verify correct MIDI channel on receiver
4. Check track is not muted

### Transport Sync Issues

**Symptom:** Pattern doesn't follow DAW tempo

**Solutions:**
1. Verify DAW transport is running (Play)
2. Check console shows: "Event 1: Object/Blank (transport)"
3. Ensure LV2 host supports `time:Position`
4. Try reloading plugin

---

## Tips & Tricks

### Creative Ideas

1. **Generative Patterns:**
   - Enable random steps in drum quadrant
   - Let patterns evolve naturally
   - Mute/unmute voices for variation

2. **Filter Sweeps:**
   - Use purple quadrant for cutoff
   - Build tension with resonance
   - Create dubstep-style wobbles

3. **Polyrhythmic Experiments:**
   - Different step lengths per quadrant
   - 3/4 in melody vs 4/4 in drums
   - Shifting phase relationships

4. **Live Remixing:**
   - Blue quadrant for one-shots
   - Trigger samples over sequences
   - Build energy with fills

### Performance Tips

- **Practice grid geography**: Memorize where each sound lives
- **Use visual feedback**: LEDs tell you what's active
- **Start simple**: Build patterns gradually
- **Trust the grid**: Muscle memory develops quickly
- **Combine quadrants**: Layer sequenced + live elements

### Integration with DAW

**Reaper:**
- Use ReaLearn for parameter mapping
- Create macros for purple quadrant controls
- Record MIDI output to arrange view

**Ardour:**
- Assign CC to plugin parameters
- Use automation lanes for modulation
- Combine with other LV2 instruments

**Carla:**
- Chain with effects plugins
- Use MIDI filter for channel routing
- Create layered instrument racks

---

## Appendix

### MIDI CC Map

| CC | Parameter | Quadrant | Range |
|----|-----------|----------|-------|
| 1 | Mod Wheel (LFO Depth) | Purple | 0-127 |
| 71 | Filter Resonance | Purple | 0-127 |
| 72 | Release Time | Purple | 0-127 |
| 73 | Attack Time | Purple | 0-127 |
| 74 | Filter Cutoff | Purple | 0-127 |
| 91-98 | Top Buttons | All | Toggle |
| 19-89 | Side Buttons | All | Toggle |

### Note Map

| Quadrant | MIDI Channel | Note Range | Use |
|----------|--------------|------------|-----|
| Drums (Red) | 10 | 36-51 | Percussion |
| Melody (Green) | 1 | 48-84 | Harmonic |
| Live (Blue) | 2 | 36-72 | Real-time |
| Params (Purple) | - | CC only | Control |

### Color Palette

| Name | Code | RGB | Use |
|------|------|-----|-----|
| Red | 5 | 255,0,0 | Drums active |
| Red Dim | 72 | 100,0,0 | Drums inactive |
| Green | 21 | 0,255,0 | Melody active |
| Green Dim | 85 | 0,100,0 | Melody inactive |
| Blue | 45 | 0,0,255 | Live active |
| Blue Dim | 82 | 0,0,100 | Live inactive |
| Purple | 53 | 255,0,255 | Params active |
| Purple Dim | 95 | 100,0,100 | Params inactive |
| Yellow | 13 | 255,255,0 | Playhead |
| Off | 0 | 0,0,0 | Disabled |

---

## Support & Resources

**Source Code:**
https://github.com/danja/flues/tree/main/lv2/quadrangle

**Documentation:**
- [Current Status](./CURRENT_STATUS.md)
- [Test Guide](./LAUNCHPAD_TEST_GUIDE.md)
- [Grid-Seq Comparison](./GRID_SEQ_COMPARISON.md)

**Launchpad Reference:**
See: `docs/reference/launchpad.pdf`

**Report Issues:**
https://github.com/danja/flues/issues

**License:**
MIT License - See source repository

---

**Last Updated:** 2025-12-25
**Version:** 1.0
**Plugin URI:** https://danja.github.io/flues/plugins/quadrangle
