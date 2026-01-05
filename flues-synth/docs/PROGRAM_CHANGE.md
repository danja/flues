# Flues-Synth MIDI Program Change System

## Overview

MIDI Program Change messages (0-28) configure the signal chain routing and dynamically remap the 9 hardware sliders to the most relevant parameters for each program.

### Program Summary

| Program | Name | Signal Chain | Status |
|---------|------|--------------|--------|
| 0 | Disyn Echo | Disyn → Interface → Delay1 | Active |
| 1 | Disyn + Delays | Disyn → Delays | Active |
| 2 | Disyn + Filter | Disyn → Delays → Filter | Active |
| 3 | Formant Voice | Noise → Formants | Active |
| 4 | Hybrid Speech | Disyn + Noise → Formants | Active |
| 5 | Physical Model | Noise → Interface → Delays → Filter | Active |
| 6 | Full Hybrid | Disyn + Noise → Formants → Interface → Delays → Filter | Active |
| 7 | Disyn Direct | Disyn → Output | Active |
| 8 | ModFM Formant | ModFM → Formants → Output | Active |
| 9 | DSF Inharmonic | DSF Single → Delays → Output | Active |
| 10 | PAF Direct | PAF → Filter → Output | Active |
| 11 | Cascaded DSF+PAF | DSF Double → Formants → Output | Active |
| 12 | Tanh Spectral | Tanh Saw → Filter → Feedback → Output | Active |
| 13 | Hybrid DSF→Formant | DSF Single → Formants → Output | Active |
| 14 | Feedback ModFM | ModFM → Delays → Filter → Feedback → Output | Active |
| 15 | Dirichlet Explorer | Dirichlet → Filter → Output | Active |
| 16 | Multi-Algorithm | Disyn (any) → Output | Active |
| 17 | Spectral Sculptor | DSF Double → Filter → Feedback → Output | Active |
| 18 | Hybrid Formant Engine | ModFM → Formants (F1-F4) → Output | Active |
| 19 | Cascaded Spectral Sculptor | DSF Single → Filter → Feedback → Output | Active |
| 20 | Parallel Formant Bank | ModFM + Noise → Formants → Output | Active |
| 21 | Feedback Loop Network | ModFM → Delays → Filter → Heavy Feedback | Active |
| 22 | Morphing Spectral Engine | Algorithm Morph + Formants → Output | Active |
| 23 | Inharmonic Bell Resonator | DSF (golden ratio) → Formants → Delays | Active |
| 24 | Filter Sweep Emulator | ModFM → Filter → LFO Mod → Output | Active |
| 25 | Multi-Stage Waveshaper | Tanh → Filter → Formants → Output | Active |
| 26 | Spectral Animator | Algorithm + Formants → LFO Mod → Output | Active |
| 27 | Feedback Chaos Engine | DSF → Interface → Delays → Filter → Max FB | Active |
| 28 | Vocal Morph Matrix | ModFM → Formants + Vocal Modes → Output | Active |
| 29 | Taylor Series | Taylor → Output (educational/lo-fi) | Active |

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

### Program 1: Disyn + Filter
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

### Program 2: Trajectory Polygon
**Signal Chain**: Trajectory Oscillator → Output (all other processing bypassed)

**Use Case**: Polygonal bounce oscillator with pitch-coupled motion

**Slider Mappings**:
- Slider 1: **Sides** (73→traj) - 3 to 12 edges
- Slider 2: **Start Position** (72→traj) - 0 to 360 degrees
- Slider 3: **Start Angle** (28→traj) - 0 to 360 degrees
- Slider 4: **Master Gain** (30→7)
- Slider 5: **Clip Drive** (74→traj) - Soft clip drive (1.0x-5.0x)
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: 6 sides, start pos 0°, start angle 45°, master gain 0.85, clip drive 1.0x

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

### Program 6: Full Hybrid
**Signal Chain**: Disyn + Noise → Formants → Interface → Delay Lines → Filter

**Use Case**: All DSP modules active for maximum sonic flexibility. Ideal for complex timbres combining distortion synthesis, formant filtering, physical modeling, and effects processing.

