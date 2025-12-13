# Distortion Synthesis Algorithms: Deep Analysis & Novel Combinations

## Overview

Analysis of distortion synthesis techniques from Lazzarini's Csound Journal article, with focus on combinations and extrapolations for complex voice generation in synthesizers.

## Core Algorithms

### 1. Discrete Summation Formulae (DSF)

**Principle:** Generate N harmonics from single closed-form expression instead of N oscillators.

**Band-limited Pulse (Windham/Steiglitz):**
```
s(t) = sin((2N+1)θ/2) / sin(θ/2)
```
- Equal amplitude harmonics
- N controls bandwidth (adjustable "cutoff")
- Division-by-zero protection needed at discontinuities

**Moorer's Generalized DSF:**
```
s(t) = (sin(ω) - a·sin(ω-θ) - a^(N+1)·[sin(ω+(N+1)θ) - a·sin(ω+Nθ)]) / (1 - 2a·cos(θ) + a²)
```

**Key Parameters:**
- `a` - Spectral rolloff (0 < a < 1)
- `N` - Bandwidth/number of partials
- `ω/θ` ratio - Harmonic/inharmonic control (like c:m in FM)

**Non-bandlimited simplification:**
```
s(t) = (sin(ω) - a·sin(ω-θ)) / (1 - 2a·cos(θ) + a²)
```
- Simpler, no bandwidth limit
- Rolloff parameter `a` provides natural decay
- Very efficient for hardware

**Advantages:**
- Time-varying N emulates filter sweeps
- No actual filtering needed
- ω/θ ratios create harmonic/inharmonic spectra
- Extremely CPU efficient

---

### 2. Hyperbolic Tangent Waveshaping

**Principle:** Smooth the signum function's discontinuity to reduce aliasing while maintaining clipping character.

**Transfer Function:** `tanh(x)` over range ±π/50

**Key Characteristics:**
- Input amplitude = distortion index
- Higher drive = more harmonics (but more aliasing)
- Requires amplitude-dependent scaling function
- Nearly bandlimited if driven carefully

**Square to Sawtooth Conversion:**
```
sawtooth(t) = square(t) · (cos(ωt) + 1)
```
- Heterodyne square wave with cosine
- Generates missing even harmonics
- ~2.5dB disparity in 2nd harmonic (acceptable)
- Cheap way to get both waveforms

**Suggested Index Limit:**
```
index ≈ 100/(frequency · log₁₀(frequency))
```

---

### 3. Asymmetrical FM Synthesis

**Principle:** Add spectral asymmetry control to FM via ring modulation with exponential waveshaper.

**Formula:**
```
s(t) = cos(ωc + k·sin(ωm)) · exp(k·(r-1/r)·cos(ωm)/2)
```

**Key Parameter:**
- `r` - Symmetry control
- `r < 1` - Shifts spectral peak below carrier
- `r > 1` - Shifts spectral peak above carrier  
- `r = 1` - Standard FM/PM

**Implementation Note:**
- Actually PM (Phase Modulation) not FM
- Exponential needs normalization: divide by `exp(0.5k[r - 1/r])`
- Use lookup table from 0 to -50 for `exp(-x)`

**Advantages:**
- Beautiful animated, shifting timbres
- One extra parameter adds huge expressiveness
- Ties waveshaping and FM together elegantly

---

### 4. Phase-Aligned Formant (PAF)

**Principle:** Ring modulation of sinusoid carrier with complex exponentially-shaped spectrum.

**Basic Formula:**
```
s(t) = [1/(1+x²)] · cos(ωc·t + φshift)
where x = 2√g·sin(ωm·t/2) / (1-g)
and g = exp(-ωm/B)
```

**Key Parameters:**
- `ωc` - Formant center frequency
- `ωm` - Fundamental frequency
- `B` - Bandwidth (controls formant width)
- `φshift` - Frequency shift for inharmonicity

**Formant Interpolation:**
For arbitrary center frequencies between harmonics:
- Use two carriers: `ωc1 = n·ωm` and `ωc2 = (n+1)·ωm`
- Linearly interpolate: `output = α·carrier1 + (1-α)·carrier2`
- Allows smooth formant frequency sweeps

**Transfer Function:** `f(x) = 1/(1+x²)`
- Creates Gaussian-like spectrum
- Can use GEN20 Gaussian table as approximation
- Direct computation or lookup table

**Advantages:**
- Excellent for vowel-like synthesis
- Smooth spectral bumps (formants)
- Low computational cost
- Equivalent to one of Moorer's DSF equations (cosine formulation)

---

### 5. Modified FM (ModFM)

**Principle:** Change FM's complex exponential variable `z = -ik` instead of `z = ik`.

