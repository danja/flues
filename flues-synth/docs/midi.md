# Flues-Synth MIDI Reference

**Complete MIDI control specification for flues-synth**

## Overview

Flues-synth provides comprehensive MIDI control via:
- **29 MIDI Continuous Controllers (CCs)** for real-time parameter modulation
- **6 Control Notes (36-41)** for toggling signal chain modules
- **30 MIDI Program Changes (0-29)** for switching synthesis configurations
- **MIDI Note On/Off** for monophonic voice triggering (velocity-sensitive)
- **MIDI All Notes Off** for emergency silence

All MIDI input is processed via ALSA sequencer on channel 1 (omni mode planned for future).

---

## MIDI Program Changes

Program Changes dynamically reconfigure the signal chain and remap hardware sliders.

### Program Summary

| Program | Name | Signal Chain | Use Case |
|---------|------|--------------|----------|
| 0 | Disyn Direct | Disyn → Output | Raw distortion synthesis |
| 1 | Disyn + Delays | Disyn → Delays | Echo/resonance |
| 2 | Disyn + Filter | Disyn → Delays → Filter | Filtered distortion |
| 3 | Formant Voice | Noise → Formants | Pure vocal synthesis |
| 4 | Hybrid Speech | Disyn + Noise → Formants | Voiced speech |
| 5 | Physical Model | Noise → Interface → Delays → Filter | Classic PM |
| 6 | Full Hybrid | Disyn + Noise → Formants → Interface → Delays → Filter | Maximum flexibility |
| 7 | Disyn Echo | Disyn → Delay1 | Simple delay effect |

### Hardware Slider Remapping

When using a MIDI controller with 9 sliders (CC: 73, 72, 28, 30, 74, 71, 1, 27, 7):
- **Sliders 1-7**: Context-dependent (change function per program)
- **Sliders 8-9**: Always Attack (CC 73) and Release (CC 72)

**Example**: In Program 0, Slider 1 (CC 73) controls Disyn Algorithm. In Program 3, Slider 1 (CC 73) controls F1 (Jaw).

See [PROGRAM_CHANGE.md](PROGRAM_CHANGE.md) for complete slider mappings per program.

---

## MIDI Continuous Controllers (CCs)

### Standard Controls (2 parameters)

| CC | Parameter | Range | Mapping | Default | Description |
|----|-----------|-------|---------|---------|-------------|
| 1 | Intensity | 0.0–1.0 | Linear | 0.5 | Interface intensity (excitation strength) |
| 7 | Master Gain | 0.0–1.0 | Linear | 0.5 | Final output level |

### Disyn Source (7 parameters)

| CC | Parameter | Range | Mapping | Default | Description |
|----|-----------|-------|---------|---------|-------------|
| 16 | Algorithm | 0–17 | Discrete | 0 | **Primitive:** 0=Dirichlet, 1=DSF Single, 2=DSF Double, 3=Tanh Square, 4=Tanh Saw, 5=PAF, 6=Modified FM<br>**Combinations:** 7=Hybrid Formant, 8=Cascaded, 9=Parallel Bank, 10=Feedback, 11=Morphing, 12=Inharmonic, 13=Adaptive Filter<br>**Novel:** 14=Multi-Stage, 15=Freq Asymmetry, 16=Cross-Mod, 17=Taylor Series |
| 17 | Param1 | 0.0–1.0 | Linear | 0.5 | Algorithm-specific parameter 1 (varies per algorithm, see [algorithms.md](algorithms.md)) |
| 18 | Param2 | 0.0–1.0 | Linear | 0.5 | Algorithm-specific parameter 2 (varies per algorithm, see [algorithms.md](algorithms.md)) |
| 19 | Param3 | 0.0–1.0 | Linear | 0.5 | Algorithm-specific parameter 3 (used by algorithms 7-17, see [algorithms.md](algorithms.md)) |
| 20 | Noise Level | 0.0–1.0 | Linear | 0.15 | White noise generator level |
| 21 | DC Level | 0.0–1.0 | Linear | 0.0 | DC offset source level |

**Note**: CC 19 was repurposed from "Disyn Level" to "Param3" to enable 3-parameter control of Combination and Novel algorithms (7-17). Use CC 7 (Master Gain) for level control instead.

### Formants (4 parameters)

| CC | Parameter | Range | Mapping | Default | Description |
|----|-----------|-------|---------|---------|-------------|
| 71 | F1 (Jaw) | 200–1000 Hz | Exponential | 500 Hz | First formant (jaw position) |
| 10 | F2 (Tongue) | 500–3000 Hz | Exponential | 1500 Hz | Second formant (tongue position) |
| 74 | F3 (Lips) | 1500–4000 Hz | Exponential | 2500 Hz | Third formant (lip rounding) |
| 75 | F4 (Quality) | 2500–4500 Hz | Exponential | 3500 Hz | Fourth formant (voice quality) |

### Vocal Modes (4 toggles)

