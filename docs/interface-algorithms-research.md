# Interface Module Algorithm Research & Enhancement Proposals

## Executive Summary

The current Interface module implements eight excitation types with varying levels of physical accuracy. This research identifies sophisticated physically-based algorithms that could dramatically improve realism, along with hypothetical algorithms implied by the existing set. All proposals are designed to work with a single Intensity parameter while supporting the existing delay-line resonator architecture.

---

## Current State Analysis

### Existing Interface Types

The current implementation includes:

1. **Pluck** - One-way damping with transient brightening
2. **Hit** - Sine-fold waveshaping for percussive strikes  
3. **Reed** - Biased clarinet-style saturation
4. **Flute** - Soft jet response with breath noise
5. **Brass** - Asymmetric lip buzz
6. **Bow** - Stick-slip friction with controllable grip
7. **Bell** - Metallic partial generator
8. **Drum** - Energy-accumulating membrane drive

### Identified Weaknesses

- **Pluck**: Uses simple damping rather than Karplus-Strong energy injection
- **Hit**: Sine-fold is not physically motivated
- **Bow**: Friction model lacks proper stick-slip hysteresis
- **Bell/Drum**: Nonlinearity insufficient for inharmonic metallic timbres

---

## Research Findings: Sophisticated Physical Models

### 1. Enhanced Karplus-Strong Pluck Algorithm

**Physical Basis**: Refined model of initial string displacement profile and energy injection.

**Key Improvements**:
- **Triangular displacement profile** with adjustable pick position
- **Initial velocity burst** separate from displacement
- **Dispersion filtering** for more realistic high-frequency decay
- **Bridge damping** via fractional delay adjustment

**Implementation Strategy**:
```
Intensity Parameter Maps To:
- 0.0 → Soft pluck (wide profile, low velocity)
- 0.5 → Moderate pluck (medium profile, medium velocity)  
- 1.0 → Hard pluck (narrow profile, high velocity + noise burst)

Algorithm Components:
1. Generate triangular initial condition: 
   width = f(1-Intensity), amplitude = f(Intensity)
2. Add velocity impulse: v₀ = Intensity × noise_burst
3. One-shot injection into delay line
4. Optional: fractional-delay dispersion filter in loop
```

**Architectural Requirements**:
- Initial condition buffer (small, ~200 samples max)
- One-shot trigger mechanism on gate
- Optional: allpass dispersion filter in delay loop

**References**: Smith & Van Duyne (1993), Karjalainen et al. (1993)

---

### 2. Physics-Based Hammer-String Collision Model

**Physical Basis**: Felt or wood mallet striking string with nonlinear contact compliance.

**Key Physics**:
- **Nonlinear spring**: Stiffness increases exponentially with compression
- **Hysteresis**: Different loading/unloading curves
- **Duration control**: Mass and compliance determine contact time
- **Spectral shaping**: Harder hammers = more high frequencies

**Implementation Strategy**:
```
Intensity Parameter Maps To:
- 0.0 → Soft felt (α=2.0, long contact)
- 0.5 → Medium hardness (α=2.5, moderate contact)
- 1.0 → Hard wood (α=3.5, short contact)

Force Model:
F(x) = K × x^α  (where x = compression depth)

Alpha (α) = 1.5 + Intensity × 2.0
Mass = 0.01 - Intensity × 0.008

Algorithm:
1. On gate: initialize mallet position and velocity
2. Per sample: compute collision force if contact
3. Inject force into delay line as position/velocity change
4. Track mallet position until separation
```

**Architectural Requirements**:
- Collision state machine (4 states: approach, contact, release, rest)
- Nonlinear force computation (power function)
- Integration for mallet dynamics (2 state variables)

**References**: Van Duyne & Smith (1995), Chaigne & Askenfelt (1994)

---

### 3. Advanced Friction-Based Bow Model

**Physical Basis**: Stick-slip friction with thermal and elasto-plastic effects.

**Key Physics**:
- **Friction curve**: Velocity-dependent force with hysteresis
- **Thermal effects**: Heating during slip modifies friction
- **Stribeck curve**: Force drops at higher velocities
- **Torsional modes**: Bow hair twisting adds complexity

**Implementation Strategy**:
```
Intensity Parameter Maps To:
- 0.0 → Light bowing (low normal force, sul tasto)
- 0.5 → Normal bowing (moderate force)
- 1.0 → Heavy bowing (high force, sul ponticello + noise)

Friction Model:
F_friction = F_normal × μ(v_rel, state)

Where μ uses enhanced model:
μ(v) = μ_s + (μ_d - μ_s) × exp(-|v|/v_s) 
     + μ_v × v  [Stribeck + viscous damping]

F_normal = Intensity × base_force
v_s = 0.01 + Intensity × 0.09  (slip velocity)

Algorithm:
1. Read string velocity from delay tap
2. Compute relative velocity: v_rel = v_bow - v_string  
3. Compute friction force from enhanced curve
4. Inject force as torque into string
5. Track thermal state for long notes
```