**Standard FM expansion:**
```
s(t) = Σ Jn(k)·cos((ωc + n·ωm)t)
```

**Modified FM expansion:**
```
s(t) = exp(-k) · Σ In(k)·cos((ωc + n·ωm)t)
```

**Key Difference:** Uses Modified Bessel Functions `In(k)` instead of Bessel Functions `Jn(k)`

**Modified Bessel Properties:**
1. **Unipolar** - Always positive
2. **Monotonic decay** - `In(k) > In+1(k)` for all n
3. **No wobble** - Natural spectral evolution unlike standard Bessels

**Implementation:**
```
carrier = cos(ωc·t)
modulator = cos(ωm·t)
output = carrier · exp(-k·(modulator - 1))
```
- Exponential lookup table (0 to -50)
- Very compact algorithm
- Can emulate low-pass and band-pass filters

**Advantages:**
- Most natural-sounding FM variant
- Eliminates FM's "unnatural wobble"
- Excellent for filter-like effects
- Can replace PAF in some contexts (phase-sync variant)

---

## Powerful Combinations for Complex Voices

### 🎯 Combination 1: Hybrid Formant Engine

**Architecture:**
```
ModFM Base Oscillator
    ↓
PAF Formant 1 (800 Hz)
    ↓
PAF Formant 2 (1200 Hz)
    ↓
PAF Formant 3 (2400 Hz)
    ↓
Output
```

**Rationale:**
- ModFM provides fundamental with natural spectral evolution
- Multiple PAF layers create vowel-like formant regions
- Independent control: pitch vs formant positions
- Each formant has bandwidth control

**Control Parameters:**
- ModFM c:m ratio, index
- Each PAF: center frequency, bandwidth, level
- Optional frequency shift for breath/noise

**Best For:**
- Voice-like lead synthesizers
- Evolving pad sounds
- Speech-like textures
- Organic, animated timbres

---

### 🎯 Combination 2: Cascaded Spectral Sculptor

**Architecture:**
```
DSF (harmonic base)
    ↓
Asymmetric FM (spectral shifting)
    ↓
Tanh Waveshaping (character)
    ↓
Output
```

**Rationale:**
- DSF creates controllable harmonic foundation via `a` parameter
- Asymmetric FM shifts spectral peaks dynamically via `r`
- Waveshaping adds final timbral character

**Parameter Mapping:**
- DSF `a`: 0.5→0.95 (sparse to dense harmonics)
- DSF ω/θ: 1.0→2.5 (harmonic to inharmonic)
- Asymmetric FM `r`: 0.5→2.0 (shift down/up)
- Asymmetric FM `k`: 0→10 (brightness)
- Tanh drive: 0→1 (saturation)

**Best For:**
- Extremely animated, morphing leads
- Modular-style complex oscillators
- Sound design flexibility
- Aggressive, modern synthesis

---

### 🎯 Combination 3: Parallel Distortion Bank

**Architecture:**
```
ModFM (c:m = 1:1, index_1) ─┐
ModFM (c:m = 3:2, index_2) ─┤
ModFM (c:m = 4:3, index_3) ─┼─→ Mixer → Output
PAF (formant @ 800Hz)      ─┤
PAF (formant @ 2400Hz)     ─┘
```

**Rationale:**
- Multiple ModFM voices at different frequency ratios
- Creates complex, non-repeating beating patterns
- PAF formants add spectral "bumps" and resonance
- Controllable mix ratios for timbral balance

**Voice Configuration:**
- Voice 1: 1:1 ratio (fundamental character)
- Voice 2: 3:2 ratio (perfect fifth flavor)
- Voice 3: 4:3 ratio (perfect fourth flavor)
- Formant 1: Low formant (warmth)
- Formant 2: High formant (brightness)

**Best For:**
- Polysynth-quality voices from single oscillator
- Rich, organic complexity
- Chorused, thick timbres
- Strings, pads, evolving textures

---

### 🎯 Combination 4: Feedback Distortion Network

**Architecture:**
```
Input → ModFM → [Gain Control] → Feedback to ModFM input
              ↓
          Output
```

**Rationale:**
- Feedback creates chaotic, evolving spectra
- ModFM handles feedback more gracefully than standard FM
- No Bessel wobble = more musical feedback behavior
- Careful gain staging prevents runaway

**Implementation Details:**
- Feedback gain: 0→0.95 (stable to chaotic)
- Low-pass filter in feedback path (optional stabilization)
- Sample delay in feedback path (prevent zero-delay)

**Best For:**
- Metallic, bell-like timbres
- FM-style metallic percussion
- Aggressive, distorted leads
- Unpredictable, organic evolution

---

### 🎯 Combination 5: Morphing Spectral Engine