**Slider Mappings**:
- Slider 1: **Delay1 Feedback** (73→28) - Delay line 1 return level
- Slider 2: **Delay2 Feedback** (72→29) - Delay line 2 return level
- Slider 3: **Filter Feedback** (28→30) - SVF output return level
- Slider 4: **Interface Type** (30→24) - Physical model type (0-11)
- Slider 5: **Intensity** (74→1) - Interface excitation strength
- Slider 6: **Tuning** (71→26) - Pitch offset (-12 to +12 semitones)
- Slider 7: **Ratio** (1→27) - Delay line 2 frequency ratio
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: Disyn 0.3, Noise 0.2, all feedback 0.2-0.3, Interface=Reed, Master 0.6

**Note**: Re-enabled after race condition fix in interface_module.c (commit 50bb5c0). Tested stable with all modules active simultaneously.

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

### Program 8: ModFM Formant
**Signal Chain**: ModFM (algo 6) → Formants (F1-F4) → Output

**Use Case**: Voice-like lead synthesizer with natural spectral evolution and formant control. Excellent for solo lines with vowel articulation.

**Slider Mappings**:
- Slider 1: **ModFM Index** (73→17) - Modulation brightness (0.01-8)
- Slider 2: **ModFM Ratio** (72→18) - Carrier/modulator ratio (0.25-6)
- Slider 3: **F1 Jaw** (28→71) - First formant 200-1000 Hz
- Slider 4: **F2 Tongue** (30→10) - Second formant 500-3000 Hz
- Slider 5: **F3 Lips** (74→74) - Third formant 1500-4000 Hz
- Slider 6: **F4 Quality** (71→75) - Fourth formant 2500-4500 Hz
- Slider 7: **Master Gain** (1→7) - Output level
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: ModFM (algo 6), Disyn level 0.6, F1=500Hz, F2=1500Hz, F3=2500Hz, F4=3500Hz

---

### Program 9: DSF Inharmonic Explorer
**Signal Chain**: DSF Single (algo 1) → Delay Lines (feedback) → Output

**Use Case**: Bell synthesis, gong sounds, metallic percussion. DSF ratio parameter creates harmonic (1.0) to inharmonic (√2, φ) spectra.

**Slider Mappings**:
- Slider 1: **DSF Decay** (73→17) - Spectral rolloff (0-0.98)
- Slider 2: **DSF Ratio** (72→18) - Harmonic/inharmonic (0.5-4.0)
- Slider 3: **Delay1 Feedback** (28→28) - Primary echo return
- Slider 4: **Delay2 Feedback** (30→29) - Secondary echo return
- Slider 5: **Intensity** (74→1) - Interface intensity
- Slider 6: **Tuning** (71→26) - Delay pitch offset (-12 to +12 semitones)
- Slider 7: **Delay Ratio** (1→27) - Delay line 2 ratio (0.5-2.0×)
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: DSF Single (algo 1), Disyn level 0.6, Delay1 0.4, Delay2 0.2

---

### Program 10: PAF Direct
**Signal Chain**: PAF (algo 5) → Filter (SVF) → Output

**Use Case**: Vowel-like synthesis with filter articulation. PAF creates formant "bump" in spectrum, filter provides additional shaping.

**Slider Mappings**:
- Slider 1: **PAF Formant** (73→17) - Formant center (0.5-6× f0)
- Slider 2: **PAF Bandwidth** (72→18) - Formant width (50-3000 Hz)
- Slider 3: **Filter Frequency** (28→32) - SVF cutoff (20Hz-20kHz)
- Slider 4: **Filter Q** (30→33) - Resonance (0.1-10)
- Slider 5: **Filter Shape** (74→34) - LP→BP→HP morph (0-1)
- Slider 6: **Tuning** (71→26) - Pitch offset
- Slider 7: **Master Gain** (1→7) - Output level
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: PAF (algo 5), Disyn level 0.8, Filter 3kHz Q=2.0 BP

---

