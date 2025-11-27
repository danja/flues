# Vocal Modes Implementation

## Overview

Advanced vocal modes have been added to the Chatterbox speech synthesizer, expanding the original formant-based synthesis with realistic vocal effects and expressive controls.

## Implemented Features

### 1. Nasal Mode

**Purpose:** Add nasal resonance to simulate sounds like French nasal vowels or English nasalized vowels.

**Implementation:**
- Adds a parallel 5th formant at 250 Hz with 100 Hz bandwidth
- Mixed at 30% level with the main formant cascade
- Located in `FormantBankModule.js`:
  ```javascript
  this.nasalFormant = new FormantModule(sampleRate);
  this.nasalFormant.setFrequency(250); // Typical nasal formant
  this.nasalFormant.setBandwidth(100); // Wide bandwidth
  ```

**Signal Flow:**
```
Input → F1 → F2 → F3 → F4 (cascade)
     ↘ Nasal Formant → 0.3x → Sum → Output
```

**UI:** Checkbox labeled "Nasal"

---

### 2. Sing Mode (Vibrato)

**Purpose:** Add natural-sounding vibrato for singing voice simulation.

**Implementation:**
- 5.5 Hz LFO modulating pitch
- ±1.5% pitch deviation (realistic vocal vibrato range)
- Continuous sinusoidal modulation
- Located in `LarynxModule.js`:
  ```javascript
  this.vibratoRate = 5.5; // Hz
  this.vibratoDepth = 0.015; // ±1.5%

  if (this.vibratoEnabled) {
    this.vibratoPhase += this.vibratoRate / this.sampleRate;
    const vibrato = Math.sin(TWO_PI * this.vibratoPhase);
    currentFreq *= 1.0 + vibrato * this.vibratoDepth;
  }
  ```

**Characteristics:**
- Smooth, natural-sounding pitch oscillation
- Independent phase accumulator for continuous vibrato
- Reset on note-on for consistent behavior

**UI:** Checkbox labeled "Sing (Vibrato)"

---

### 3. Shout Mode

**Purpose:** Simulate louder, more energetic speech with formant "shouting shift".

**Implementation:**
- Increases all formant frequencies by 15% (models vocal tract tension)
- Boosts noise level by 50% (adds breathiness)
- Dynamically updates formant frequencies in real-time
- Located in `chatterbox-worklet.js`:
  ```javascript
  updateFormants() {
    const shoutMultiplier = this.shout ? 1.15 : 1.0;
    for (let i = 0; i < this.baseFormants.length; i++) {
      const freq = this.baseFormants[i].frequency * shoutMultiplier;
      this.formantBank.setFormant(i, freq, this.baseFormants[i].bandwidth);
    }
  }
  ```

**Acoustic Rationale:**
- Higher formants = brighter, more forward sound
- Increased aspiration = more air pressure
- Mimics natural vocal effort increase

**UI:** Checkbox labeled "Shout"

---

### 4. Fry Mode (Vocal Fry)

**Purpose:** Simulate creaky voice/vocal fry register (subharmonic phonation).

**Implementation:**
- Adds subharmonic oscillator at half the fundamental frequency
- Mixed at 30% level with main larynx signal
- Independent phase accumulator for subharmonic
- Located in `LarynxModule.js`:
  ```javascript
  if (this.fryEnabled) {
    this.fryPhase += (currentFreq * 0.5) / this.sampleRate; // Octave below
    const fry = (this.fryPhase * 2 - 1) * 0.3; // Attenuated subharmonic
    shaped += fry;
  }
  ```

**Characteristics:**
- Creates characteristic "creaky" sound
- Octave-below subharmonic (f₀/2)
- Maintained through pitch changes

**UI:** Checkbox labeled "Fry (Vocal Fry)"

---

### 5. Stress Control

**Purpose:** Dynamic loudness control with natural saturation at high levels.

**Implementation:**
- Maps slider (0-1) to gain range (0.5-2.0x)
- Applies soft clipping via `tanh()` for stress > 0.6
- Simulates natural vocal strain at loud volumes
- Located in `chatterbox-worklet.js`:
  ```javascript
  // Apply stress (amplitude + distortion)
  const stressGain = 0.5 + this.stress * 1.5;
  sample *= stressGain;

  // Add soft clipping for high stress values
  if (this.stress > 0.6) {
    const drive = (this.stress - 0.6) * 5; // 0-2 drive
    sample = Math.tanh(sample * (1 + drive));
  }
  ```

