# Flues-Synth MIDI Program Change System

## Overview

MIDI Program Change messages (0-7) configure the signal chain routing and dynamically remap the 9 hardware sliders to the most relevant parameters for each program.

## Hardware Slider Mapping

The MIDI controller has 9 sliders sending these CC numbers:
1. **Slider 1**: CC 73 (Attack)
2. **Slider 2**: CC 72 (Release)
3. **Slider 3**: CC 28
4. **Slider 4**: CC 30
5. **Slider 5**: CC 74 (F3/Lips)
6. **Slider 6**: CC 71 (F1/Jaw)
7. **Slider 7**: CC 1 (Intensity/Mod Wheel)
8. **Slider 8**: CC 27 (Delay Ratio)
9. **Slider 9**: CC 7 (Master Gain)

**Note**: Sliders 1, 2, and 9 (Attack, Release, Master) are **not remapped** - they always control the same parameters across all programs.

## Programs

### Program 0: Disyn Direct
**Signal Chain**: Disyn → Output (all other processing bypassed)

**Use Case**: Raw distortion synthesis, no formants, delays, or filtering

**Slider Mappings**:
- Slider 1: Attack (73)
- Slider 2: Release (72)
- Slider 3: **Disyn Algorithm** (28→16) - Select algorithm 0-6
- Slider 4: **Disyn Param1** (30→17) - Algorithm-specific parameter 1
- Slider 5: **Disyn Param2** (74→18) - Algorithm-specific parameter 2
- Slider 6: **Disyn Level** (71→19) - Disyn output level
- Slider 7: Intensity (1)
- Slider 8: Delay Ratio (27)
- Slider 9: Master Gain (7)

**Default Settings**: Disyn level 0.8, master gain 0.7

---

### Program 1: Disyn + Delays
**Signal Chain**: Disyn → Delay Lines (feedback enabled, no formants/filter)

**Use Case**: Add echo/resonance to Disyn without formants

**Slider Mappings**:
- Slider 1: Attack (73)
- Slider 2: Release (72)
- Slider 3: **Delay1 Feedback** (28) - Delay line 1 return
- Slider 4: **Delay2 Feedback** (30→29) - Delay line 2 return
- Slider 5: **Filter Feedback** (74→30) - Filter return (usually 0)
- Slider 6: **Disyn Level** (71→19)
- Slider 7: Intensity (1)
- Slider 8: Delay Ratio (27)
- Slider 9: Master Gain (7)

**Default Settings**: Disyn 0.6, Delay1/2 feedback 0.3

---

### Program 2: Disyn + Filter
**Signal Chain**: Disyn → Delay Lines → Filter (full chain except formants)

**Use Case**: Shape Disyn with state-variable filter

**Slider Mappings**:
- Slider 1: Attack (73)
- Slider 2: Release (72)
- Slider 3: **Filter Frequency** (28→32) - SVF cutoff (20Hz-20kHz)
- Slider 4: **Filter Q** (30→33) - Resonance (0.1-10)
- Slider 5: **Filter Shape** (74→34) - LP→BP→HP morph (0-1)
- Slider 6: **Disyn Level** (71→19)
- Slider 7: Intensity (1)
- Slider 8: Delay Ratio (27)
- Slider 9: Master Gain (7)

**Default Settings**: Disyn 0.5, delays 0.25, filter FB 0.2, freq 1kHz, Q=2

---

### Program 3: Formant Voice
**Signal Chain**: Noise → Formant Cascade (vocal synthesis, no Disyn/delays/filter)

**Use Case**: Pure vocal formant synthesis

**Slider Mappings**:
- Slider 1: Attack (73)
- Slider 2: Release (72)
- Slider 3: **F1 (Jaw)** (28→71) - 200-1000 Hz
- Slider 4: **F2 (Tongue)** (30→10) - 500-3000 Hz
- Slider 5: **F3 (Lips)** (74) - 1500-4000 Hz
- Slider 6: **F4 (Quality)** (71→75) - 2500-4500 Hz
- Slider 7: **Noise Level** (1→20) - Aspirator level
- Slider 8: **Nasal Toggle** (27→80) - ≥64 = nasal resonance ON
- Slider 9: Master Gain (7)