| CC | Parameter | Values | Mapping | Default | Description |
|----|-----------|--------|---------|---------|-------------|
| 80 | Nasal | 0–127 | Boolean (≥64 = ON) | OFF | Nasal resonance at 250 Hz |
| 81 | Sing | 0–127 | Boolean (≥64 = ON) | OFF | Vibrato (5.5 Hz, ±1.5%) |
| 82 | Shout | 0–127 | Boolean (≥64 = ON) | OFF | Formant boost (+15%) |
| 83 | Fry | 0–127 | Boolean (≥64 = ON) | OFF | Vocal fry (f₀/2 subharmonics) |

### Interface & Delay (3 parameters)

| CC | Parameter | Range | Mapping | Default | Description |
|----|-----------|-------|---------|---------|-------------|
| 24 | Interface Type | 0–11 | Discrete | 2 (Reed) | 0=Pluck, 1=Hit, 2=Reed, 3=Flute, 4=Brass, 5=Bow, 6=Bell, 7=Drum, 8=Crystal, 9=Vapor, 10=Quantum, 11=Plasma |
| 26 | Tuning | -12–+12 st | Linear | 0 st | Pitch offset (semitones) |
| 27 | Ratio | 0.5–2.0 | Exponential | 1.0 | Delay line 2 frequency ratio |

### Feedback (3 parameters)

| CC | Parameter | Range | Mapping | Default | Description |
|----|-----------|-------|---------|---------|-------------|
| 28 | Delay1 Feedback | 0.0–1.0 | Linear | 0.2 | Delay line 1 return amount |
| 29 | Delay2 Feedback | 0.0–1.0 | Linear | 0.2 | Delay line 2 return amount |
| 30 | Filter Feedback | 0.0–1.0 | Linear | 0.1 | SVF output return amount |

### Filter (3 parameters)

| CC | Parameter | Range | Mapping | Default | Description |
|----|-----------|-------|---------|---------|-------------|
| 32 | Frequency | 20–20000 Hz | Exponential | 2000 Hz | SVF cutoff frequency |
| 33 | Q | 0.1–10.0 | Exponential | 1.0 | SVF resonance (quality factor) |
| 34 | Shape | 0.0–1.0 | Linear | 0.0 | Filter morph: 0=LP, 0.5=BP, 1.0=HP |

### Envelope (2 parameters)

| CC | Parameter | Range | Mapping | Default | Description |
|----|-----------|-------|---------|---------|-------------|
| 73 | Attack | 0.001–1.0 s | Exponential | 0.01 s | Envelope attack time |
| 72 | Release | 0.01–3.0 s | Exponential | 0.05 s | Envelope release time |

### Modulation (2 parameters)

| CC | Parameter | Range | Mapping | Default | Description |
|----|-----------|-------|---------|---------|-------------|
| 36 | LFO Frequency | 0.1–20 Hz | Exponential | 5.0 Hz | LFO rate |
| 37 | AM↔FM Depth | -1.0–+1.0 | Bipolar | 0.0 | Negative=AM, Positive=FM |

---

## Control Notes (36-41)

MIDI notes 36-41 toggle signal chain modules on/off (velocity ignored).