**Stress Levels:**
- 0.0-0.3: Soft (0.5-0.95x gain)
- 0.3-0.5: Relaxed (0.95-1.25x gain, no clipping)
- 0.5-0.7: Normal (1.25-1.55x gain, mild clipping)
- 0.7-0.9: Loud (1.55-1.85x gain, moderate clipping)
- 0.9-1.0: Very Loud (1.85-2.0x gain, heavy clipping)

**UI:** Slider labeled "Stress" with text readout (Soft/Relaxed/Normal/Loud/Very Loud)

---

## Signal Flow with Vocal Modes

```
┌─────────────────────────────────────────────────┐
│ EXCITATION SOURCES                              │
│                                                 │
│  Larynx (sawtooth)                              │
│    ├─ Vibrato LFO (Sing mode)                  │
│    └─ Subharmonic (Fry mode)                   │
│                                                 │
│  Aspirator (noise)                              │
│    └─ Level boosted (Shout mode)               │
└──────────────────┬──────────────────────────────┘
                   ↓
┌─────────────────────────────────────────────────┐
│ FORMANT BANK                                    │
│                                                 │
│  Main Cascade: F1 → F2 → F3 → F4              │
│    └─ Frequencies boosted 15% (Shout mode)     │
│                                                 │
│  Nasal Formant (250 Hz, parallel)              │
│    └─ Mixed at 30% (Nasal mode)                │
└──────────────────┬──────────────────────────────┘
                   ↓
┌─────────────────────────────────────────────────┐
│ ENVELOPE & DYNAMICS                             │
│                                                 │
│  Attack/Release Envelope                        │
│                                                 │
│  Stress Processing                              │
│    ├─ Gain: 0.5-2.0x                           │
│    └─ Soft clipping (tanh) for stress > 0.6   │
└──────────────────┬──────────────────────────────┘
                   ↓
                Reverb
                   ↓
                Output
```

---

## Testing

All vocal modes have comprehensive test coverage in `tests/vocal-modes.spec.js`:

### Test Suites

1. **LarynxModule - Vocal Fry**
   - Verifies subharmonic generation
   - Tests fry frequency (f₀/2)
   - Validates energy differences

2. **LarynxModule - Vibrato**
   - Confirms pitch modulation
   - Tests phase reset behavior
   - Verifies continuous LFO operation

3. **FormantBankModule - Nasal Mode**
   - Tests nasal formant enable/disable
   - Validates 250 Hz resonance
   - Checks parallel mixing

4. **Stress Processing**
   - Verifies gain mapping (0-1 → 0.5-2.0x)
   - Tests soft clipping threshold
   - Validates tanh saturation

5. **Shout Mode**
   - Confirms 15% formant boost
   - Tests noise level increase
   - Validates real-time updates

6. **Integration Tests**
   - Multiple simultaneous modes
   - Full synthesis chain
   - Signal integrity checks

**Total:** 41 tests passing ✓

---

## Usage Examples

### Natural Speech
```
Voiced: ✓
Aspirated: ✗
Nasal: ✗
Sing: ✗
Shout: ✗
Fry: ✗
Stress: 50% (Normal)
```

### Singing Voice
```
Voiced: ✓
Aspirated: ✗
Nasal: ✗
Sing: ✓ (adds vibrato)
Shout: ✗
Fry: ✗
Stress: 60% (Normal-Loud)
```

### Nasal Vowels (French)
```
Voiced: ✓
Aspirated: ✗
Nasal: ✓ (adds 250 Hz resonance)
Sing: ✗
Shout: ✗
Fry: ✗
Stress: 50%
```

### Creaky Voice
```
Voiced: ✓
Aspirated: ✗
Nasal: ✗
Sing: ✗
Shout: ✗
Fry: ✓ (adds subharmonics)
Stress: 40% (Relaxed)
```