**Architecture:**
```
DSF ───┐
       ├─→ Crossfader (position α) → Output
ModFM ─┤
       │
PAF ───┘
```

**Rationale:**
- Continuously morph between algorithm types
- Each algorithm has independent spectral evolution
- Smooth timbral transitions via crossfading
- Expressive performance control

**Morph Positions:**
- α = 0.0: Pure DSF (harmonic series)
- α = 0.33: DSF→ModFM blend
- α = 0.5: Pure ModFM (natural evolution)
- α = 0.66: ModFM→PAF blend  
- α = 1.0: Pure PAF (formant synthesis)

**Best For:**
- Live performance control
- Sound design workflows
- Exploring timbral space
- Preset morphing

---

### 🎯 Combination 6: Inharmonic Resonator

**Architecture:**
```
DSF (non-integer ω/θ ratio)
    ↓
PAF (with frequency shift ≠ 0)
    ↓
Output
```

**Rationale:**
- Non-integer ω/θ ratios create bell-like inharmonicity
- PAF frequency shift adds additional inharmonic components
- Time-varying ratios = evolving metallicness
- Both algorithms contribute to inharmonic character

**Parameter Ranges:**
- DSF ω/θ: √2, φ (golden ratio), other irrationals
- PAF frequency shift: 5→50 Hz (subtle to extreme)
- Time modulation of both parameters

**Best For:**
- Bell synthesis
- Gong and metallic percussion
- Struck/plucked instrument modeling
- Cinematic metallic textures

---

### 🎯 Combination 7: Adaptive Filter Emulation

**Architecture:**
```
DSF (time-varying N, a) + ModFM (time-varying k)
    ↓
Mixed output
```

**Rationale:**
- DSF N parameter acts as "cutoff frequency"
- DSF a parameter acts as "resonance/Q"
- ModFM k provides additional spectral shaping
- No actual filters needed

**Envelope Mapping:**
- Filter cutoff → DSF N: 1→50 partials
- Filter resonance → DSF a: 0.5→0.99
- Filter character → ModFM k: 0→15
- All parameters envelope-controllable

**Best For:**
- Acid-style sounds (303 emulation)
- Dynamic timbral animation
- Percussive sounds with evolving spectra
- CPU-efficient "filtered" synthesis

---

## Novel Extrapolations

### Multi-Stage Waveshaping Cascade

**Architecture:**
```
Input → Tanh → Exponential (PAF-style) → Ring Mod with Carrier → Output
```

**Rationale:**
- Each stage adds different spectral characteristics
- Tanh: smooths and reduces aliasing
- Exponential: shapes spectrum like PAF
- Ring mod: shifts and spreads spectrum
- All parameters independently controllable

**Parameter Control:**
- Tanh drive: 0→10
- Exponential depth: 0→5
- Ring mod carrier frequency: 0.5x→5x input frequency
- Per-stage mix controls

---

### Frequency-Dependent Asymmetry

**Concept:** Use different `r` values for different frequency regions.

**Implementation:**
```
Low freq band (0-500Hz): r < 1 (shift down)
Mid freq band (500-2kHz): r ≈ 1 (neutral)
High freq band (2k+Hz): r > 1 (shift up)
```

**Rationale:**
- Creates unnatural but interesting spectral tilts
- Emphasis control for different frequency regions
- Novel timbral shaping not found in nature

---

### Cross-Algorithm Modulation

**Connections:**
- DSF's `a` parameter modulates ModFM's `k`
- PAF's formant position modulates Asymmetric FM's `r`
- ModFM's index modulates DSF's N

**Rationale:**
- Creates coupled, organic spectral evolution
- Multiple parameters evolve in related ways
- More natural-sounding animation than independent LFOs

---

### Stereo Spectral Splitting

**Implementation:**
```
Left channel: DSF with ω/θ = n
Right channel: DSF with ω/θ = n + small_offset (0.01→0.05)
```

**Rationale:**
- Creates stereo width from inharmonicity differences
- Beating patterns create movement in stereo field
- No delay or traditional stereo processing needed

---

## Hardware Implementation Recommendations

### For Bluemchen (or similar embedded hardware)

**Tier 1: Most Efficient (Start Here)**

1. **ModFM** - Carrier + modulator + exponential lookup
   - ~5 operations per sample
   - Shared exp table with other algorithms
   
2. **Simple DSF** - Non-bandlimited version
   - ~8 operations per sample
   - No bandwidth limiting overhead
   
3. **Tanh Waveshaping** - Single lookup table
   - ~3 operations per sample
   - Great as post-processor

**Tier 2: Moderate Cost, High Impact**

4. **PAF** (single formant) - For vocal character
   - ~12 operations per sample
   - Worth it for formant synthesis
   
