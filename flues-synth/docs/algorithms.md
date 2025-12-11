# Flues-Synth Algorithm Reference

Comprehensive technical documentation of all synthesis and signal processing algorithms used in flues-synth. This guide provides enough detail for implementation in other systems.

## Table of Contents

1. [Distortion Synthesis Algorithms](#distortion-synthesis-algorithms)
2. [Signal Processing Modules](#signal-processing-modules)
3. [Physical Modeling Algorithms](#physical-modeling-algorithms)
4. [Modulation and Effects](#modulation-and-effects)

---

## Distortion Synthesis Algorithms

The flues-synth uses seven distortion synthesis algorithms ported from the Disyn LV2 plugin, based on research by Victor Lazzarini. These algorithms generate complex harmonic and inharmonic spectra efficiently without explicit additive synthesis.

### 1. Dirichlet Pulse (Band-Limited Pulse Train)

**Principle:** Generates N harmonics with equal amplitude using a closed-form expression. Based on the Windham/Steiglitz band-limited pulse formula.

**Formula:**
```
s(t) = [sin((2N+1)θ/2) / sin(θ/2) - 1] / N
where θ = 2π·phase
```

**Parameters:**
- **param1 (Harmonics):** Number of partials (1-64)
  - Mapped: `N = 1 + round(param1 × 63)`
  - Controls bandwidth (spectral "cutoff")
- **param2 (Tilt):** Spectral slope (-3 to +15 dB/octave)
  - Mapped: `tilt = -3 + param2 × 18`
  - Applied as amplitude factor: `10^(tilt/20)`

**Implementation Notes:**
- Division-by-zero protection: if `|sin(θ/2)| < 1e-8`, return 1.0
- Peak RMS: ~0.8
- Creates bright, harmonic-rich tones
- Excellent for square/pulse wave synthesis

**Use Cases:**
- Classic subtractive-style synthesis
- Filter sweeps (time-varying N parameter)
- Harmonic pulse trains

---

### 2. DSF Single (Discrete Summation Formula)

**Principle:** Moorer's generalized discrete summation formula provides harmonic/inharmonic control through frequency ratio and spectral rolloff.

**Formula:**
```
s(t) = [sin(ω) - a·sin(ω-θ)] / (1 - 2a·cos(θ) + a²) × √(1-a²)
where:
  ω = 2π·phase
  θ = 2π·secondaryPhase
  a = decay parameter (spectral rolloff)
```

**Parameters:**
- **param1 (Decay):** Spectral rolloff (0-0.98)
  - Mapped: `a = param1 × 0.98`
  - Lower values = brighter spectrum
  - Higher values = darker, faster rolloff
- **param2 (Ratio):** ω/θ frequency ratio (0.5-4.0, exponential)
  - Mapped: `ratio = 0.5 × (4.0/0.5)^param2`
  - 1.0 = harmonic
  - √2 (~1.414) = inharmonic (golden bells)
  - φ (~1.618) = golden ratio (natural bells)

**Implementation Notes:**
- Division-by-zero protection: if `|denominator| < 1e-8`, return 0.0
- Normalization factor `√(1-a²)` maintains constant RMS
- Output scaled by 0.5× to match other algorithm levels
- Peak RMS: ~0.5

**Use Cases:**
- Bell synthesis (ratio = √2 or φ)
- Gong timbres (varying ratio)
- Metallic percussion
- Filter-like spectral shaping without filters

---

### 3. DSF Double (Double-Sided DSF)

**Principle:** Two DSF components with opposite phase relationships create complex beating patterns and richer inharmonic spectra.

**Formula:**
```
s(t) = 0.25 × [DSF(ω, θ, a) + DSF(ω, -θ, a)]
where DSF is computed as in DSF Single
```

**Parameters:**
- **param1 (Decay):** Spectral rolloff (0-0.96)
  - Mapped: `a = param1 × 0.96`
- **param2 (Ratio):** Frequency ratio (0.5-4.5, exponential)
  - Mapped: `ratio = 0.5 × (4.5/0.5)^param2`
  - Wider range than DSF Single for more extreme inharmonicity

**Implementation Notes:**
- Maintains two separate phase accumulators for positive and negative modulation
- Output scaled by 0.25× (combination of two DSF components)
- Peak RMS: ~0.5
- More complex than DSF Single due to dual computation

**Use Cases:**
- Complex metallic timbres
- Church bell synthesis
- Gamelan-like sounds
- Spectrally dense textures

---

### 4. Tanh Square (Hyperbolic Tangent Waveshaping)

**Principle:** Smooth waveshaping using hyperbolic tangent to create square-like waves with controlled harmonic content and reduced aliasing.

**Formula:**
```
s(t) = tanh(sin(2π·phase) × drive) × trim
```

**Parameters:**
- **param1 (Drive):** Saturation amount (0.05-5.0, exponential)
  - Mapped: `drive = 0.05 × (5.0/0.05)^param1`
  - Higher drive = more harmonics, harder clipping
- **param2 (Trim):** Output scaling (0.2-1.2, exponential)
  - Mapped: `trim = 0.2 × (1.2/0.2)^param2`
  - Compensates for level changes at different drive settings

**Implementation Notes:**
- Uses standard `tanh()` function (available in math.h)
- Nearly band-limited at moderate drive settings
- Peak RMS: ~0.8
- Aliasing increases with drive

**Use Cases:**
- Saturated square waves
- Analog-style distortion
- Soft-clipped waveforms
- Vintage synthesizer emulation

---

### 5. Tanh Saw (Square-to-Sawtooth Transformation)

**Principle:** Heterodyne a tanh-shaped square wave with a cosine to generate missing even harmonics, creating a sawtooth-like spectrum.

**Formula:**
```
square = tanh(sin(2π·phase₁) × drive)
saw = square + cos(2π·phase₂) × (1 - square²)
s(t) = square × (1-blend) + saw × blend
```

**Parameters:**
- **param1 (Drive):** Saturation amount (0.05-4.5, exponential)
  - Mapped: `drive = 0.05 × (4.5/0.05)^param1`
- **param2 (Blend):** Square/saw mix (0-1, linear)
  - 0 = pure square
  - 1 = pure sawtooth
  - 0.5 = 50/50 blend

**Implementation Notes:**
- Maintains two phase accumulators (sine and cosine)
- Heterodyning term: `cos(θ) × (1-square²)`
- Peak RMS: ~0.7
- Output scaled by 0.7× for safety

**Use Cases:**
- Variable-waveshape oscillator
- Morphing between square and saw
- Rich harmonic content
- Classic analog waveforms

---

### 6. PAF (Phase-Aligned Formant)

**Principle:** Ring modulation of a carrier with an exponentially-shaped spectrum to create formant-like resonances without filtering.

**Formula:**
```
carrier = sin(2π·secondaryPhase)
modulator = sin(2π·phase)
decay = exp(-bandwidth / sampleRate)
smoothed = decay × smoothed + (1-decay) × modulator
s(t) = carrier × (0.6 + 0.4 × smoothed) × 0.5
```

**Parameters:**
- **param1 (Formant):** Formant/fundamental ratio (0.5-6.0, exponential)
  - Mapped: `ratio = 0.5 × (6.0/0.5)^param1`
  - Carrier frequency = ratio × fundamental
- **param2 (Bandwidth):** Formant bandwidth (50-3000 Hz, exponential)
  - Mapped: `bandwidth = 50 × (3000/50)^param2`
  - Controls formant width/sharpness

**Implementation Notes:**
- One-pole lowpass filter on modulator signal
- Decay coefficient: `exp(-BW/SR)` provides exponential smoothing
- Output scaled by 0.5× to match other algorithms
- Peak RMS: ~0.5
- Creates Gaussian-like spectral bump

**Use Cases:**
- Vowel-like synthesis
- Formant generation without bandpass filters
- Natural, organic timbres
- Vocal tract modeling

---

### 7. Modified FM (ModFM)

**Principle:** FM synthesis using Modified Bessel functions instead of standard Bessel functions, creating natural spectral evolution without the "wobble" of classic FM.

**Formula:**
```
carrier = cos(2π·phase)
modulator = cos(2π·modPhase)
envelope = exp(-index)
s(t) = carrier × exp(index × (modulator - 1)) × envelope × 0.6
```

**Mathematical Basis:**
- Standard FM: `Σ Jₙ(k)·cos((ωc + n·ωm)t)` (Bessel functions)
- Modified FM: `exp(-k) × Σ Iₙ(k)·cos((ωc + n·ωm)t)` (Modified Bessel)

**Parameters:**
- **param1 (Index):** Modulation depth (0.01-8.0, exponential)
  - Mapped: `index = 0.01 × (8.0/0.01)^param1`
  - Controls spectral brightness/complexity
- **param2 (Ratio):** Carrier/modulator ratio (0.25-6.0, exponential)
  - Mapped: `ratio = 0.25 × (6.0/0.25)^param2`
  - Integer ratios = harmonic
  - Non-integer = inharmonic

**Implementation Notes:**
- Uses exponential function: `exp(k × (cos(θ) - 1))`
- Normalization envelope: `exp(-k)` prevents level changes
- Output scaled by 0.6× for level matching
- Peak RMS: ~0.5
- Much more natural evolution than classic FM

**Use Cases:**
- Natural FM synthesis
- Filter-like effects without actual filtering
- Evolving, animated timbres
- Bell and vocal-like sounds

---

## Signal Processing Modules

### DC Blocker

**Principle:** First-order highpass filter that removes DC offset while preserving audio content.

**Formula:**
```
y[n] = x[n] - x[n-1] + R × y[n-1]
where R = 0.999 (pole coefficient)
```

**Frequency Response:**
- -60 dB at DC (0 Hz)
- -3 dB at ~7.6 Hz (at 48 kHz sample rate)
- Effectively transparent above 20 Hz

**Implementation:**
```c
float dc_blocker_process(DCBlocker* blocker, float input) {
    float output = input - blocker->x1 + blocker->R * blocker->y1;
    blocker->x1 = input;
    blocker->y1 = output;
    return output;
}
```

**Use Cases:**
- Remove DC offset from oscillators
- Prevent feedback loop latching
- Protect against DC accumulation

**Flues-synth Usage:**
- **DC Blocker 1:** After Disyn oscillators (catches DC at source)
- **DC Blocker 2:** On feedback path (prevents loop accumulation)

---

### Formant Filter (Resonant Bandpass)

**Principle:** Second-order biquad bandpass filter designed for vocal formant synthesis.

**Transfer Function:**
```
H(z) = (b₀ + b₁z⁻¹ + b₂z⁻²) / (1 + a₁z⁻¹ + a₂z⁻²)
```

**Coefficient Calculation:**
```
Q = frequency / bandwidth
ω = 2π × frequency / sampleRate
α = sin(ω) / (2Q)

Numerator:
  b₀ = Q × α
  b₁ = 0
  b₂ = -Q × α

Denominator:
  a₀ = 1 + α
  a₁ = -2 × cos(ω)
  a₂ = 1 - α

Normalized coefficients:
  a₀' = b₀ / a₀
  a₁' = b₁ / a₀ = 0
  a₂' = b₂ / a₀
  b₁' = a₁ / a₀
  b₂' = a₂ / a₀
```

**Difference Equation:**
```
y[n] = a₀'×x[n] + a₁'×x[n-1] + a₂'×x[n-2] - b₁'×y[n-1] - b₂'×y[n-2]
```

**Parameters:**
- **Frequency:** Center frequency in Hz (20 Hz - Nyquist)
- **Bandwidth:** Filter bandwidth in Hz
  - Narrower = sharper resonance, higher Q
  - Wider = gentler slope, lower Q
- **Q:** Quality factor = frequency / bandwidth
  - Clamped to minimum 0.5 for stability

**Implementation Notes:**
- Q-based parameterization for stable tuning
- Constant 0 dB peak gain (unity at center frequency)
- Stability checks prevent infinite/NaN values
- State reset when instability detected

**Typical Formant Frequencies:**
- **F1 (Jaw):** 200-1000 Hz (vowel height)
- **F2 (Tongue):** 500-3000 Hz (vowel frontness)
- **F3 (Lips):** 1500-4000 Hz (brightness)
- **F4 (Quality):** 2500-4500 Hz (vocal quality)

**Cascade Gain Compensation:**
- Each formant attenuates signal by ~3-6 dB
- Flues-synth applies 2.0× makeup gain after formant cascade
- Preserves output level comparable to direct oscillator

---

### State-Variable Filter (SVF)

**Principle:** Topology that simultaneously produces lowpass, bandpass, and highpass outputs from the same state variables, allowing morphing between filter types.

**State Update Equations:**
```
f = 2 × sin(π × frequency / sampleRate)
q = 1 / Q

low[n] = low[n-1] + f × band[n-1]
high[n] = input[n] - low[n] - q × band[n]
band[n] = band[n-1] + f × high[n]
```

**Output Morphing:**
```
if (shape < 0.5):
    // LP → BP transition
    mix = shape × 2
    output = low × (1-mix) + band × mix
else:
    // BP → HP transition
    mix = (shape - 0.5) × 2
    output = band × (1-mix) + high × mix
```

**Parameters:**
- **Frequency:** Cutoff/center frequency (20 Hz - 20 kHz)
- **Q:** Resonance (0.1 - 10)
  - Low Q (0.1-1): Gentle slope
  - High Q (5-10): Sharp resonance
- **Shape:** Filter type morph (0-1, linear)
  - 0.0 = Pure lowpass
  - 0.5 = Pure bandpass
  - 1.0 = Pure highpass

**Implementation Notes:**
- `f` coefficient clamped to 1.0 maximum for stability
- `q` coefficient clamped to 0.01 minimum
- Continuous morphing between types (no clicking)
- Very CPU efficient (no coefficient recalculation per sample)

**Stability:**
- Inherently stable for `f ≤ 1` and `q > 0`
- No pole positions to check
- Self-limiting feedback structure

**Use Cases:**
- Filter sweeps (time-varying frequency)
- Resonant effects (high Q settings)
- Morphing filter types (animated shape parameter)
- Subtractive synthesis

---

### Envelope Generator (Attack-Release)

**Principle:** Exponential attack/release envelope with gate-driven triggering.

**State Machine:**
- **IDLE:** Gate off, output = 0
- **ATTACK:** Gate on, rising exponentially to 1.0
- **SUSTAIN:** At 1.0, held while gate on
- **RELEASE:** Gate off, falling exponentially to 0

**Exponential Curve:**
```
Attack coefficient:  aCoeff = exp(-1.0 / (attackTime × sampleRate))
Release coefficient: rCoeff = exp(-1.0 / (releaseTime × sampleRate))

During attack:  envelope += (1.0 - envelope) × (1.0 - aCoeff)
During release: envelope += (0.0 - envelope) × (1.0 - rCoeff)
```

**Time Mapping (Exponential):**
```
Attack:  1 ms  → 1000 ms   (param 0.0 → 1.0)
         attackTime = 1 × (1000/1)^param

Release: 10 ms → 3000 ms   (param 0.0 → 1.0)
         releaseTime = 10 × (3000/10)^param
```

**Implementation:**
```c
float envelope_process(EnvelopeModule* env) {
    if (env->gate) {
        if (env->envelope < 0.99f) {
            // Attack phase
            float aCoeff = expf(-1.0f / (env->attack_time * env->sample_rate));
            env->envelope += (1.0f - env->envelope) * (1.0f - aCoeff);
        } else {
            // Sustain phase
            env->envelope = 1.0f;
        }
    } else {
        if (env->envelope > 0.01f) {
            // Release phase
            float rCoeff = expf(-1.0f / (env->release_time * env->sample_rate));
            env->envelope += (0.0f - env->envelope) * (1.0f - rCoeff);
        } else {
            // Idle phase
            env->envelope = 0.0f;
        }
    }
    return env->envelope;
}
```

**Threshold Detection:**
- Attack complete when envelope > 0.99
- Release complete when envelope < 0.01
- Prevents infinite asymptotic approach

---

### LFO Modulation

**Principle:** Low-frequency oscillator providing bipolar amplitude modulation (AM) or frequency modulation (FM).

**Oscillator:**
```
phase = (phase + frequency / sampleRate) mod 1.0
lfo = sin(2π × phase)  // Range: -1 to +1
```

**Bipolar Depth Control:**
```
if (depth ≥ 0):
    // FM mode
    AM_amount = 0
    FM_amount = depth
else:
    // AM mode
    AM_amount = -depth
    FM_amount = 0
```

**Application:**
```
AM: output = input × (1.0 + AM_amount × lfo)
FM: frequency' = frequency × (1.0 + FM_amount × lfo)
```

**Parameters:**
- **LFO Frequency:** Modulation rate (0.1 - 20 Hz, exponential)
- **AM ↔ FM Depth:** Bipolar modulation control (-1 to +1)
  - -1.0 = 100% amplitude modulation
  - 0.0 = No modulation
  - +1.0 = 100% frequency modulation

**Use Cases:**
- Vibrato (FM, slow rate ~5 Hz)
- Tremolo (AM, slow rate ~5 Hz)
- Auto-wah (FM on filter frequency)
- Animated textures

---

## Physical Modeling Algorithms

Flues-synth includes 12 physical modeling "interface" strategies that simulate different instrument excitation mechanisms. Each strategy processes an excitation signal (from Disyn, noise, or DC sources) to create characteristic timbres.

### Interface Strategy Overview

All strategies follow a common pattern:
1. **Excitation input** (from sources)
2. **Nonlinear processing** (waveshaping, clipping, feedback)
3. **Energy coupling** to delay lines
4. **Output** fed to delay/filter stages

### 1. Pluck Strategy (Karplus-Strong)

**Principle:** Simulates plucked string using excitation burst with lowpass filtering.

**Algorithm:**
```
excitation = input × (1.0 - intensity)  // Damping
output = excitation
```

**Characteristics:**
- Bright, metallic attack
- Natural decay
- High intensity = less damping = brighter sound

---

### 2. Hit Strategy (Struck Percussion)

**Principle:** Sharp transient with fast decay, simulating mallet strikes.

**Algorithm:**
```
envelope = envelope × 0.95  // Fast decay
excitation = input × envelope
output = tanh(excitation × (1.0 + intensity × 2.0))  // Soft clip
```

**Characteristics:**
- Sharp attack transient
- Fast exponential decay
- Intensity controls saturation

---

### 3. Reed Strategy (Clarinet-Style)

**Principle:** Nonlinear waveshaping simulating reed closure/opening.

**Algorithm:**
```
pressure = dc_source + noise × (1.0 - intensity)
reflection = -feedback_from_delays
junction = pressure + reflection
reed_position = intensity
flow = junction - junction³ × reed_position × 0.3
output = tanh(flow × 2.0)
```

**Detailed Flow:**
1. **Pressure source:** DC + noise (breath)
2. **Reflection:** Negative feedback from bore
3. **Junction sum:** Pressure + reflection
4. **Cubic nonlinearity:** Simulates reed valve
5. **Soft clipping:** Prevents explosions

**Characteristics:**
- Woody, breathy timbre
- Self-oscillation with DC input
- Intensity controls reed stiffness

---

### 4. Flute Strategy (Jet-Edge)

**Principle:** Air jet impinging on edge, creating turbulence-driven oscillation.

**Algorithm:**
```
jet = noise × 0.2 + dc × 0.8
reflection = feedback_from_delays
jet_deviation = jet + reflection × intensity
output = tanh(jet_deviation × 3.0)
```

**Characteristics:**
- Airy, hollow timbre
- Requires DC + noise excitation
- Intensity controls feedback coupling

---

### 5. Brass Strategy (Lip Buzz)

**Principle:** Lip vibration with strong nonlinear feedback.

**Algorithm:**
```
pressure = dc + noise × 0.1
reflection = feedback
lip_opening = pressure + reflection × intensity
output = sin(lip_opening × 4.0) × tanh(pressure)
```

**Characteristics:**
- Buzzy, brassy timbre
- Strong harmonic content
- Intensity controls lip tension

---

### 6. Bow Strategy (Friction)

**Principle:** Stick-slip friction model for bowed strings.

**Algorithm:**
```
velocity = feedback
bow_force = intensity
friction = velocity × (1.0 + bow_force × tanh(velocity * 10.0))
output = tanh(friction × 2.0)
```

**Characteristics:**
- Sustained, singing tone
- Depends heavily on feedback
- Intensity controls bow pressure

---

### 7. Bell Strategy (Metallic Resonator)

**Principle:** Bright, ringing resonance with inharmonic character.

**Algorithm:**
```
strike = input × 2.0
resonance = feedback × 0.95
output = tanh((strike + resonance) × (1.0 + intensity))
```

**Characteristics:**
- Bright, ringing attack
- Long sustain with feedback
- Metallic timbre

---

### 8. Drum Strategy (Membrane)

**Principle:** Struck membrane with nonlinear stiffening.

**Algorithm:**
```
strike = input
tension = feedback × intensity
membrane = strike + tension × (1.0 - tension²)
output = tanh(membrane × 1.5)
```

**Characteristics:**
- Thuddy, resonant attack
- Pitch rises with intensity
- Natural membrane behavior

---

### 9-12. Hypothetical Strategies (Crystal, Vapor, Quantum, Plasma)

These strategies are experimental/abstract physical models:

**Crystal:** Golden-ratio coupled oscillators (glassy, shimmering)
**Vapor:** Chaotic turbulent flow (unstable, evolving)
**Quantum:** Phase-locked harmonic coupling (metallic interference)
**Plasma:** Amplitude-driven energy feedback (aggressive, growling)

---

## Modulation and Effects

### Feedback Mixer

**Principle:** Combines delay and filter outputs with independent level controls before feeding back into the signal chain.

**Mixing Formula:**
```
feedback_signal = delay1_out × delay1_feedback
                + delay2_out × delay2_feedback
                + filter_out × filter_feedback
```

**Parameters:**
- **Delay1 Feedback:** First delay return (0-1)
- **Delay2 Feedback:** Second delay return (0-1)
- **Filter Feedback:** SVF return (0-1)

**Safety:**
- Total feedback can exceed 1.0 (e.g., 0.5 + 0.3 + 0.4 = 1.2)
- DC Blocker on feedback path prevents latching
- Tanh soft clip at output prevents runaway

---

### Dual Delay Lines

**Principle:** Two parallel delay lines with pitch tracking and ratio control.

**Implementation:**
```
delay_time_1 = (1.0 / frequency) × (tuning_semitones / 12.0)
delay_time_2 = delay_time_1 × ratio

Delay1: circular buffer at delay_time_1
Delay2: circular buffer at delay_time_2
```

**Parameters:**
- **Tuning:** Semitone offset (-12 to +12 semitones)
- **Ratio:** Delay2/Delay1 length ratio (0.5 to 2.0)
  - 0.5 = octave up
  - 1.0 = unison
  - 2.0 = octave down

**Pitch Tracking:**
- Delays automatically follow MIDI note frequency
- Creates harmonic/inharmonic interval relationships
- Used for resonance and physical modeling

---

### Soft Clipping (Tanh)

**Principle:** Smooth limiting to prevent digital clipping while adding gentle saturation.

**Formula:**
```
output = tanh(input)
```

**Transfer Function:**
- Linear region: -1 to +1 (transparent)
- Soft compression: ±1 to ±2
- Hard limit: asymptotic to ±1

**Application in Flues-synth:**
```
Global Processing Chain:
  input × 0.85 (pad)
    → tanh() (soft clip)
    → × 0.95 (master gain)
    → peak ~0.81
```

---

## Implementation Guidelines

### Numerical Stability

**Critical Checks:**
1. **Division by zero:** Add epsilon before division
   ```c
   if (fabs(denominator) < EPSILON) return 0.0f;
   ```

2. **Filter stability:** Clamp state variables
   ```c
   if (!isfinite(output)) {
       reset_filter_state();
       return 0.0f;
   }
   ```

3. **Phase wrap:** Use modulo to prevent accumulation
   ```c
   phase = phase - floorf(phase);  // Wraps to [0, 1)
   ```

### Parameter Mapping

**Exponential Mapping (frequency-like parameters):**
```c
float expo_map(float param, float min, float max) {
    // param: 0-1 normalized
    return min * powf(max / min, param);
}
```

**Linear Mapping (blend/mix parameters):**
```c
float linear_map(float param, float min, float max) {
    return min + param * (max - min);
}
```

### Performance Optimization

**Lookup Tables:**
- Use for: `exp()`, `sin()`, `cos()`, `tanh()`
- Linear interpolation acceptable for most cases
- Pre-compute at initialization

**SIMD Vectorization:**
- Process 4 samples at once using SSE/NEON
- Particularly effective for filters and oscillators
- ARM NEON support in flues-synth (compile flag)

**Branch Prediction:**
- Avoid conditionals in inner loops
- Use `fmin()/fmax()` instead of `if` for clamping

---

## References

### Academic Papers

1. **Moorer, J.A.** (1976) "The Synthesis of Complex Audio Spectra by Means of Discrete Summation Formulae"
2. **Lazzarini, V.** (2009) "Distortion Synthesis Techniques" - Csound Journal
3. **Chamberlin, H.** (1985) "Musical Applications of Microprocessors" - State Variable Filters

### Flues-Synth Documentation

- **Signal Flow:** `docs/flues-synth-signal-flow.svg`
- **MIDI Control:** `docs/midi.md`
- **Program Guide:** `docs/PROGRAM_CHANGE.md`
- **Level Analysis:** `docs/disyn-level-analysis.md`
- **Distortion Analysis:** `docs/distortion-synthesis-analysis.md`

### Source Code

- **Disyn Algorithms:** `lv2/disyn/src/modules/OscillatorModule.hpp`
- **DSP Modules:** `flues-synth/src/audio/modules/`
- **Test Suite:** `flues-synth/tests/disyn_levels.c`

---

## Glossary

- **Band-limited:** No aliasing above Nyquist frequency
- **Biquad:** Second-order IIR filter (two poles, two zeros)
- **DSF:** Discrete Summation Formula
- **Formant:** Resonant frequency region in vocal tract
- **Inharmonic:** Partials not at integer multiples of fundamental
- **Moorer:** James A. Moorer, pioneer of discrete summation techniques
- **PAF:** Phase-Aligned Formant
- **SVF:** State-Variable Filter
- **Waveshaping:** Nonlinear transfer function applied to signal

---

*Document Version: 1.0*
*Last Updated: 2025-12-11*
*Flues-Synth Version: 0.1.0*