### Shouting
```
Voiced: ✓
Aspirated: ✓ (adds breathiness)
Nasal: ✗
Sing: ✗
Shout: ✓ (raises formants, boosts noise)
Fry: ✗
Stress: 80% (Loud with distortion)
```

### Whisper
```
Voiced: ✗
Aspirated: ✓
Nasal: ✗
Sing: ✗
Shout: ✗
Fry: ✗
Stress: 30% (Soft)
```

---

## Technical Details

### Parameter Ranges

| Parameter | Range | Unit | Mapping |
|-----------|-------|------|---------|
| Pitch | 80-400 | Hz | Exponential |
| Vibrato Rate | 5.5 | Hz | Fixed |
| Vibrato Depth | ±1.5 | % | Fixed |
| Nasal Frequency | 250 | Hz | Fixed |
| Nasal Bandwidth | 100 | Hz | Fixed |
| Nasal Mix | 30 | % | Fixed |
| Fry Frequency | f₀/2 | Hz | Tracks pitch |
| Fry Mix | 30 | % | Fixed |
| Shout Formant Boost | +15 | % | Fixed |
| Shout Noise Boost | +50 | % | Fixed |
| Stress Gain | 0.5-2.0 | x | Linear |
| Stress Clipping Threshold | 0.6 | normalized | Fixed |

### DSP Specifications

- **Sample Rate:** 48000 Hz (typical)
- **Formant Filters:** Biquad bandpass (Q-based design)
- **Cascade Gain:** 8.0x makeup gain
- **Vibrato Waveform:** Sine wave (smooth modulation)
- **Fry Waveform:** Sawtooth (matches larynx)
- **Clipping Function:** `tanh()` soft saturation
- **Nasal Topology:** Parallel (not in cascade)

---

## References

### Speech Synthesis
- Fant, G. (1960). *Acoustic Theory of Speech Production*
- Stevens, K. N. (1998). *Acoustic Phonetics*
- Klatt, D. H. (1980). "Software for a cascade/parallel formant synthesizer"

### Vocal Effects
- Titze, I. R. (2000). *Principles of Voice Production* (vocal fry, vibrato)
- Sundberg, J. (1987). *The Science of the Singing Voice* (shouting formant shift)
- Chen, S. H., et al. (2007). "Acoustic characteristics of vocal fry"

### Related Projects
- [Pink Trombone](https://dood.al/pinktrombone/) - Articulatory speech synthesis
- [Praat](https://www.fon.hum.uva.nl/praat/) - Speech analysis with formant tracking
- Klatt Synthesizer - Classic formant synthesis with parallel nasal path

---

## Future Enhancements

Potential additions for future versions:

1. **Breathiness Control:** Independent aspiration noise filtering
2. **Tremolo:** Amplitude modulation (separate from vibrato)
3. **Glottal Tenseness:** Control waveshaping amount in larynx
4. **Formant Bandwidth Modulation:** Dynamic bandwidth for breathy/tense voices
5. **Pitch Glides:** Automatic portamento between notes
6. **Voice Age Simulation:** Formant frequency scaling for child/adult/elderly
7. **Diplophonia:** Multiple fundamental frequencies (pathological voice)
8. **Spectral Tilt Control:** High-frequency rolloff adjustment

---

## Changelog

### 2025-01 - Vocal Modes Release

**Added:**
- Nasal mode (250 Hz parallel formant)
- Sing mode (5.5 Hz vibrato)
- Shout mode (15% formant boost + noise increase)
- Fry mode (octave-below subharmonics)
- Stress slider (0.5-2.0x gain with soft clipping)

**Modified:**
- `LarynxModule`: Added vibrato and vocal fry processing
- `FormantBankModule`: Added nasal formant and parallel mixing
- `chatterbox-worklet.js`: Added stress processing and shout mode formant updates
- UI: Updated Voice Modes panel with working controls

**Tests:**
- Added `tests/vocal-modes.spec.js` (12 new tests)
- Total test count: 41 passing

**Documentation:**
- Updated README.md with vocal modes features
- Updated CLAUDE.md with implementation details
- Created VOCAL-MODES.md (this document)

---

*Chatterbox Speech Synthesizer - Part of the Flues Audio Experiments Project*