**Architectural Requirements**:
- Delay line tap for velocity sensing
- Friction state machine (stick/slip detection)
- Thermal integrator (1st order filter)
- Optional: torsional mode coupling

**References**: McIntyre & Woodhouse (1979), Serafin et al. (2002), Desvages & Bilbao (2016)

---

### 4. Woodwind Reed Valve Model with Embouchure

**Physical Basis**: Clarinet reed as pressure-controlled valve with flow-induced oscillation.

**Key Physics**:
- **Reed opening**: Modulated by pressure difference
- **Bernoulli flow**: Volume flow through variable orifice  
- **Reed resonance**: Natural frequency affects response
- **Embouchure**: Lip force modifies reed compliance

**Implementation Strategy**:
```
Intensity Parameter Maps To:
- 0.0 → Soft reed (high compliance, low threshold)
- 0.5 → Medium reed (moderate compliance)
- 1.0 → Stiff reed (low compliance, high threshold)

Reed Model (simplified):
y = y₀ × [1 - Δp/(p_close × Intensity)]  (reed opening)
flow = C × y × sqrt(|Δp|) × sign(Δp)  (Bernoulli)

Where:
- y₀ = equilibrium opening
- Δp = p_mouth - p_bore (from delay line feedback)
- p_close = closing pressure (increases with Intensity)

Algorithm:
1. Read bore pressure from delay line
2. Compute pressure difference with DC source
3. Calculate reed opening (clamp to [0, y₀])
4. Compute flow through opening
5. Inject flow as pressure wave into bore
```

**Architectural Requirements**:
- Pressure-to-flow conversion
- Reed dynamics (optional: 2nd order resonance)
- Flow limiting and clamping
- Sqrt and sign functions

**References**: McIntyre et al. (1983), Scavone (1997), Gilbert et al. (1989)

---

### 5. Jet-Drive Flute Model with Vortex Shedding

**Physical Basis**: Air jet striking labium, with edge-tone instability and vortex formation.

**Key Physics**:
- **Jet dynamics**: Time delay for jet travel
- **Edge geometry**: Sharp vs. rounded affects oscillation
- **Vortex shedding**: Periodic flow separation
- **Hydrodynamic coupling**: Strong amplitude dependence

**Implementation Strategy**:
```
Intensity Parameter Maps To:
- 0.0 → Soft air stream (low jet velocity, weak coupling)
- 0.5 → Moderate jet (normal playing)
- 1.0 → Overblowing (high velocity, increased nonlinearity)

Jet Model:
y_jet(t) = U₀ + U₁ × [y_mouth(t - τ) - y_bore(t)]

Where:
- U₀ = DC jet velocity = Intensity × max_velocity  
- U₁ = feedback gain = 0.3 + Intensity × 0.5
- τ = jet delay = distance / velocity
- y_mouth = mouth cavity resonance (or noise source)
- y_bore = bore feedback from delay line

Labium response:
flow_in = C × tanh(β × y_jet)  (soft clipping)
β = 2.0 + Intensity × 8.0  (nonlinearity strength)

Algorithm:
1. Generate/read mouth signal (noise + resonance)
2. Read bore feedback from delay
3. Compute jet displacement with delay  
4. Apply labium nonlinearity
5. Inject flow into bore
```

**Architectural Requirements**:
- Fractional delay line for jet travel time
- Mouth cavity resonance (optional: 2nd order filter)
- Smooth nonlinearity (tanh or polynomial)
- Noise generator for turbulence

**References**: Fletcher & Rossing (1991), Verge et al. (1997), Chafe (1990)

---

### 6. Lip-Reed Brass Model with Mechanical Resonance

**Physical Basis**: Lip valve oscillating under pressure, with nonlinear compliance and inertia.

**Key Physics**:
- **Two-mass model**: Upper and lower lips with coupling
- **Bernoulli opening**: Pressure-controlled valve  
- **Lip resonance**: Natural frequency affects timbre
- **Asymmetric opening**: Outward/inward travel different

