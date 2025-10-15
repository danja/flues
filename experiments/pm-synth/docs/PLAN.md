# Physical Modelling Synth Implementation Plan

## Overview
Transform the pm-synth from a clarinet-specific synth into a general-purpose physical modeling synthesizer with modular architecture supporting multiple instrument types (pluck, hit, reed, flute, brass) and enhanced control capabilities.

## Architecture Transformation

### Core Signal Flow
```
Sources → Envelope → Interface → Delay Lines → Feedback → Filter → Output
   ↓                                                ↑
   └─────────────── Modulation (LFO) ─────────────┘
```

### Module Breakdown

#### 1. **Sources Module** (NEW)
Create `src/audio/modules/SourcesModule.js`:
- Three independent sources: DC, White Noise, Sawtooth (pitch-tracking)
- Each with independent level control
- Summing mixer output
- CV input for sawtooth frequency

#### 2. **Envelope Module** (REFACTOR existing)
Refactor into `src/audio/modules/EnvelopeModule.js`:
- Gate-triggered AR envelope (Attack/Release only)
- Remove Decay/Sustain (per requirements)
- Applies to summed source signal

#### 3. **Interface Module** (NEW - replaces reed-only nonlinearity)
Create `src/audio/modules/InterfaceModule.js`:
- Rotary switch for 5 interface types
- Intensity parameter (0-1) controlling effect strength
- **Pluck**: Initial impulse with quick decay (Karplus-Strong style)
- **Hit**: Sharp impulse with variable hardness
- **Reed**: Clarinet-style nonlinear reflection (existing tanh)
- **Flute**: Edge tone simulation (softer nonlinearity)
- **Brass**: Lip model (asymmetric nonlinearity)