| Note | Function | Description |
|------|----------|-------------|
| 36 (C2) | Toggle Noise | Enable/disable noise generator |
| 37 (C#2) | Toggle Disyn | Enable/disable Disyn oscillator |
| 38 (D2) | Toggle Feedback | Enable/disable feedback loop (delays + filter) |
| 39 (D#2) | Toggle Formants | Enable/disable formant cascade |
| 40 (E2) | Toggle Filter | Enable/disable SVF filter |
| 41 (F2) | Hard Mute | Emergency mute (clears delays, DC blockers, filters) |

**Usage**: Send Note On to toggle state. Note Off is ignored.

**Example**:
```
Note On 36 (velocity 64)  → Noise toggles OFF
Note On 36 (velocity 64)  → Noise toggles ON
Note On 41 (velocity 127) → Hard mute activated (clears all state)
```

---

## MIDI Note On/Off

**Monophonic voice triggering** (polyphony planned for Phase 2).

- **Note On**: Triggers envelope, sets pitch (A440 reference)
- **Note Off**: Releases envelope
- **Velocity**: Scales initial envelope output (0-127 → 0.0-1.0)
- **Range**: Any MIDI note (0-127), musically useful range C2-C6

**Example**:
```
Note On 60 (C4), velocity 96  → Trigger note at middle C, 75% velocity
Note Off 60                   → Release envelope
```

---

## MIDI All Notes Off

**CC 123** sends emergency All Notes Off message, silencing the synth immediately.

---

## MK-449 Remapping (Optional)

If `FLUES_MK449_MAP` environment variable is set, CCs are remapped for Arturia MK-449 keyboard:

| MK-449 CC | Remapped To | Parameter |
|-----------|-------------|-----------|
| 91 | 28 | Delay1 Feedback |
| 92 | 29 | Delay2 Feedback |
| 93 | 30 | Filter Feedback |
| 84 | 27 | Delay Ratio |
| 12 | 20 | Noise Level |
| 13 | 19 | Disyn Level |
| 5 | 1 | Intensity |

**Enable**: `FLUES_MK449_MAP=1 ./builddir/flues-synth`

---

## CC Processing Order

1. **ALSA Sequencer Input** → Receives raw MIDI events
2. **MK-449 Remapping** (if enabled) → Translates controller-specific CCs
3. **Program-Based Slider Remapping** → Context-dependent slider mappings
4. **Parameter Mapping** → Converts CC 0-127 to parameter ranges
5. **Engine Update** → Applies value to DSP module

---

## Exponential Mapping Details

Parameters marked "Exponential" use perceptually-linear scaling:

**Formula**: `value = min × (max/min)^(cc/127)`

**Examples**:
- **F1 (CC 71)**: CC 0 = 200 Hz, CC 64 = 447 Hz, CC 127 = 1000 Hz
- **Filter Freq (CC 32)**: CC 0 = 20 Hz, CC 64 = 632 Hz, CC 127 = 20000 Hz
- **LFO Freq (CC 36)**: CC 0 = 0.1 Hz, CC 64 = 1.4 Hz, CC 127 = 20 Hz

---

## Bipolar Mapping Details

AM↔FM Depth (CC 37) uses bipolar mapping:

**Formula**: `depth = (cc/127 - 0.5) × 2`

**Examples**:
- CC 0 → -1.0 (full AM)
- CC 64 → 0.0 (no modulation)
- CC 127 → +1.0 (full FM)

---

## Boolean Mapping Details

Vocal mode toggles (CC 80-83):

**Rule**: `value ≥ 64 → ON, value < 64 → OFF`

**Examples**:
- CC 80 = 0 → Nasal OFF
- CC 80 = 64 → Nasal ON
- CC 80 = 127 → Nasal ON

---

## MIDI Debug Mode

Enable verbose MIDI event logging:

```bash
FLUES_MIDI_DEBUG=1 ./builddir/flues-synth
```

**Output**:
```
MIDI DBG: ch1 NOTE ON   60 vel 96 (src 128:0)
MIDI DBG: ch1 CC  71 = 63 (src 128:0)
CC 71: F1 (Jaw) = 447.21 Hz
MIDI DBG: ch1 PROGRAM CHANGE   3 (src 128:0)
Program 3: Formant Voice
```

---

## Web UI Integration

The `flues-synth/web-ui` provides browser-based control using WebSocket → ALSA MIDI bridge.

**CC Mapping**: Identical to engine (see `web-ui/client/src/utils/parameterMaps.js`)

**Features**:
- Visual knobs/sliders for all 29 parameters
- On-screen keyboard (C4-C5) for quick testing
- Real-time waveform + spectrum visualization
- Program change selector (0-7)

See [web-ui/README.md](../web-ui/README.md) for setup instructions.

---

## MIDI Implementation Chart

```
Function                      Transmitted   Recognized   Remarks
═══════════════════════════════════════════════════════════════════
Basic Channel    Default      —             1
                 Changed      —             1-16         Future: omni
Mode             Default      —             Mode 3       Poly (mono for now)
                 Messages     —             ×
                 Altered      —             ×
Note Number                   —             0-127        True note
                 True Voice   —             0-127        A440 reference
Velocity         Note On      —             ○            Scales envelope
                 Note Off     —             ×            Ignored
Aftertouch       Keys         —             ×
                 Channel      —             ×            Future: expression
Pitch Bend                    —             ×            Future: per-note bend
Control Change                —             ○            29 CCs (see table)
Program Change                —             ○            0-7 (see table)
System Exclusive              —             ×
System Common    Song Pos     —             ×
                 Song Sel     —             ×
                 Tune         —             ×
System Real Time Clock        —             ×
                 Commands     —             ×
Auxiliary        All Notes Off —            ○            CC 123
Messages         Active Sense —             ×
                 Reset        —             ×

Legend:
○ = Implemented
× = Not implemented
— = Not applicable
```

---

## Future Enhancements

- **4-voice polyphony** with independent envelopes and voice stealing
- **MPE support** for polyphonic expression (per-note pitch bend, timbre)
- **MIDI Learn** for dynamic CC assignment
- **Aftertouch** for pressure-sensitive modulation
- **Pitch Bend** for smooth glide and vibrato
- **SysEx** for preset storage and recall
- **MIDI Clock** for tempo-synced LFO and delays

---

## Quick Reference

**Most Common CCs**:
- **1** = Intensity (interface excitation)
- **7** = Master Gain (output level)
- **71-75** = Formants F1-F4 (vowel shaping)
- **73/72** = Attack/Release (envelope)
- **28-30** = Feedback (delays + filter returns)
- **32-34** = Filter (frequency, Q, shape)

**Program Changes**:
- **0** = Disyn Direct (raw distortion)
- **3** = Formant Voice (pure speech)
- **5** = Physical Model (classic PM)
- **7** = Disyn Echo (simple delay)

**Emergency**:
- **Note 41** = Hard Mute (clears all state)
- **CC 123** = All Notes Off

---

**Last Updated**: 2025-12-11
**Version**: Phase 1 (monophonic, 29 CCs, 8 programs, 6 control notes)