### Program 11: Cascaded DSF+PAF
**Signal Chain**: DSF Double (algo 2) → Formants (F1-F4) → Output

**Use Case**: Bell-like timbres with formant resonance. Combining DSF's inharmonic capabilities with formant filtering creates complex, evolving metallic tones.

**Slider Mappings**:
- Slider 1: **DSF Decay** (73→17) - Spectral rolloff
- Slider 2: **DSF Ratio** (72→18) - Inharmonicity (try √2=1.414, φ=1.618)
- Slider 3: **F1 Low Formant** (28→71) - 200-1000 Hz
- Slider 4: **F2 High Formant** (30→10) - 500-3000 Hz
- Slider 5: **F3 Brightness** (74→74) - 1500-4000 Hz
- Slider 6: **Tuning** (71→26) - Pitch offset
- Slider 7: **Master Gain** (1→7) - Output level
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: DSF Double (algo 2), Disyn level 0.5, F1=800Hz, F2=2400Hz, F3=4000Hz

---

### Program 12: Tanh Spectral
**Signal Chain**: Tanh Saw (algo 4) → Filter (SVF) → Feedback → Output

**Use Case**: Aggressive filtered synthesis with feedback resonance. Tanh provides harmonic saturation, filter shapes spectrum, feedback adds metallic character. Perfect for acid-style sounds.

**Slider Mappings**:
- Slider 1: **Tanh Drive** (73→17) - Saturation amount (0.05-4.5)
- Slider 2: **Tanh Blend** (72→18) - Square/saw mix (0-1)
- Slider 3: **Filter Frequency** (28→32) - Cutoff sweep (20Hz-20kHz)
- Slider 4: **Filter Q** (30→33) - Resonance (0.1-10)
- Slider 5: **Filter Shape** (74→34) - LP→BP→HP morph
- Slider 6: **Filter Feedback** (71→30) - Feedback return level
- Slider 7: **Intensity** (1→1) - Interface intensity
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: Tanh Saw (algo 4), Disyn level 0.7, Filter 1.5kHz Q=3.0 BP, Filter FB 0.4

---

### Program 13: Hybrid DSF→Formant
**Signal Chain**: DSF Single (algo 1) → Formants (F1-F4 full cascade) → Output

**Use Case**: Rich vocal synthesis with harmonic source. DSF provides spectrally complex base, formants add vowel character. Excellent for choir-like pads and vocal leads.

**Slider Mappings**:
- Slider 1: **DSF Decay** (73→17) - Harmonic brightness (0-0.98)
- Slider 2: **DSF Ratio** (72→18) - Source character (0.5-4)
- Slider 3: **F1 Jaw** (28→71) - First formant
- Slider 4: **F2 Tongue** (30→10) - Second formant
- Slider 5: **F3 Lips** (74→74) - Third formant
- Slider 6: **F4 Quality** (71→75) - Fourth formant
- Slider 7: **Master Gain** (1→7) - Output level
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: DSF Single (algo 1), Disyn level 0.4, F1=600Hz, F2=1500Hz, F3=3000Hz, F4=4000Hz

---

### Program 14: Feedback ModFM
**Signal Chain**: ModFM (algo 6) → Delays → Filter → Feedback (all returns) → Output

**Use Case**: Metallic bell-like timbres with complex feedback behavior. ModFM's natural evolution + feedback creates unpredictable but musical harmonics. Excellent for FM bells and metallic percussion.

**Slider Mappings**:
- Slider 1: **ModFM Index** (73→17) - Modulation depth (0.01-8)
- Slider 2: **ModFM Ratio** (72→18) - Carrier/mod ratio (0.25-6)
- Slider 3: **Delay1 Feedback** (28→28) - Primary resonance
- Slider 4: **Delay2 Feedback** (30→29) - Secondary resonance
- Slider 5: **Filter Feedback** (74→30) - Filter return
- Slider 6: **Tuning** (71→26) - Delay pitch offset
- Slider 7: **Delay Ratio** (1→27) - Delay 2 ratio
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: ModFM (algo 6), Disyn level 0.5, Delay1 0.5, Delay2 0.3, Filter FB 0.4, Filter 2kHz Q=2.5

