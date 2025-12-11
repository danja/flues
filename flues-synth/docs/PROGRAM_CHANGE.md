# Flues-Synth MIDI Program Change System

## Overview

MIDI Program Change messages (0-7) configure the signal chain routing and dynamically remap the 9 hardware sliders to the most relevant parameters for each program.

### Program Summary

| Program | Name | Signal Chain | Status |
|---------|------|--------------|--------|
| 0 | Disyn Direct | Disyn → Output | Active |
| 1 | Disyn + Delays | Disyn → Delays | Active |
| 2 | Disyn + Filter | Disyn → Delays → Filter | Active |
| 3 | Formant Voice | Noise → Formants | Active |
| 4 | Hybrid Speech | Disyn + Noise → Formants | Active |
| 5 | Physical Model | Noise → Interface → Delays → Filter | Active |
| 6 | Full Hybrid | *DISABLED* (redirects to 5) | Disabled |
| 7 | Disyn Echo | Disyn → Delay1 | Active |

## Hardware Slider Mapping

The MIDI controller has 9 sliders sending these CC numbers:
1. **Slider 1**: CC 73 (context-dependent)
2. **Slider 2**: CC 72 (context-dependent)
3. **Slider 3**: CC 28 (context-dependent)
4. **Slider 4**: CC 30 (context-dependent)
5. **Slider 5**: CC 74 (context-dependent)
6. **Slider 6**: CC 71 (context-dependent)
7. **Slider 7**: CC 1 (context-dependent)
8. **Slider 8**: CC 27 → **Attack** (73) - always
9. **Slider 9**: CC 7 → **Release** (72) - always

**Note**: Sliders 8 and 9 are **always** Attack and Release. Sliders 1-7 change function based on the current program.

## Programs

### Program 0: Disyn Direct
**Signal Chain**: Disyn → Output (all other processing bypassed)

**Use Case**: Raw distortion synthesis, no formants, delays, or filtering

**Slider Mappings**:
- Slider 1: **Disyn Algorithm** (73→16) - Select algorithm 0-6
- Slider 2: **Disyn Param1** (72→17) - Algorithm-specific parameter 1
- Slider 3: **Disyn Param2** (28→18) - Algorithm-specific parameter 2
- Slider 4: **Disyn Level** (30→19) - Disyn output level
- Slider 5: **Intensity** (74→1) - Interface intensity
- Slider 6: **Tuning** (71→26) - Pitch offset
- Slider 7: **Ratio** (1→27) - Delay ratio
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: Disyn level 0.8, master gain 0.7

---

### Program 1: Disyn + Delays
**Signal Chain**: Disyn → Delay Lines (feedback enabled, no formants/filter)

**Use Case**: Add echo/resonance to Disyn without formants

**Slider Mappings**:
- Slider 1: **Delay1 Feedback** (73→28) - Delay line 1 return
- Slider 2: **Delay2 Feedback** (72→29) - Delay line 2 return
- Slider 3: **Filter Feedback** (28→30) - Filter return (usually 0)
- Slider 4: **Disyn Level** (30→19) - Disyn output level
- Slider 5: **Intensity** (74→1) - Interface intensity
- Slider 6: **Tuning** (71→26) - Pitch offset
- Slider 7: **Ratio** (1→27) - Delay ratio
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: Disyn 0.6, Delay1/2 feedback 0.3

---

### Program 2: Disyn + Filter
**Signal Chain**: Disyn → Delay Lines → Filter (full chain except formants)

**Use Case**: Shape Disyn with state-variable filter

**Slider Mappings**:
- Slider 1: **Filter Frequency** (73→32) - SVF cutoff (20Hz-20kHz)
- Slider 2: **Filter Q** (72→33) - Resonance (0.1-10)
- Slider 3: **Filter Shape** (28→34) - LP→BP→HP morph (0-1)
- Slider 4: **Disyn Level** (30→19) - Disyn output level
- Slider 5: **Intensity** (74→1) - Interface intensity
- Slider 6: **Tuning** (71→26) - Pitch offset
- Slider 7: **Ratio** (1→27) - Delay ratio
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: Disyn 0.5, delays 0.25, filter FB 0.2, freq 1kHz, Q=2

---

### Program 3: Formant Voice
**Signal Chain**: Noise → Formant Cascade (vocal synthesis, no Disyn/delays/filter)

**Use Case**: Pure vocal formant synthesis

**Slider Mappings**:
- Slider 1: **F1 (Jaw)** (73→71) - 200-1000 Hz
- Slider 2: **F2 (Tongue)** (72→10) - 500-3000 Hz
- Slider 3: **F3 (Lips)** (28→74) - 1500-4000 Hz
- Slider 4: **F4 (Quality)** (30→75) - 2500-4500 Hz
- Slider 5: **Noise Level** (74→20) - Aspirator level
- Slider 6: **Nasal Toggle** (71→80) - ≥64 = nasal resonance ON
- Slider 7: **Master Gain** (1→7) - Final output level
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: Noise 0.5, F1=500Hz, F2=1500Hz, F3=2500Hz, F4=3500Hz

---

### Program 4: Hybrid Speech
**Signal Chain**: Disyn + Noise → Formant Cascade (speech with harmonic excitation)

**Use Case**: Voiced speech (larynx-like excitation through formants)