**Default Settings**: Noise 0.5, F1=500Hz, F2=1500Hz, F3=2500Hz, F4=3500Hz

---

### Program 4: Hybrid Speech
**Signal Chain**: Disyn + Noise → Formant Cascade (speech with harmonic excitation)

**Use Case**: Voiced speech (larynx-like excitation through formants)

**Slider Mappings**:
- Slider 1: Attack (73)
- Slider 2: Release (72)
- Slider 3: **F1 (Jaw)** (28→71)
- Slider 4: **F2 (Tongue)** (30→10)
- Slider 5: **F3 (Lips)** (74)
- Slider 6: **F4 (Quality)** (71→75)
- Slider 7: **Disyn Level** (1→19) - Harmonic content
- Slider 8: Delay Ratio (27)
- Slider 9: Master Gain (7)

**Default Settings**: Disyn 0.4, noise 0.15, formants at default vowel positions

---

### Program 5: Physical Model
**Signal Chain**: Noise → Interface → Delay Lines → Filter (pure PM, no Disyn/formants)

**Use Case**: Classic Karplus-Strong style physical modeling

**Slider Mappings**:
- Slider 1: Attack (73)
- Slider 2: Release (72)
- Slider 3: **Delay1 Feedback** (28)
- Slider 4: **Delay2 Feedback** (30→29)
- Slider 5: **Filter Feedback** (74→30)
- Slider 6: **Interface Type** (71→24) - 0-11 (Reed, Pluck, Hit, etc.)
- Slider 7: Intensity (1)
- Slider 8: Delay Ratio (27)
- Slider 9: Master Gain (7)

**Default Settings**: Noise 0.3, delays 0.4, filter FB 0.3, interface=Reed

---

### Program 6: Full Hybrid
**Signal Chain**: Disyn + Noise → Formants → Interface → Delay Lines → Filter

**Use Case**: All modules active (maximum flexibility)

**Slider Mappings**:
- Slider 1: Attack (73)
- Slider 2: Release (72)
- Slider 3: **Delay1 Feedback** (28)
- Slider 4: **Delay2 Feedback** (30→29)
- Slider 5: **Filter Feedback** (74→30)
- Slider 6: **Interface Type** (71→24)
- Slider 7: Intensity (1)
- Slider 8: Delay Ratio (27)
- Slider 9: Master Gain (7)

**Default Settings**: Disyn 0.3, noise 0.15, formants default, delays 0.35, filter FB 0.25, interface=Reed

---

### Program 7: Experimental
**Signal Chain**: Disyn → Interface → Delays + Formants → Filter (formants in feedback loop)

**Use Case**: Resonant formant coloring in the feedback path

**Slider Mappings**:
- Slider 1: Attack (73)
- Slider 2: Release (72)
- Slider 3: **F1 (Jaw)** (28→71)
- Slider 4: **F2 (Tongue)** (30→10)
- Slider 5: **Delay1 Feedback** (74→28)
- Slider 6: **Interface Type** (71→24)
- Slider 7: Intensity (1)
- Slider 8: Delay Ratio (27)
- Slider 9: Master Gain (7)

**Default Settings**: Disyn 0.35, noise 0.1, delays 0.3, filter FB 0.2, interface=Flute

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

Example: In Program 0, slider 3 sends CC 28, which is remapped to CC 16 (Disyn Algorithm).

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
CC 28 = 64          ; Slider 3 → F1 (Jaw) = mid position
CC 30 = 96          ; Slider 4 → F2 (Tongue) = higher
Program Change 6    ; Switch to Full Hybrid
CC 28 = 64          ; Slider 3 → Delay1 Feedback (same CC, different function!)
```

### Workflow Tip

Use programs to quickly A/B between synthesis methods:
- **Program 0**: Hear raw Disyn algorithms
- **Program 3**: Hear pure formant synthesis
- **Program 4**: Blend Disyn through formants
- **Program 6**: Full hybrid with all processing

### Advanced: Per-Program Presets

Since slider mappings change per program, you can save controller snapshots:
- Program 0 snapshot: Dirichlet algorithm with high drive
- Program 3 snapshot: /a/ vowel (F1=700Hz, F2=1200Hz)
- Program 6 snapshot: Reed interface with heavy feedback

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