---

### Program 15: Dirichlet Explorer
**Signal Chain**: Dirichlet Pulse (algo 0) → Filter (SVF) → Output

**Use Case**: Classic harmonic synthesis with filter control. Windham/Steiglitz pulse train with adjustable bandwidth. Excellent for filter sweeps and resonant sounds.

**Slider Mappings**:
- Slider 1: **Harmonics** (73→17) - Number of partials (1-64)
- Slider 2: **Tilt** (72→18) - Spectral tilt (-3 to +15 dB/oct)
- Slider 3: **Filter Frequency** (28→32) - Cutoff (20Hz-20kHz)
- Slider 4: **Filter Q** (30→33) - Resonance (0.1-10)
- Slider 5: **Filter Shape** (74→34) - LP→BP→HP morph
- Slider 6: **Tuning** (71→26) - Pitch offset
- Slider 7: **Master Gain** (1→7) - Output level
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: Dirichlet (algo 0), Disyn level 0.8, Filter 5kHz Q=1.5 LP

---

### Program 16: Multi-Algorithm Demo
**Signal Chain**: Disyn (any algorithm, selected by slider 1) → Output

**Use Case**: Educational/demo mode. Quickly audition all 7 algorithms with identical parameter control. Useful for sound design and understanding algorithm characteristics.

**Slider Mappings**:
- Slider 1: **Algorithm Selector** (73→16) - 0-6 (Dirichlet, DSF1, DSF2, TanhSq, TanhSaw, PAF, ModFM)
- Slider 2: **Param1** (72→17) - Algorithm-specific parameter 1
- Slider 3: **Param2** (28→18) - Algorithm-specific parameter 2
- Slider 4: **Disyn Level** (30→19) - Output gain
- Slider 5: **Intensity** (74→1) - Interface intensity (bypassed)
- Slider 6: **Tuning** (71→26) - Pitch offset
- Slider 7: **Master Gain** (1→7) - Final output level
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: Disyn level 0.6, all other modules bypassed, compromise gain for all algorithms

---

### Program 17: Spectral Sculptor
**Signal Chain**: DSF Double (algo 2) → Filter (SVF) → Feedback (all returns) → Output

**Use Case**: Dynamic spectral animation for acid-style sounds and evolving timbres. DSF decay parameter acts as "cutoff", ratio adds character, filter provides resonance.

**Slider Mappings**:
- Slider 1: **DSF Decay** (73→17) - Spectral density (acts like Q)
- Slider 2: **DSF Ratio** (72→18) - Harmonic/inharmonic
- Slider 3: **Filter Frequency** (28→32) - Cutoff sweep
- Slider 4: **Filter Q** (30→33) - Resonance
- Slider 5: **Filter Shape** (74→34) - LP→BP→HP morph
- Slider 6: **Delay1 Feedback** (71→28) - Resonance feedback
- Slider 7: **Filter Feedback** (1→30) - Filter return
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: DSF Double (algo 2), Disyn level 0.6, Filter 2kHz Q=2.5 BP, Delay1 0.3, Delay2 0.25, Filter FB 0.35

---

### Program 18: Hybrid Formant Engine
**Signal Chain**: ModFM (algo 6) → Formants (F1-F4) → Output

**Use Case**: Voice-like lead synth with natural FM evolution and formant resonance. Based on Combination 1 from distortion synthesis research. Excellent for vocal pads, evolving leads, and speech-like textures.

**Slider Mappings**:
- Slider 1: **ModFM Index** (73→17) - Brightness/spectral complexity
- Slider 2: **ModFM Ratio** (72→18) - C:M ratio (harmonic relationship)
- Slider 3: **F1 (Jaw)** (28→71) - Formant 1 (200-1000 Hz)
- Slider 4: **F2 (Tongue)** (30→10) - Formant 2 (500-3000 Hz)
- Slider 5: **F3 (Lips)** (74→74) - Formant 3 (1500-4000 Hz)
- Slider 6: **F4 (Quality)** (71→75) - Formant 4 (2500-4500 Hz)
- Slider 7: **Master Gain** (1→7) - Output level
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: ModFM (algo 6), Disyn level 0.5, F1=800Hz F2=1200Hz F3=2400Hz F4=3500Hz, Master 0.7