**Implementation Strategy**:
```
Intensity Parameter Maps To:
- 0.0 → Soft embouchure (low tension, low resonance)
- 0.5 → Normal embouchure (moderate tension)
- 1.0 → Tight embouchure (high tension, high resonance, brassy)

Simplified Single-Mass Lip Model:
m × ÿ + r × ẏ + k × y = p_mouth - p_bore

Where:
- k = stiffness = (1 + Intensity)² × k₀  (frequency scales)
- m = effective mass (fixed)
- r = damping = 0.1 + Intensity × 0.5

Valve Flow:
opening = y₀ + y(t)  (lip displacement)
flow = max(0, opening) × sqrt(|Δp|) × sign(Δp)

Algorithm:
1. Read bore pressure from delay
2. Compute pressure difference  
3. Update lip dynamics (2nd order system)
4. Compute valve opening (clamp to positive)
5. Calculate flow through lips
6. Inject into bore as traveling wave
```

**Architectural Requirements**:
- 2nd-order resonator for lip dynamics
- Pressure-to-flow conversion with sqrt
- Asymmetric valve behavior
- Optional: two-mass model for richer harmonics

**References**: Adachi & Sato (1996), Cullen et al. (2000), Kemp et al. (2022)

---

### 7. Enhanced Bell/Gong with Geometric Nonlinearity

**Physical Basis**: Large-amplitude vibrations of thin shells cause mode coupling and pitch glides.

**Key Physics**:
- **Tension modulation**: Vibration stretches material, raising pitch
- **Mode coupling**: Energy transfers between partials
- **Inharmonic spectrum**: Circular shell eigenfrequencies
- **Slow energy redistribution**: Characteristic of gongs/cymbals

**Implementation Strategy**:
```
Intensity Parameter Maps To:
- 0.0 → Small-signal linear behavior (bell-like)
- 0.5 → Moderate nonlinearity (shimmer and pitch glide)
- 1.0 → Strong nonlinearity (crash cymbal, fast spreading)

Tension Modulation Model:
ΔL/L = γ × (amplitude)²  (geometric stretching)
f(t) = f₀ × sqrt(1 + ΔL/L)  (instantaneous pitch)

Where:
- γ = nonlinearity coefficient = Intensity × 0.1

Mode Coupling (via passive nonlinear filter):
mode_coupler_output = input + α × input³

Where:
- α = Intensity × 0.05
- Applied at delay line termination

Algorithm:
1. Read signal from delay line(s)
2. Compute instantaneous energy E = x²
3. Modulate delay length: L → L × sqrt(1 + γ×E)
4. Apply mode coupling: x → x + α×x³  
5. Feed back to delay lines
```

**Architectural Requirements**:
- Energy measurement (RMS or peak tracking)
- Variable delay length (fractional interpolation)
- Cubic nonlinearity (x³ computation or LUT)
- Stability monitoring (energy conservation)

**References**: Chaigne & Doutaut (1997), Van Duyne & Smith (1995), Bilbao (2004)

---

### 8. Nonlinear Membrane Drum Model

**Physical Basis**: 2D membrane with amplitude-dependent tension causing pitch glides.

**Key Physics**:
- **Geometric nonlinearity**: Large deflections stretch membrane
- **Tension increase**: Raises resonant frequencies
- **Inharmonic spectrum**: 2D eigenmodes (Bessel functions)
- **Impact transients**: Energy injection from mallet

**Implementation Strategy**:
```
Intensity Parameter Maps To:
- 0.0 → Linear small-amplitude (tom-tom)
- 0.5 → Moderate nonlinearity (tabla with pitch bend)
- 1.0 → Strong nonlinearity (bass drum "thump")

For 1D waveguide approximation:
ρ_effective = ρ₀ + η × (u_center)²

Where:
- ρ₀ = base wave speed parameter (~0.25)
- η = tension modulation = Intensity² × 0.15  
- u_center = instantaneous center amplitude

Applied via delay modulation:
delay_length(t) = L₀ / sqrt(ρ_effective(t))

Algorithm (lumped model):
1. Monitor peak amplitude in delay lines
2. Compute tension modulation: ΔT ∝ (amplitude)²
3. Modulate delay line lengths proportionally
4. Apply to both delay lines if using dual setup
5. Ensure stability: keep ρ_effective < 0.5

Alternative (modal synthesis):
1. Bank of resonators at inharmonic frequencies
2. Couple modes via shared tension modulation
3. Each mode sees: f_i(t) = f_i0 × sqrt(1 + Σ E_j)
```

**Architectural Requirements**:
- Amplitude tracking (peak detector or RMS)
- Delay modulation with interpolation
- Stability bounds checking
- Optional: modal decomposition with ~6-12 modes

**References**: Avanzini & Rocchesso (2001), Bilbao (2004), Chaigne & Askenfelt (1994)

---

## Hypothetical Interface Algorithms