5. **Asymmetric FM** - Adds animation
   - ~8 operations per sample
   - Significant expressive value

---

### Recommended Complete Voice Architecture

```
ModFM Base Oscillator
    ↓
[Optional] Tanh Waveshaping (overdrive)
    ↓
Ring Modulation with Simple DSF
    ↓
Output
```

**Control Parameters:**
- **ModFM:**
  - c:m ratio: 1:1 → 8:1 (timbre)
  - Index: 0 → 15 (brightness)
  
- **DSF:**
  - ω/θ ratio: 1.0 → 3.0 (harmonic to inharmonic)
  - Rolloff `a`: 0.5 → 0.95 (spectral density)
  
- **Tanh:**
  - Drive: 0 → 1 (saturation amount)
  
- **Mix:**
  - Dry/Wet: 0 → 1 (DSF blend amount)

**Why This Works:**
- Natural spectral evolution from ModFM
- Additional harmonic control from DSF
- Optional character from Tanh
- Complex animation from minimal CPU
- Single shared exponential lookup table
- Single shared sine/cosine wavetable

**CPU Budget Estimate:**
- ModFM: 5 ops/sample
- Tanh (if used): 3 ops/sample  
- DSF: 8 ops/sample
- Ring mod: 2 ops/sample
- **Total: ~18 operations per sample**

At 48kHz sample rate: ~864k operations/second (very achievable)

---

## Lookup Table Requirements

All algorithms share common tables:

1. **Sine/Cosine Table** (16384 samples)
   - Used by: ALL algorithms
   - Interpolated lookup
   
2. **Exponential Table** (8192 samples, 0 to -50)
   - Used by: ModFM, Asymmetric FM, PAF
   - `exp(-x)` for x ∈ [0, 50]
   
3. **Tanh Table** (16384 samples, -π to +π)
   - Used by: Waveshaping
   - Optional but valuable
   
4. **Gaussian Table** (8192 samples)
   - Used by: PAF (alternative to computing 1/(1+x²))
   - GEN20 approximation

**Total ROM: ~50KB for all tables**

---

## Key Insights

### Why These Work So Well

1. **Spectral Density from Simplicity**
   - Each algorithm generates dozens of partials
   - From just 2-3 base oscillators
   - Far more efficient than additive synthesis

2. **Natural Evolution**
   - Modified Bessels (ModFM) avoid FM's "wobble"
   - DSF rolloff parameter creates organic changes
   - Parameters map to musical concepts (brightness, formants)

3. **Interchangeable Interpretations**
   - Most techniques are mathematically related
   - Can combine or substitute algorithms
   - Hybrid approaches are natural

4. **Parameter Coupling**
   - Index of modulation = brightness = filter cutoff
   - Single parameter controls complex spectral changes
   - Intuitive for sound design

### Why Perfect for Hardware Synths

- **Low CPU overhead** - Generate 20+ partials from single oscillator
- **Shared resources** - Same lookup tables across algorithms
- **Expressive** - Few parameters control complex spectra
- **Aliasing management** - Most techniques are inherently bandlimited or nearly so
- **Real-time** - No offline processing or analysis needed
- **Musical** - Parameters map to familiar concepts

---

## Further Research Directions

1. **Machine Learning Spectral Matching**
   - Train models to find optimal parameter combinations
   - Match target spectra using distortion synthesis
   - Real-time parameter interpolation

2. **Waveguide Integration**
   - Use distortion oscillators as waveguide excitation
   - Combine with physical modeling
   - Hybrid synthesis approaches

3. **Granular Distortion**
   - Apply these techniques at grain level
   - Microsound with complex spectra
   - Novel textures

4. **Multi-Rate Processing**
   - High-rate modulation, low-rate output
   - Oversampling for quality
   - Decimation strategies

5. **Spectral Freezing**
   - Capture spectrum at interesting moments
   - Morph between frozen states
   - Dynamic spectral snapshots

---

## Conclusion

These distortion synthesis techniques represent elegant solutions to generating complex, evolving spectra from minimal computational resources. By understanding their mathematical foundations and experimenting with novel combinations, we can create synthesis engines that rival the complexity of much more expensive additive or sample-based approaches.

For hardware implementations, the key is starting with the most efficient algorithms (ModFM, simple DSF) and building up complexity as CPU budget allows. The shared lookup table architecture and parameter coupling make these techniques particularly suitable for embedded systems.

The "forgotten" algorithms like ModFM and Asymmetric FM deserve renewed attention - they offer musical advantages over their more famous cousins while maintaining comparable efficiency.

---

*Analysis based on: Victor Lazzarini, "Distortion Synthesis," Csound Journal Issue 11*
*Extended analysis by Danny Ayers, December 2024*