---

### Program 19: Cascaded Spectral Sculptor
**Signal Chain**: DSF Single (algo 1) → Filter (SVF) → Feedback → Output

**Use Case**: Animated morphing leads with DSF harmonic control + filter shaping. Based on Combination 2. Creates extremely animated timbres for modern synthesis and sound design.

**Slider Mappings**:
- Slider 1: **DSF Decay** (73→17) - Spectral rolloff (sparse to dense)
- Slider 2: **DSF Ratio** (72→18) - Harmonic/inharmonic character
- Slider 3: **Filter Frequency** (28→32) - Cutoff sweep
- Slider 4: **Filter Q** (30→33) - Resonance
- Slider 5: **Filter Shape** (74→34) - LP→BP→HP morph
- Slider 6: **Filter Feedback** (71→30) - Filter return
- Slider 7: **Master Gain** (1→7) - Output level
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: DSF Single (algo 1), Disyn level 0.6, Filter 3kHz Q=3.0, Filter FB 0.4, Master 0.7

---

### Program 20: Parallel Formant Bank
**Signal Chain**: ModFM + Noise → Formants (F1-F4) → Output

**Use Case**: Rich organic complexity with chorused thick timbres. Based on Combination 3 parallel distortion bank. Excellent for polysynth-quality voices, pads, and strings.

**Slider Mappings**:
- Slider 1: **ModFM Index** (73→17) - Brightness control
- Slider 2: **ModFM Ratio** (72→18) - C:M ratio
- Slider 3: **F1 (Jaw)** (28→71) - Formant 1
- Slider 4: **F2 (Tongue)** (30→10) - Formant 2
- Slider 5: **F3 (Lips)** (74→74) - Formant 3
- Slider 6: **F4 (Quality)** (71→75) - Formant 4
- Slider 7: **Noise Level** (1→20) - Breathiness/texture
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: ModFM (algo 6), Disyn 0.4, Noise 0.2, F1=800Hz F2=1500Hz F3=2400Hz F4=3200Hz, Master 0.7

---

### Program 21: Feedback Loop Network
**Signal Chain**: ModFM → Delays → Filter → Heavy Feedback → Output

**Use Case**: Metallic bell-like timbres with chaotic evolution. Based on Combination 4 feedback distortion network. Creates FM-style metallic percussion and aggressive distorted leads.

**Slider Mappings**:
- Slider 1: **ModFM Index** (73→17) - FM depth
- Slider 2: **ModFM Ratio** (72→18) - C:M ratio
- Slider 3: **Delay1 Feedback** (28→28) - First delay return
- Slider 4: **Delay2 Feedback** (30→29) - Second delay return
- Slider 5: **Filter Feedback** (74→30) - Filter return (chaos control)
- Slider 6: **Filter Frequency** (71→32) - Cutoff
- Slider 7: **Filter Q** (1→33) - Resonance
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: ModFM (algo 6), Disyn 0.5, Delay1 0.5, Delay2 0.45, Filter FB 0.5, Filter 2kHz Q=2.0, Master 0.65

---

### Program 22: Morphing Spectral Engine
**Signal Chain**: Algorithm morphing (via slider) + Formants → Output

**Use Case**: Live performance control for exploring timbral space. Based on Combination 5. Allows continuous morphing between all 7 Disyn algorithms with formant coloration.

**Slider Mappings**:
- Slider 1: **Algorithm** (73→16) - Select from 7 algorithms
- Slider 2: **Param1** (72→17) - Algorithm parameter 1
- Slider 3: **Param2** (28→18) - Algorithm parameter 2
- Slider 4: **F1 (Jaw)** (30→71) - Formant 1
- Slider 5: **F2 (Tongue)** (74→10) - Formant 2
- Slider 6: **Intensity** (71→1) - Interface intensity
- Slider 7: **Master Gain** (1→7) - Output level
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: Disyn level 0.5, F1=600Hz F2=1400Hz, Master 0.7