These algorithms are implied by the existing physical models but represent more extreme or fantastical behaviors.

### H1. Crystal - Idealized Perfectly Inharmonic Resonator

**Concept**: Simulates a crystalline structure with completely arbitrary partial ratios, unlike bell/gong which maintain some relationship.

**Physical Justification**: Quasicrystalline materials exhibit inharmonic vibrational modes. Extends metal percussion to extreme inharmonicity.

**Implementation**:
```
Intensity Parameter Maps To:
- 0.0 → Near-harmonic (ratio = 1.0)
- 0.5 → Mildly inharmonic (golden ratio partials)
- 1.0 → Chaotic inharmonicity (prime number ratios)

Delay Line Setup:
- Use 3-4 delay lines with independent lengths
- Ratios: [1.0, φ, φ², φ³] where φ = 1.618...
- Or: [1.0, π/2, √2, √3, √5]

Nonlinearity:
- Cross-coupling between lines via product terms
- out_i = delay_i + Intensity × Σ(delay_j × delay_k)

This creates sum/difference tones at irrational ratios.
```

**Architectural Requirements**:
- 3-4 independent delay lines
- Cross-product computations
- Ring modulation or frequency shifting

---

### H2. Vapor - Chaotic Aeroacoustic Turbulence

**Concept**: Models extreme turbulent flow behavior where coherent oscillation breaks down into broadband chaos.

**Physical Justification**: Extends flute jet model to supersonic velocities where shock waves and vortex breakdown dominate. Loosely based on supersonic jet noise.

**Implementation**:
```
Intensity Parameter Maps To:
- 0.0 → Stable jet (flute-like)
- 0.5 → Transition to turbulence (flutter)
- 1.0 → Fully chaotic turbulent screech

Chaos Injection:
1. Couple delay feedback to nonlinear map:
   x_n+1 = r × x_n × (1 - x_n)  [logistic map]
   
2. r parameter driven by Intensity:
   r = 2.5 + Intensity × 1.5
   
3. At r > 3.57, map enters chaos
4. Inject chaotic signal as turbulent forcing

Turbulence Model:
- Multiple delay lines with incommensurate lengths
- Each modulated by independent chaotic oscillator
- Sum creates dense, evolving spectral texture
```

**Architectural Requirements**:
- Chaotic oscillator (logistic map or similar)
- Multiple feedback paths with chaos injection
- Heavy low-pass filtering to prevent aliasing

---

### H3. Quantum - Amplitude-Quantized Resonator

**Concept**: Resonator where amplitude can only exist at discrete "quantum" levels, causing artificial harmonic distortion and zipper noise.

**Physical Justification**: Inspired by quantum mechanical harmonic oscillator but not physically real. Creates interesting artifact-based timbres.

**Implementation**:
```
Intensity Parameter Maps To:
- 0.0 → Continuous amplitude (normal)
- 0.5 → 8-bit quantization (lo-fi)
- 1.0 → 3-bit quantization (extreme zipper)

Quantization:
levels = 2^(8 - floor(Intensity × 7))
output = round(input × levels) / levels

Applied to delay line feedback path.

This creates quantization distortion with:
- Harmonic generation at high levels
- Chaotic behavior near zero crossings
- Sample-rate dependent artifacts
```

**Architectural Requirements**:
- Bit-depth reduction in feedback path
- Optional: noise shaping to push artifacts higher
- Anti-aliasing considerations

---

### H4. Plasma - Electromagnetic Waveguide with Nonlinear Dispersion

**Concept**: Models electromagnetic wave propagation in ionized gas where wave speed depends on intensity.

**Physical Justification**: Loosely inspired by plasma physics and nonlinear optics (self-focusing). Creates sci-fi timbres.

**Implementation**:
```
Intensity Parameter Maps To:
- 0.0 → Linear dispersion (normal delay)
- 0.5 → Moderate self-focusing
- 1.0 → Strong self-focusing and harmonic generation

Nonlinear Delay:
delay_time(t) = τ₀ / [1 + β × |signal(t)|]

Where:
- β = Intensity × 0.3
- High amplitude → faster propagation
- Creates amplitude-to-frequency conversion

Dispersion:
- Frequency-dependent delay via allpass filters
- Modulate allpass coefficients with amplitude
```

**Architectural Requirements**:
- Amplitude-dependent delay modulation
- Allpass filter bank with dynamic coefficients
- Envelope follower for amplitude detection

---

## Architectural Integration Strategy

### Unified Interface Module Structure

All interface algorithms share common architectural patterns:

```
class InterfaceModule {
  // Common parameters
  - type: InterfaceType (enum)
  - intensity: float [0,1]
  
  // Shared state
  - gateState: bool
  - previousGate: bool
  - phaseState: float[4]  // Multi-purpose state variables
  
  // Shared buffers  
  - delayBuffer: float[256]  // For initialization/transients
  - stateBuffer: float[16]   // Algorithm-specific state
  
  // Core interface
  + process(input, cv, gate): float
  + setIntensity(value): void
  + setType(type): void
  + reset(): void
  
  // Type-specific processing
  - processPluck(): float
  - processHit(): float
  - processReed(): float
  ... etc
}
```

### Parameter Mapping Philosophy

Each interface type maps Intensity to relevant physical parameters:

- **Soft (0.0)**: Gentle excitation, low coupling, linear behavior
- **Medium (0.5)**: Normal playing conditions  
- **Hard (1.0)**: Extreme playing, high coupling, strong nonlinearity

This ensures intuitive control across wildly different physical models.

### Computational Considerations

**Per-Sample Operations Budget**: ~20-50 operations per algorithm

**Complexity Tiers**:
1. **Simple** (5-15 ops): Pluck, Hit, Crystal, Quantum
2. **Moderate** (15-30 ops): Reed, Flute, Brass  
3. **Complex** (30-50 ops): Bow, Bell, Drum, enhanced models

**Optimization Strategies**:
- Lookup tables for nonlinear functions (tanh, x³, sqrt)
- Polynomial approximations where possible
- Shared state across types (minimize memory)
- Fractional delay via Hermite interpolation (4-point, 3 multiplies)

### Shared Code Modules

**1. Nonlinearity Library**
- Soft clipper (tanh approximation)
- Hard clipper (clamp)
- Power function (x^α via exp/log)
- Polynomial waveshaper
- Friction curve generator

**2. Excitation Generators**  
- Triangular profile generator (pluck)
- Noise burst generator (hit)
- Chaotic oscillator (vapor, quantum)

**3. Delay Modulation**
- Fractional delay calculator
- Hermite interpolation
- Bounds checking for stability

**4. Energy Tracking**
- RMS estimator
- Peak envelope follower  
- Leaky integrator

### Stability Guarantees

**Energy-Based Stability**:
- All passive nonlinearities (input energy ≥ output energy)
- Delay modulation bounded to stable range
- Explicit energy monitoring with reset on overflow

**Numerical Stability**:
- State variable bounds checking
- Denormal prevention (add tiny DC offset)
- Anti-aliasing for chaotic systems

---

## Recommended Implementation Priority

### Phase 1: Core Physical Models (High Impact)
1. Enhanced Karplus-Strong Pluck
2. Advanced Friction Bow
3. Physics-Based Hammer

### Phase 2: Aeroacoustic Models (Medium Impact)  
4. Woodwind Reed with Embouchure
5. Jet-Drive Flute with Vortex

### Phase 3: Nonlinear Resonators (High Complexity)
6. Enhanced Bell/Gong
7. Nonlinear Membrane Drum

### Phase 4: Hypothetical Algorithms (Experimental)
8. Crystal (easiest hypothetical)
9. Quantum (artifact-based)
10. Vapor (chaos-based)
11. Plasma (most complex)

---

## Testing and Validation Strategy

### Unit Tests
- Parameter range validation (intensity 0-1)
- Energy conservation (for passive models)
- Stability under extreme inputs
- Gate triggering behavior

### Perceptual Tests
- A/B comparison with recordings
- Expressive range evaluation  
- Intensity parameter linearity
- Interface contrast/distinctiveness

### Performance Tests
- CPU cycle count per sample
- Memory footprint
- Real-time audio buffer performance

---

## Conclusion

The proposed algorithms span a spectrum from rigorous physical models (Karplus-Strong, bow friction, reed valve) to speculative hypothetical systems (crystal, vapor, plasma, quantum). All maintain the architectural principle of single-parameter Intensity control while offering dramatically improved realism or novel sonic possibilities.

**Key Advantages**:
- Maintains existing delay-line resonator architecture
- Single Intensity parameter sufficient for meaningful variation
- Shared code reduces implementation complexity
- Clear upgrade path from current implementation
- Hypothetical algorithms add creative sound design capabilities

**Implementation Feasibility**:
- Core algorithms: 20-50 operations per sample (feasible)
- Shared modules reduce code duplication
- Progressive enhancement possible (start simple, add detail)
- Real-time performance achievable on modern hardware

The research demonstrates that the current Interface module, while functional, represents only a fraction of what's possible within the established architectural framework. The physically-based improvements would substantially increase realism, while the hypothetical algorithms expand the instrument's creative palette into uncharted sonic territory.
