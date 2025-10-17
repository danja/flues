# PM Synth Signal Flow Documentation

## Current Signal Flow (Updated 2025-10-17)

```
Sources (DC + Noise + Tone)
    ↓
Envelope (Attack/Release)
    ↓
    ├─→ Enveloped Signal
    |
Feedback Loop ─→ DC Blocker ─→ Clean Feedback
                                    ↓
                    Enveloped Signal + Clean Feedback
                                    ↓
                            Interface (Strategy)
                                    ↓
                              Delay Lines
                                    ↓
                                  Filter
                                    ↓
                    ┌───────────────┴───────────────┐
                    ↓                               ↓
              Feedback Module              Modulation (AM)
                    ↓                               ↓
              (loop back)                        Reverb
                                                    ↓
                                                 Output
```

## Key Design Decisions

### DC Blocker Placement

**Why DC blocker is on feedback only:**
- DC from sources provides constant "pressure" to bias interface operating point
- Essential for reed/brass/bow interfaces to sustain oscillation
- DC blocker on feedback prevents runaway DC accumulation in delay lines
- This architecture allows intentional DC bias while preventing unintentional DC buildup

### Signal Flow Details

1. **Sources → Envelope**
   - DC: 0-1 range, provides constant pressure
   - Noise: Bipolar, provides turbulence/excitation
   - Tone: Bipolar sawtooth, provides harmonic content
   - Envelope shapes the amplitude over time

2. **Feedback Processing**
   - Delay outputs + filter output mixed by FeedbackModule
   - DC blocker removes any DC offset from feedback loop
   - Clean feedback prevents DC buildup while preserving AC signal

3. **Interface Input**
   - Sum of enveloped sources (with DC bias) + clean feedback
   - Interface processes this combined signal
   - Different interface types respond differently to DC bias:
     - **Reed/Brass**: DC acts as breath/mouth pressure
     - **Bow**: DC provides sustained bow velocity
     - **Pluck/Hit**: DC less relevant (transient excitation)
     - **Hypothetical**: DC affects operating point uniquely

4. **Delay Lines**
   - Receive interface output
   - Two delay lines with tunable ratio
   - Create resonant modes/formants

5. **Feedback Loop**
   - Delay outputs fed back through FeedbackModule
   - Individual gain control for each delay line
   - Filter output can also be fed back
   - Creates sustained oscillation

6. **Modulation**
   - FM: Modulates delay line pitch before processing
   - AM: Modulates final output amplitude
   - Bipolar LFO: Center position = no modulation

## Interface Strategy Pattern

Each interface type is implemented as a separate strategy class that processes the input signal according to its physical model:

### Physical Models (8)
- **Pluck**: One-way damping, passes transients
- **Hit**: Sine-fold waveshaping, percussive
- **Reed**: Biased saturation, clarinet-like
- **Flute**: Soft nonlinearity with breath noise
- **Brass**: Asymmetric lip buzz
- **Bow**: Stick-slip friction with state
- **Bell**: Metallic overtones with phase evolution
- **Drum**: Energy accumulation with decay

### Hypothetical Models (4)
- **Crystal**: Inharmonic cross-coupling, golden ratio spacing
- **Vapor**: Chaotic turbulence, logistic map oscillators
- **Quantum**: Bit-depth quantization, zipper artifacts
- **Plasma**: Nonlinear dispersion, self-focusing

## DC Parameter Behavior

### At Different DC Levels:

**0% DC (0.0)**
- Minimal bias
- Interface operates around zero
- Requires noise/tone for sustained sound
- Best for transient interfaces (pluck, hit)

**50% DC (0.5) - Default**
- Moderate bias
- Balanced operating point
- Good for most interfaces
- Sustains oscillation with feedback

**100% DC (1.0)**
- Maximum bias
- Interface heavily biased toward positive
- Strong sustained pressure
- Can push some interfaces into saturation

### Interface-Specific DC Effects:

**Reed/Brass**
- DC = breath/mouth pressure
- Low DC = soft/airy tone
- High DC = forced/bright tone

**Bow**
- DC = bow velocity/pressure
- Affects stick-slip behavior
- Higher DC = more aggressive bowing

**Flute**
- DC = air stream intensity
- Affects jet instability threshold
- Too much DC can cause overblowing

**Pluck/Hit**
- DC has minimal effect
- These interfaces respond to transients
- Rely on envelope and noise for excitation

**Hypothetical**
- DC affects operating point uniquely
- Crystal: Bias for cross-coupling
- Vapor: Shifts chaos parameter space
- Quantum: Affects quantization center
- Plasma: Modifies dispersion characteristics

## Troubleshooting

### No Sound
- Check DC level (try 50%)
- Check Noise level (try 15%)
- Check Feedback levels (try 50% on both delays)
- Verify envelope attack isn't too long

### Sound Cuts Off Quickly
- Increase Release time
- Increase Feedback levels
- Check DC level (higher sustains longer)

### Distorted/Harsh Sound
- Reduce DC level
- Reduce Feedback levels
- Lower Interface Intensity
- Check for clipping (red indicators)

### Modulation Not Working
- Check LFO Frequency (try 5 Hz)
- Move Modulation knob away from center (50%)
- FM affects pitch, AM affects amplitude
- Both are applied correctly in current build

## References

- `interface-algorithms-research.md` - Detailed physical models and algorithms
- `interface-refactoring-summary.md` - Implementation details and architecture
- `adding-new-interface-guide.md` - How to extend with new interface types