---

### Program 23: Inharmonic Bell Resonator
**Signal Chain**: DSF Single (golden ratio) → Formants → Delays → Output

**Use Case**: Bell synthesis with gong-like metallic percussion. Based on Combination 6. Creates struck/plucked instrument modeling and cinematic metallic textures.

**Slider Mappings**:
- Slider 1: **DSF Decay** (73→17) - Spectral rolloff
- Slider 2: **DSF Ratio** (72→18) - Set to √2 or φ for inharmonicity
- Slider 3: **F1** (28→71) - Low formant (warmth)
- Slider 4: **F3** (30→74) - High formant (brightness)
- Slider 5: **Tuning** (74→26) - Delay pitch offset (-12 to +12)
- Slider 6: **Delay1 Feedback** (71→28) - Sustain/resonance
- Slider 7: **Master Gain** (1→7) - Output level
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: DSF Single (algo 1), Disyn 0.5, F1=800Hz F3=2800Hz, Delay1 0.35, Master 0.7

---

### Program 24: Filter Sweep Emulator
**Signal Chain**: ModFM → Filter → LFO Modulation → Output

**Use Case**: Acid-style sounds with CPU-efficient filtered synthesis. Based on Combination 7 adaptive filter emulation. Replaces actual filter sweeps with spectral distortion.

**Slider Mappings**:
- Slider 1: **Decay/Index** (73→17) - Spectral control
- Slider 2: **Index** (72→17) - Additional brightness (duplicate for sweep)
- Slider 3: **Filter Frequency** (28→32) - Cutoff
- Slider 4: **Filter Q** (30→33) - Resonance
- Slider 5: **LFO Frequency** (74→36) - Modulation rate (0.1-20 Hz)
- Slider 6: **AM↔FM Depth** (71→37) - Bipolar modulation (-1 to +1)
- Slider 7: **Master Gain** (1→7) - Output level
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: ModFM (algo 6), Disyn 0.6, Filter 1.5kHz Q=4.0, LFO 2Hz, AM↔FM 0.3, Master 0.7

---

### Program 25: Multi-Stage Waveshaper
**Signal Chain**: Tanh Saw → Filter (cascade) → Formants (exponential shape) → Output

**Use Case**: Each stage adds different spectral characteristics. Based on Novel Extrapolation 1. Tanh smooths/reduces aliasing, filter shapes, formants add exponential character.

**Slider Mappings**:
- Slider 1: **Tanh Drive** (73→17) - Saturation (0.05-4.5)
- Slider 2: **Tanh Blend** (72→18) - Square/Saw mix (0-1)
- Slider 3: **Filter Frequency** (28→32) - Cascade cutoff
- Slider 4: **Filter Q** (30→33) - Resonance
- Slider 5: **F1** (74→71) - Formant 1 (exponential shaping)
- Slider 6: **F2** (71→10) - Formant 2
- Slider 7: **Master Gain** (1→7) - Output level
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: Tanh Saw (algo 4), Disyn 0.6, Filter 2.5kHz Q=2.0, F1=1000Hz F2=2000Hz, Master 0.7

---

### Program 26: Spectral Animator
**Signal Chain**: Algorithm selection → Formants → LFO Modulation → Output

**Use Case**: Cross-algorithm parameter animation with coupled evolution. Based on Novel Extrapolation 3. Creates organic spectral evolution with related parameter movement.

**Slider Mappings**:
- Slider 1: **Algorithm** (73→16) - Select from 7 algorithms
- Slider 2: **Param1** (72→17) - Algorithm parameter 1
- Slider 3: **F1** (28→71) - Formant 1
- Slider 4: **F2** (30→10) - Formant 2
- Slider 5: **LFO Frequency** (74→36) - Modulation rate
- Slider 6: **AM↔FM Depth** (71→37) - Modulation type/depth
- Slider 7: **Master Gain** (1→7) - Output level
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: Disyn 0.5, F1=700Hz F2=1600Hz, LFO 3Hz, AM↔FM 0.4, Master 0.7

