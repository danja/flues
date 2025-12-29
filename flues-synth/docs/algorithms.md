# Flues-Synth Algorithm Reference

Comprehensive technical documentation of all synthesis and signal processing algorithms used in flues-synth. This guide provides enough detail for implementation in other systems.

## Table of Contents

1. [Distortion Synthesis Algorithms](#distortion-synthesis-algorithms)
2. [Signal Processing Modules](#signal-processing-modules)
3. [Physical Modeling Algorithms](#physical-modeling-algorithms)
4. [Modulation and Effects](#modulation-and-effects)

---

## Distortion Synthesis Algorithms

The flues-synth uses seventeen distortion synthesis algorithms ported from the Disyn LV2 plugin. The first seven are primitive algorithms based on research by Victor Lazzarini (2014-2018). Algorithms 8-17 are Combinations and Novel Extrapolations that cascade, blend, or modulate the primitive algorithms in new ways. These algorithms generate complex harmonic and inharmonic spectra efficiently without explicit additive synthesis.

### Primitive Algorithms (0-6)

The seven primitive algorithms provide fundamental distortion synthesis techniques.

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

### Combination Algorithms (7-13)

The seven Combination algorithms cascade, mix, or modulate the primitive algorithms to create new synthesis techniques with emergent properties not achievable by signal routing alone.

### 8. Hybrid Formant Engine (Combination 1)

**Principle:** Combines Modified FM synthesis with three fixed-frequency PAF formants (800, 1200, 2400 Hz) to create vocal-like timbres with complex formant structure.

**Formula:**
```
base = carrier × exp(-index × (|modulator| - 1)) × 0.4
formant1 = sin(2π × 800 Hz × spacing)
formant2 = sin(2π × 1200 Hz × spacing)
formant3 = sin(2π × 2400 Hz × spacing)
s(t) = (base + formant1 + formant2 + formant3) × 0.25
```

**Parameters:**
- **param1 (ModFM Index):** Modulation depth (0.01-3.0, exponential)
  - Controls base tone brightness
- **param2 (PAF Bandwidth):** Not currently used (reserved for formant Q)
- **param3 (Formant Spacing):** Frequency multiplier (0.8-1.2×)
  - Shifts all formants proportionally
  - 0.8× = deeper/darker voice
  - 1.2× = brighter/higher voice

**Implementation Notes:**
- ModFM base provides rich harmonic source
- Three sine-wave formants create formant peaks
- Formant spacing control enables vowel-like shifting
- Peak RMS: ~0.17
- Effective peak with synth scaling: ~0.10

**Use Cases:**
- Vocal synthesis
- Formant-rich pads
- Harmonic/resonant drones
- Speech-like timbres

---

### 9. Cascaded Spectral Sculptor (Combination 2)

**Principle:** Three-stage cascade: DSF → Asymmetric FM → Tanh waveshaping. Each stage progressively shapes the spectrum.

**Formula:**
```
stage1_dsf = (sin(ω) - a·sin(ω-θ)) / (1 - 2a·cos(θ) + a²)
stage2_asym = cos(ωc + k·sin(ωm)) × exp(k·(r-1/r)·cos(ωm)/2)
stage3_tanh = tanh(stage2 × drive)
s(t) = stage3 × 0.6
```

**Parameters:**
- **param1 (DSF Decay):** Spectral rolloff (0.5-0.95)
  - Controls initial harmonic content
- **param2 (Asym Ratio):** Asymmetric FM ratio r (0.5-2.0)
  - Adjusts spectral asymmetry
- **param3 (Tanh Drive):** Waveshaping intensity (0-5)
  - Controls final harmonic richness

**Implementation Notes:**
- DSF generates base spectrum with controlled rolloff
- Asymmetric FM adds spectral complexity
- Tanh soft-clips and adds additional harmonics
- Peak RMS: ~0.42
- Effective peak with synth scaling: ~0.13

**Use Cases:**
- Complex evolving timbres
- Aggressive distortion
- Multi-stage spectral shaping
- Industrial/harsh sounds

---

### 10. Parallel Distortion Bank (Combination 3)

**Principle:** Five parallel voices mixed together: 3× ModFM at different ratios (1:1, 3:2, 4:3) + 2× PAF formants (800, 2400 Hz).

**Formula:**
```
modfm1 = cos(ωc) × exp(k × (cos(ωm) - 1))  // 1:1
modfm2 = cos(ωc) × exp(k × (cos(1.5·ωm) - 1))  // 3:2
modfm3 = cos(ωc) × exp(k × (cos(1.333·ωm) - 1))  // 4:3
paf1 = sin(2π × 800 Hz)
paf2 = sin(2π × 2400 Hz)
modfmMix = (modfm1 + modfm2 + modfm3) / 3
pafMix = (paf1 + paf2) / 2
s(t) = (modfmMix × (1-balance) + pafMix × balance) × 0.5
```

**Parameters:**
- **param1 (ModFM Index):** Shared modulation depth (0.01-8.0)
  - Controls spectral complexity of all three ModFM voices
- **param2 (PAF Bandwidth):** Not currently used
- **param3 (Mix Balance):** ModFM vs. PAF blend (0=ModFM, 1=PAF)
  - Morphs between harmonic FM and formant tones

**Implementation Notes:**
- Three ModFM voices create rich inharmonic spectrum
- Different ratios produce detuned chorus effect
- Two PAF formants add resonant peaks
- Peak RMS: ~0.15
- Effective peak with synth scaling: ~0.09

**Use Cases:**
- Thick, layered timbres
- Choir-like pads
- Detuned ensemble sounds
- Complex harmonic textures

---

### 11. Feedback Distortion Network (Combination 4)

**Principle:** Modified FM synthesis with feedback loop where output modulates input frequency, creating chaotic/nonlinear behavior.

**Formula:**
```
modifiedFreq = baseFreq + feedbackSample × gain × baseFreq
carrier = cos(2π·phase)  // at modifiedFreq
modulator = cos(2π·modPhase)  // at modifiedFreq
output = carrier × exp(k × (modulator - 1))
feedbackSample = output  // stored for next sample
s(t) = output × 0.5
```

**Parameters:**
- **param1 (ModFM Index):** Modulation depth (0.01-8.0)
- **param2 (Feedback Gain):** Feedback amount (0-0.95)
  - 0 = no feedback (normal ModFM)
  - 0.95 = maximum chaos
- **param3:** Not currently used (reserved for feedback lowpass)

**Implementation Notes:**
- One-sample delay in feedback path
- Feedback modulates frequency, not phase
- Can produce chaotic/unstable behavior at high gain
- Peak RMS: ~0.26
- Effective peak with synth scaling: ~0.13

**Use Cases:**
- Chaotic/unstable tones
- Feedback synthesis
- Nonlinear timbre evolution
- Experimental/noise textures

---

### 12. Morphing Spectral Engine (Combination 5)

**Principle:** Continuous crossfade between three algorithms: DSF ↔ ModFM ↔ PAF. The param1 morphing position smoothly blends between these three timbres.

**Formula:**
```
if morphPos < 0.5:
  alpha = morphPos × 2
  dsf = (sin(ω) - a·sin(ω-θ)) / (1 - 2a·cos(θ) + a²)
  modfm = cos(ωc) × exp(k × (cos(ωm) - 1))
  output = (1-alpha) × dsf + alpha × modfm
else:
  alpha = (morphPos - 0.5) × 2
  modfm = cos(ωc) × exp(k × (cos(ωm) - 1))
  paf = sin(2π × 2·frequency)
  output = (1-alpha) × modfm + alpha × paf
s(t) = output × 0.6
```

**Parameters:**
- **param1 (Morph Position):** Algorithm blend (0-1)
  - 0.0 = DSF (harmonic/inharmonic)
  - 0.5 = ModFM (FM synthesis)
  - 1.0 = PAF (formant)
- **param2 (Character):** Shared parameter for all three algorithms
  - DSF decay, ModFM index, PAF ratio
- **param3:** Not currently used

**Implementation Notes:**
- Linear crossfading between adjacent algorithms
- Smooth transitions across full parameter range
- All three algorithms share param2 for coherent control
- Peak RMS: ~0.34
- Effective peak with synth scaling: ~0.15

**Use Cases:**
- Smooth timbre morphing
- Evolving pad sounds
- Spectral transitions
- Dynamic texture changes

---

### 13. Inharmonic Resonator (Combination 6)

**Principle:** DSF with golden ratio (φ) frequency relationship followed by PAF with frequency shift. Creates naturally inharmonic, bell-like timbres.

**Formula:**
```
φ = 1.618034 (golden ratio)
dsf = (sin(ω) - a·sin(ω - 2π·φ)) / (1 - 2a·cos(2π·φ) + a²)
pafFreq = 2·frequency + shift
paf = sin(2π·pafFreq)
s(t) = (dsf + paf) × 0.5
```

**Parameters:**
- **param1 (DSF Decay):** Spectral rolloff (0.5-0.9)
  - Controls initial harmonic decay
- **param2 (PAF Shift):** Frequency offset (5-50 Hz, exponential)
  - Detuning amount for the formant
- **param3:** Not currently used

**Implementation Notes:**
- Golden ratio creates natural inharmonicity
- PAF formant adds resonant peak with slight detuning
- 50/50 mix of DSF and PAF
- Peak RMS: ~0.28
- Effective peak with synth scaling: ~0.14

**Use Cases:**
- Bell synthesis
- Gong-like sounds
- Inharmonic resonance
- Natural metallic timbres

---

### 14. Adaptive Filter Emulation (Combination 7)

**Principle:** DSF + ModFM mixed to emulate a resonant filter. DSF decay and ratio simulate filter cutoff, ModFM adds character.

**Formula:**
```
cutoff = param1
resonance = param2
dsfDecay = 0.5 + resonance × 0.49  // 0.5-0.99
theta = 2π × (1 + cutoff × 2)  // cutoff affects ratio
dsf = (sin(ω) - a·sin(ω-θ)) / (1 - 2a·cos(θ) + a²)
modfmIndex = 0.01 × (2.0/0.01)^cutoff
modfm = cos(ωc) × exp(k × (cos(ωm) - 1))
s(t) = (dsf + modfm) × 0.15
```

**Parameters:**
- **param1 (Cutoff):** Pseudo-filter cutoff (0-1)
  - Controls DSF ratio and ModFM index
- **param2 (Resonance):** Pseudo-filter resonance (0-1)
  - Controls DSF decay (Q-like behavior)
- **param3:** Not currently used

**Implementation Notes:**
- Not a real filter, but emulates filter-like spectral shaping
- Higher cutoff = brighter spectrum
- Higher resonance = more pronounced peaks
- Peak RMS: ~0.43
- Effective peak with synth scaling: ~0.15

**Use Cases:**
- Filter sweep emulation
- Resonant sweeps without actual filtering
- Evolving spectral content
- Pseudo-subtractive synthesis

---

### Novel Extrapolations (14-16)

Three experimental algorithms that extend distortion synthesis in novel directions: multi-stage waveshaping, frequency-dependent processing, and cross-algorithm modulation.

### 15. Multi-Stage Waveshaping (Novel 1)

**Principle:** Three-stage nonlinear cascade: Tanh saturation → Exponential shaping → Ring modulation. Each stage adds progressively more complex harmonic distortion.

**Formula:**
```
input = sin(2π·phase)
stage1 = tanh(drive × input)
stage2 = stage1 × exp(depth × stage1)
carrier = sin(2π × carrierMult × frequency)
stage3 = stage2 × (1 + carrier)
s(t) = stage3 × 0.25
```

**Parameters:**
- **param1 (Tanh Drive):** Initial saturation (0.1-10.0, exponential)
  - Controls soft-clipping intensity
- **param2 (Exp Depth):** Exponential shaping (0.1-1.5, exponential)
  - Controls asymmetric harmonic enhancement
- **param3 (Ring Carrier):** Ring mod frequency (0.5x-5x fundamental)
  - Adds inharmonic sidebands

**Implementation Notes:**
- Stage 1: Soft saturation via tanh
- Stage 2: Exponential adds asymmetric distortion
- Stage 3: Ring mod adds metallic character
- Peak RMS: ~0.19
- Effective peak with synth scaling: ~0.13

**Use Cases:**
- Complex distortion
- Aggressive harmonic generation
- Industrial/harsh timbres
- Multi-stage waveshaping experiments

---

### 16. Frequency-Dependent Asymmetry (Novel 2)

**Principle:** Asymmetric FM synthesis with frequency-band-specific asymmetry ratios. Different frequency ranges use different r values for spectral variation.

**Formula:**
```
if frequency < 500 Hz:
  r = 0.5 + param1 × 0.5  // low-band (0.5-1.0)
else if frequency > 2000 Hz:
  r = 1.0 + param2 × 1.0  // high-band (1.0-2.0)
else:
  alpha = (frequency - 500) / 1500
  r = lowR × (1-alpha) + highR × alpha  // interpolate
asym_fm = cos(ωc + k·sin(ωm)) × exp(k·(r-1/r)·cos(ωm)/2)
s(t) = asym_fm × 0.5
```

**Parameters:**
- **param1 (Low-Band r):** Asymmetry for bass (0.5-1.0)
  - <500 Hz: downward spectral shift
- **param2 (High-Band r):** Asymmetry for treble (1.0-2.0)
  - >2000 Hz: upward spectral shift
- **param3:** Not currently used

**Implementation Notes:**
- Frequency-dependent processing
- Smooth interpolation in mid-band (500-2000 Hz)
- Different spectral character across pitch range
- Peak RMS: ~0.37
- Effective peak with synth scaling: ~0.13

**Use Cases:**
- Pitch-dependent timbre variation
- Frequency-split spectral processing
- Natural instrument modeling (brighter high notes)
- Dynamic spectral evolution

---

### 17. Cross-Algorithm Modulation (Novel 3)

**Principle:** Circular modulation where DSF parameters modulate ModFM parameters and vice versa, creating interdependent spectral evolution.

**Formula:**
```
dsfRatio = 1.5 + (param2 × 0.25 × 0.5)
modfmIndex = 0.25 + (param1 × 0.7 × 1.0)
dsf = (sin(ω) - a·sin(ω-θ)) / (1 - 2a·cos(θ) + a²)
modfm = cos(ωc) × exp(k × (cos(ωm) - 1))
s(t) = (dsf + modfm) × 0.35
```

**Parameters:**
- **param1 (DSF→ModFM Depth):** DSF modulates ModFM index (0-1)
  - Controls circular modulation strength
- **param2 (ModFM→DSF Depth):** ModFM modulates DSF ratio (0-1)
  - Creates feedback-like behavior
- **param3:** Not currently used

**Implementation Notes:**
- Circular modulation creates complex interactions
- Parameters influence each other
- Subtle nonlinear spectral evolution
- Peak RMS: ~0.21
- Effective peak with synth scaling: ~0.09

**Use Cases:**
- Interactive spectral evolution
- Complex modulation routing
- Interdependent parameter control
- Experimental synthesis

---

### 18. Taylor Series Approximation (Novel 4)

**Principle:** Generates waveforms using truncated Taylor series expansion of sine functions. By controlling the number of terms, the algorithm produces everything from rough aliased approximations (few terms) to smooth sinusoids (many terms). Blends fundamental and second harmonic for timbral variation.

**Mathematical Basis:**
The Taylor series for sine around x=0:
```
sin(x) = x - x³/3! + x⁵/5! - x⁷/7! + x⁹/9! - ...
       = Σ(n=0 to ∞) [(-1)ⁿ × x^(2n+1) / (2n+1)!]
```

**Algorithm:**
```
// Wrap angles to [-π, π] for convergence
θ₁ = wrap(2π × phase)
θ₂ = wrap(2 × θ₁)

// Compute truncated Taylor series (iterative)
fundamental = taylor_sine(θ₁, firstTerms)
second_harmonic = taylor_sine(θ₂, secondTerms)

// Blend outputs
s(t) = fundamental × (1-blend) + second_harmonic × blend
```

**Iterative Computation:**
```c
float taylor_sine(float x, int num_terms) {
    float wrapped = wrap_angle(x);  // [-π, π]
    float result = 0.0f;
    float term = wrapped;
    float x_squared = wrapped * wrapped;

    for (int n = 0; n < num_terms; n++) {
        result += term;
        // Next term: multiply by -x²/((2n+2)(2n+3))
        float denom = (2*n + 2) * (2*n + 3);
        term *= -x_squared / denom;
    }

    // Clamp to prevent runaway values
    return fmaxf(-1.5f, fminf(1.5f, result));
}
```

**Parameters:**
- **param1 (First Terms):** Number of terms for fundamental (1-10)
  - Mapped: `N = 1 + round(param1 × 9)`
  - 1 term: just x (sawtooth-like, severe aliasing)
  - 5 terms: reasonable approximation
  - 10 terms: nearly perfect sine wave
- **param2 (Second Terms):** Number of terms for 2nd harmonic (1-10)
  - Mapped: `M = 1 + round(param2 × 9)`
  - Controls brightness/overtone character
- **param3 (Blend):** Mix between fundamental and 2nd harmonic (0-1)
  - 0.0 = pure fundamental
  - 0.5 = 50/50 mix
  - 1.0 = pure second harmonic (octave up)

**Implementation Notes:**
- **Angle wrapping critical:** Taylor series diverges rapidly for |x| > π
- **Intermediate clamping:** Each taylor_sine clamped to ±1.5
- **Final output clamp:** Result clamped to ±1.0 for audio safety
- Iterative computation avoids factorial/power explosion
- Peak RMS: ~0.7
- Lower term counts produce aliasing (intentional aesthetic)

**Convergence Behavior:**
- **1 term:** `sin(x) ≈ x` (linear ramp, harsh)
- **2 terms:** `sin(x) ≈ x - x³/6` (softened, cubic bend)
- **5 terms:** Good approximation within [-π, π]
- **10 terms:** Machine-precision sine within [-π, π]

**Spectral Characteristics:**
- Low term counts: Rich aliased harmonics (digital artifact)
- High term counts: Pure fundamental/harmonic (clean sine)
- Second harmonic adds octave content
- Blend parameter morphs between timbres

**Use Cases:**
- Educational: Visualize Taylor series convergence
- Lo-fi synthesis: Aliased/digital character (1-3 terms)
- Morphing oscillator: Smooth/harsh transitions
- Harmonic exploration: Fundamental + octave blending
- Spectral sculpting: Term count automation creates evolving aliasing

**Comparison with Other Algorithms:**
- Unlike Dirichlet Pulse (exact harmonics), Taylor creates aliasing
- Unlike Tanh waveshaping (smooth saturation), Taylor creates stepped approximations
- Unique aesthetic: "partial computation" of sine wave

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