**Slider Mappings**:
- Slider 1: **F1 (Jaw)** (73→71) - 200-1000 Hz
- Slider 2: **F2 (Tongue)** (72→10) - 500-3000 Hz
- Slider 3: **F3 (Lips)** (28→74) - 1500-4000 Hz
- Slider 4: **F4 (Quality)** (30→75) - 2500-4500 Hz
- Slider 5: **Disyn Level** (74→19) - Harmonic content
- Slider 6: **Noise Level** (71→20) - Aspirator level
- Slider 7: **Master Gain** (1→7) - Final output level
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: Disyn 0.4, noise 0.15, formants at default vowel positions

---

### Program 5: Physical Model
**Signal Chain**: Noise → Interface → Delay Lines → Filter (pure PM, no Disyn/formants)

**Use Case**: Classic Karplus-Strong style physical modeling

**Slider Mappings**:
- Slider 1: **Delay1 Feedback** (73→28) - Delay line 1 return
- Slider 2: **Delay2 Feedback** (72→29) - Delay line 2 return
- Slider 3: **Filter Feedback** (28→30) - Filter return
- Slider 4: **Interface Type** (30→24) - 0-11 (Reed, Pluck, Hit, etc.)
- Slider 5: **Intensity** (74→1) - Interface intensity
- Slider 6: **Tuning** (71→26) - Pitch offset
- Slider 7: **Ratio** (1→27) - Delay ratio
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: Noise 0.3, delays 0.4, filter FB 0.3, interface=Reed

---

### Program 6: Full Hybrid (DISABLED)
**Status**: This program is temporarily disabled due to stability issues (segfaults). Selecting program 6 will redirect to program 5.

**Signal Chain**: Disyn + Noise → Formants → Interface → Delay Lines → Filter

**Use Case**: All modules active (maximum flexibility) - currently disabled

**Note**: Slider mappings and default settings are not active. Program 5 settings will be used instead.

---

### Program 7: Disyn Echo
**Signal Chain**: Disyn → Delay Line 1 (single delay, no formants/filter)

**Use Case**: Simple echo/delay effect on distortion synthesis

**Slider Mappings**:
- Slider 1: **Disyn Algorithm** (73→16) - Select algorithm 0-6
- Slider 2: **Disyn Param1** (72→17) - Algorithm-specific parameter 1
- Slider 3: **Disyn Param2** (28→18) - Algorithm-specific parameter 2
- Slider 4: **Disyn Level** (30→19) - Disyn output level
- Slider 5: **Intensity** (74→1) - Interface intensity
- Slider 6: **Tuning** (71→26) - Delay pitch offset (-12 to +12 semitones)
- Slider 7: **Delay1 Feedback** (1→28) - Echo return level
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: Disyn 0.6, Delay1 feedback 0.3, Delay2 off, master gain 0.6

---

## Implementation Details

### Program Change Handler

When a MIDI Program Change is received:
1. Signal chain modules are enabled/disabled
2. Default parameter values are set for the new configuration
3. Slider CC remapping is activated for that program
4. Console prints a banner showing the program name, signal chain, and slider mappings

### Slider Remapping

The `remap_slider_cc()` function translates slider CCs based on `g_current_program`:
- Input: CC number from hardware slider
- Output: Target CC number for the actual parameter
- Sliders 8 and 9 are **always** mapped to Attack (CC 73) and Release (CC 72)
- Sliders 1-7 are context-dependent per program

Example: In Program 0, slider 1 sends CC 73, which is remapped to CC 16 (Disyn Algorithm).

### CC Processing Order

1. MK-449 remapping (if enabled via `FLUES_MK449_MAP`)
2. **Program-based slider remapping** (new)
3. Normal CC parameter mapping

### Startup Behavior

On launch, the synth initializes with **Program 0 (Disyn Direct)** to provide a known starting state.

---

## Usage Examples

### Switching Programs

Send MIDI Program Change 0-7 from your controller or DAW.

**Example MIDI sequence**:
```
Program Change 3    ; Switch to Formant Voice
CC 73 = 64          ; Slider 1 → F1 (Jaw) = mid position
CC 72 = 96          ; Slider 2 → F2 (Tongue) = higher
Program Change 5    ; Switch to Physical Model
CC 73 = 64          ; Slider 1 → Delay1 Feedback (same CC, different function!)
```

### Workflow Tip

Use programs to quickly A/B between synthesis methods:
- **Program 0**: Hear raw Disyn algorithms
- **Program 3**: Hear pure formant synthesis
- **Program 4**: Blend Disyn through formants
- **Program 5**: Classic physical modeling
- **Program 7**: Simple echo effect on Disyn

### Advanced: Per-Program Presets

Since slider mappings change per program, you can save controller snapshots:
- Program 0 snapshot: Dirichlet algorithm with high drive
- Program 3 snapshot: /a/ vowel (F1=700Hz, F2=1200Hz)
- Program 5 snapshot: Reed interface with heavy feedback

---

## Technical Notes

- Program changes are **immediate** (no crossfade)
- All notes are **not** silenced on program change (voices continue)
- Slider values are **not** queried on program change (set sliders after switching)
- Unknown programs (>7) fallback to Program 0
- Console output includes full slider mapping legend for each program

---

## Future Enhancements

- Per-program preset storage (save/recall slider positions)
- Program change velocity for parameter variations
- Bank select for 128 programs (8 banks × 16 programs)
- Smooth crossfade between programs