---

### Program 27: Feedback Chaos Engine
**Signal Chain**: DSF → Interface → Delays → Filter → Maximum Feedback → Output

**Use Case**: Unpredictable organic evolution with physical modeling. Experimental program combining distortion synthesis with physical interface and extreme feedback. Creates evolving chaotic textures.

**Slider Mappings**:
- Slider 1: **DSF Decay** (73→17) - Spectral density
- Slider 2: **DSF Ratio** (72→18) - Harmonic character
- Slider 3: **Interface Type** (28→24) - Physical model (0-11)
- Slider 4: **Delay1 Feedback** (30→28) - First delay return
- Slider 5: **Delay2 Feedback** (74→29) - Second delay return
- Slider 6: **Filter Feedback** (71→30) - Filter return (chaos)
- Slider 7: **Intensity** (1→1) - Interface intensity
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: DSF Double (algo 2), Noise 0.15, Interface Reed, Disyn 0.4, Delay1 0.6, Delay2 0.55, Filter FB 0.5, Filter 1.8kHz Q=3.5, Intensity 0.6, Master 0.65

---

### Program 28: Vocal Morph Matrix
**Signal Chain**: ModFM → All 4 Formants + Vocal Modes → Output

**Use Case**: Complete vocal synthesis with singing and expression. Experimental program combining ModFM with full formant cascade and vocal mode toggles (Nasal, Sing, Shout).

**Slider Mappings**:
- Slider 1: **ModFM Index** (73→17) - Vocal brightness
- Slider 2: **ModFM Ratio** (72→18) - Voice character
- Slider 3: **F1 (Jaw)** (28→71) - Vowel height
- Slider 4: **F2 (Tongue)** (30→10) - Vowel frontness
- Slider 5: **Nasal** (74→80) - Nasal resonance (toggle ≥64)
- Slider 6: **Sing** (71→81) - Vibrato (toggle ≥64)
- Slider 7: **Shout** (1→82) - 15% formant boost (toggle ≥64)
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: ModFM (algo 6), Disyn 0.5, F1=700Hz F2=1220Hz F3=2600Hz F4=3500Hz, Vocal modes off, Master 0.7

---

### Program 29: Taylor Series Approximation

**Signal Chain**: Taylor (Algorithm 17) → Output

**Use Case**: Educational synthesis demonstrating Taylor series convergence and intentional aliasing. Explore the progression from harsh digital artifacts (few terms) to smooth sine waves (many terms). Blend fundamental and second harmonic for timbral variation.

**Slider Mappings**:
- Slider 1: **First Terms** (73→17) - Terms for fundamental (1-10)
- Slider 2: **Second Terms** (72→18) - Terms for 2nd harmonic (1-10)
- Slider 3: **Blend** (28→19) - Fundamental/2nd harmonic mix (0-100%)
- Slider 4: **Master Gain** (30→7) - Output level
- Slider 5: (unused)
- Slider 6: (unused)
- Slider 7: (unused)
- Slider 8: **Attack** (27→73)
- Slider 9: **Release** (7→72)

**Default Settings**: Taylor (algo 17), First=5 terms, Second=5 terms, Blend=50%, Disyn 0.7, Master 0.7

**Sound Design Tips**:
- **1-2 terms**: Harsh sawtooth-like, severe aliasing (lo-fi aesthetic)
- **3-4 terms**: Soft-clipped sine, noticeable harmonics
- **5-7 terms**: Good approximation with subtle aliasing
- **8-10 terms**: Nearly perfect sine wave
- **Blend control**: 0% = fundamental only, 50% = equal mix, 100% = octave up

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
- Unknown programs (>28) fallback to Program 0
- Console output includes full slider mapping legend for each program

---

## Future Enhancements

- Per-program preset storage (save/recall slider positions)
- Program change velocity for parameter variations
- Bank select for 128 programs (8 banks × 16 programs)
- Smooth crossfade between programs