#### 4. **Delay Lines Module** (REFACTOR existing)
Refactor into `src/audio/modules/DelayLinesModule.js`:
- Two independent delay lines with fractional interpolation
- CV-controlled length (pitch tracking)
- **Tuning** control: coarse pitch adjustment
- **Ratio** control: delay2_length = delay1_length × ratio
  - Ratio = 1.0 (12 o'clock): equal lengths (harmonic)
  - Ratio < 1.0: inharmonic (drum/gong tones)
  - Ratio > 1.0: stretched inharmonic

#### 5. **Feedback Module** (NEW)
Create `src/audio/modules/FeedbackModule.js`:
- Three feedback paths with independent level controls:
  - Delay Line 1 output → input
  - Delay Line 2 output → input
  - Post-filter output → input
- Summing junction before delay line inputs

#### 6. **Filter Module** (REFACTOR existing)
Refactor into `src/audio/modules/FilterModule.js`:
- **Frequency**: cutoff/center frequency
- **Q**: resonance/bandwidth
- **Shape**: morphing control (0-1)
  - 0.0: Lowpass
  - 0.5: Bandpass
  - 1.0: Highpass
- Use state-variable filter or parallel filter mix

#### 7. **Modulation Module** (NEW)
Create `src/audio/modules/ModulationModule.js`:
- Sine wave LFO with frequency control
- **Type/Level** control (bipolar):
  - Center (12 o'clock): No modulation
  - CCW (0-50): AM (amplitude modulation) 0-100%
  - CW (50-100): FM (frequency modulation) 0-100%
- Modulation targets: delay line length (FM) or output amplitude (AM)

## UI Implementation

### Control Layout
Create modular UI blocks in `src/ui/`:

1. **SourcesPanel.js**: 3 knobs (DC level, Noise level, Tone level)
2. **EnvelopePanel.js**: 2 knobs (Attack, Release)
3. **InterfacePanel.js**: 1 rotary switch, 1 knob (Type, Intensity)
4. **DelayLinesPanel.js**: 2 knobs (Tuning, Ratio)
5. **FeedbackPanel.js**: 3 knobs (Delay1 FB, Delay2 FB, Post-Filter FB)
6. **FilterPanel.js**: 3 knobs (Frequency, Q, Shape)
7. **ModulationPanel.js**: 2 knobs (LFO Freq, Type/Level)
8. **KeyboardController.js**: Reuse existing (provides Gate + CV)
9. **Visualizer.js**: Reuse existing

### New UI Components
- **RotarySwitchController.js**: 5-position switch for Interface type selection
- Update **KnobController.js**: Add bipolar mode for Modulation Type/Level

## Testing Strategy

### Unit Tests (Vitest)
- `tests/unit/modules/SourcesModule.test.js`
- `tests/unit/modules/EnvelopeModule.test.js`
- `tests/unit/modules/InterfaceModule.test.js`
- `tests/unit/modules/DelayLinesModule.test.js`
- `tests/unit/modules/FeedbackModule.test.js`
- `tests/unit/modules/FilterModule.test.js`
- `tests/unit/modules/ModulationModule.test.js`

### Integration Tests (Playwright)
- `tests/integration/pm-synth.spec.js`: Full signal chain tests
- UI interaction tests
- Audio output validation

## File Structure
```
pm-synth/
├── src/
│   ├── audio/
│   │   ├── modules/
│   │   │   ├── SourcesModule.js
│   │   │   ├── EnvelopeModule.js
│   │   │   ├── InterfaceModule.js
│   │   │   ├── DelayLinesModule.js
│   │   │   ├── FeedbackModule.js
│   │   │   ├── FilterModule.js
│   │   │   └── ModulationModule.js
│   │   ├── PMSynthEngine.js (refactored from ClarinetEngine)
│   │   ├── pm-synth-worklet.js (refactored from clarinet-worklet)
│   │   └── PMSynthProcessor.js (refactored from ClarinetProcessor)
│   ├── ui/
│   │   ├── panels/
│   │   │   ├── SourcesPanel.js
│   │   │   ├── EnvelopePanel.js
│   │   │   ├── InterfacePanel.js
│   │   │   ├── DelayLinesPanel.js
│   │   │   ├── FeedbackPanel.js
│   │   │   ├── FilterPanel.js
│   │   │   └── ModulationPanel.js
│   │   ├── KnobController.js (enhanced)
│   │   ├── RotarySwitchController.js (new)
│   │   ├── KeyboardController.js (reuse)
│   │   └── Visualizer.js (reuse)
│   ├── main.js (refactored)
│   └── constants.js (expanded)
├── tests/
│   ├── unit/modules/
│   └── integration/
├── docs/
│   ├── requirements.md (existing)
│   ├── PLAN.md (this document)
│   └── architecture.mmd (update)
└── index.html (updated layout)
```

## Implementation Phases

### Phase 1: Core Audio Modules (Foundation)
1. Create SourcesModule.js
2. Refactor EnvelopeModule.js (extract from existing)
3. Create DelayLinesModule.js (dual delay lines)
4. Create FeedbackModule.js
5. Create constants for all new parameters

### Phase 2: Signal Processing
1. Create InterfaceModule.js (5 types)
2. Refactor FilterModule.js (shape morphing)
3. Create ModulationModule.js (LFO)

### Phase 3: Engine Integration
1. Refactor PMSynthEngine.js to use all modules
2. Update pm-synth-worklet.js
3. Update PMSynthProcessor.js

### Phase 4: UI Implementation
1. Create RotarySwitchController.js
2. Enhance KnobController.js (bipolar mode)
3. Create all panel components
4. Update main.js for new architecture
5. Update index.html layout

### Phase 5: Testing & Documentation
1. Write unit tests for all modules
2. Write integration tests
3. Update architecture diagram
4. Write comprehensive README
5. Test all interface types and parameter ranges

## Key Design Principles

1. **Small, focused classes**: Each module < 200 lines
2. **Test-driven**: Unit test each module independently
3. **Reusable components**: UI controllers work for any parameter
4. **Clear signal flow**: Audio path is traceable through modules
5. **Consistent API**: All modules follow same initialization/process pattern
6. **Constants-driven**: All magic numbers in constants.js
7. **PWA-ready**: Responsive design, works offline

## Technical Details

### Module API Pattern
All audio modules follow this consistent interface:

```javascript
class ModuleName {
  constructor(sampleRate) {
    // Initialize state variables
  }

  setParameter(name, value) {
    // Update parameter (0-1 normalized)
  }

  process(input, cv, gate) {
    // Process one sample
    return output;
  }

  reset() {
    // Reset state (on note-on)
  }
}
```

### Signal Flow Details

1. **Keyboard** generates:
   - `gate`: 0 or 1 (note on/off)
   - `cv`: frequency in Hz

2. **Sources** generates excitation:
   - DC: constant value × level
   - Noise: white noise × level
   - Tone: naive_sawtooth(cv) × level
   - Output: sum of all three

3. **Envelope** shapes sources:
   - AR envelope triggered by gate
   - Output: source × envelope

4. **Interface** applies nonlinearity:
   - Type-dependent transfer function
   - Intensity controls effect strength
   - Output: processed signal

5. **Delay Lines** create resonance:
   - Two parallel delay lines
   - Length from CV + tuning offset
   - Ratio adjusts delay2 relative to delay1
   - Outputs: [delay1_out, delay2_out]

6. **Feedback** mixes signals:
   - Input = interface_out + delay1_fb + delay2_fb + filter_fb
   - Three independent gains
   - Output: mixed signal → delay lines

7. **Filter** shapes tone:
   - SVF with morphable response
   - Shape blends LP/BP/HP
   - Q controls resonance
   - Output: filtered signal

8. **Modulation** adds movement:
   - LFO generates sine wave
   - Bipolar control selects AM or FM
   - AM: modulates output gain
   - FM: modulates delay line length

## Interface Type Implementations

### Pluck
```javascript
// One-way filter: pass initial impulse, dampen subsequent
process(input) {
  const threshold = this.intensity * 0.5;
  if (Math.abs(input) > threshold) {
    this.lastPeak = input;
    return input;
  }
  return input * (1 - this.intensity);
}
```

### Hit
```javascript
// Sharp waveshaper with adjustable hardness
process(input) {
  const hardness = 1 + this.intensity * 10;
  return Math.tanh(input * hardness) / hardness;
}
```

### Reed (existing)
```javascript
// Clarinet-style cubic nonlinearity
process(input) {
  const stiffness = this.intensity * 8 + 0.8;
  return fastTanh(input * stiffness);
}
```

### Flute
```javascript
// Soft symmetric nonlinearity (jet instability)
process(input) {
  const softness = 1 + this.intensity * 2;
  return input - (input ** 3) / (3 * softness);
}
```

### Brass
```javascript
// Asymmetric lip model (different + and - slopes)
process(input) {
  const asym = 0.5 + this.intensity;
  if (input > 0) {
    return Math.tanh(input * asym);
  } else {
    return Math.tanh(input / asym);
  }
}
```

## State-Variable Filter Implementation

For morphable LP/BP/HP response:

```javascript
class SVFilter {
  process(input, freq, q, shape) {
    const f = 2 * Math.sin(Math.PI * freq / sampleRate);
    const qInv = 1 / q;

    // State-variable filter topology
    this.low += f * this.band;
    this.high = input - this.low - qInv * this.band;
    this.band += f * this.high;

    // Morph between responses
    if (shape < 0.5) {
      // Blend lowpass → bandpass
      const mix = shape * 2;
      return this.low * (1 - mix) + this.band * mix;
    } else {
      // Blend bandpass → highpass
      const mix = (shape - 0.5) * 2;
      return this.band * (1 - mix) + this.high * mix;
    }
  }
}
```

## Parameter Ranges and Mappings

### Sources
- DC Level: 0-1 linear
- Noise Level: 0-1 linear
- Tone Level: 0-1 linear

### Envelope
- Attack: 0.001-0.5s (exponential mapping)
- Release: 0.01-2.0s (exponential mapping)

### Interface
- Type: [Pluck, Hit, Reed, Flute, Brass]
- Intensity: 0-1 linear

### Delay Lines
- Tuning: -12 to +12 semitones (linear)
- Ratio: 0.5-2.0 (exponential, center = 1.0)

### Feedback
- Delay1 FB: 0-0.99 linear
- Delay2 FB: 0-0.99 linear
- Filter FB: 0-0.99 linear

### Filter
- Frequency: 20-20000 Hz (exponential)
- Q: 0.5-20 (exponential)
- Shape: 0-1 linear (0=LP, 0.5=BP, 1=HP)

### Modulation
- LFO Freq: 0.1-20 Hz (exponential)
- Type/Level: bipolar 0-100
  - 0-50: AM depth 100%-0%
  - 50: No modulation
  - 50-100: FM depth 0%-100%

## Performance Considerations

- Target: < 15% CPU usage on modern hardware
- AudioWorklet ensures < 10ms latency
- Dual delay lines: max length = sampleRate/20 (20 Hz lowest note)
- Memory footprint: ~2-3 MB (2x delay buffers)
- No heap allocations in process() functions
- All arrays pre-allocated in constructors

## Browser Compatibility

- Chrome/Edge 66+: Full AudioWorklet support
- Firefox 76+: Full AudioWorklet support
- Safari 14.1+: Full AudioWorklet support
- iOS Safari 14.5+: Requires user interaction for audio

## References

- Smith, J.O. "Physical Modeling Using Digital Waveguides" (1992)
- Karjalainen, M. "Plucked-string models: From Karplus-Strong to digital waveguides" (1998)
- Välimäki, V. "Discrete-time modeling of acoustic tubes" (1995)
- Cook, P. "Real Sound Synthesis for Interactive Applications" (2002)
- Roads, C. "The Computer Music Tutorial" (1996)
