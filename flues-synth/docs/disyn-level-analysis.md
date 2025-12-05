# Disyn Level Analysis & Recommendations

**Date:** 2025-12-05
**Issue:** Broadband noise from excessive Disyn oscillator output levels

## Executive Summary

The Disyn oscillator algorithms produce raw peak levels ranging from **+0.06 to +2.15** at default parameters, and up to **+11.3** at parameter extremes. When combined with formant makeup gain (3.0×) and feedback, this causes signal levels to exceed ±1.0 and generate broadband noise.

## Test Results (48kHz, 440Hz, 0.5s duration)

### Default Parameters - Raw Output

| Algorithm        | Peak  | RMS    | Clips | Status     |
|------------------|-------|--------|-------|------------|
| Dirichlet Pulse  | +2.15 | 0.3675 | 858   | *** EXCESSIVE |
| DSF Single       | +1.71 | 0.7071 | 3633  | ** HIGH    |
| DSF Double       | +1.73 | 0.6611 | 2848  | ** HIGH    |
| Tanh Square      | +0.06 | 0.0414 | 0     | OK         |
| Tanh Saw         | +0.51 | 0.3619 | 0     | OK         |
| PAF              | +0.62 | 0.4245 | 0     | OK         |
| Modified FM      | +0.95 | 0.6366 | 0     | * LOUD     |

### After Current Synth Scaling (0.5 × 0.5 = 0.25×)

| Algorithm        | Raw Peak | Effective Peak | Notes |
|------------------|----------|----------------|-------|
| Dirichlet Pulse  | +2.145   | +0.536         | Still problematic after formant gain |
| DSF Single       | +1.709   | +0.427         | Still problematic after formant gain |
| DSF Double       | +1.730   | +0.432         | Still problematic after formant gain |
| Tanh Square      | +0.058   | +0.015         | Safe |
| Tanh Saw         | +0.513   | +0.128         | Safe |
| PAF              | +0.620   | +0.155         | Safe |
| Modified FM      | +0.948   | +0.237         | Safe |

### With Formant Makeup Gain (3.0×)

| Algorithm        | Effective Peak | Post-Formant Peak | Result |
|------------------|----------------|-------------------|--------|
| Dirichlet Pulse  | +0.536         | +1.608            | **CLIPS** |
| DSF Single       | +0.427         | +1.281            | **CLIPS** |
| DSF Double       | +0.432         | +1.296            | **CLIPS** |

**Conclusion:** The first three algorithms still clip after formant cascade, even with current scaling.

## Signal Flow & Amplification Stages

```
Disyn Raw Output (2.15 peak)
    ↓ ×0.5 (disyn_level)
    = 1.075
    ↓ Envelope (×1.0 at sustain)
    ↓ Formant Cascade (×3.0 makeup gain)
    = 3.225
    ↓ ×0.5 (global pad)
    = 1.613
    ↓ soft_clip_drive (tanh, already distorting)
    ↓ ×0.35 (master_gain)
    = 0.565 final output
```

Even though final output is <1.0, **intermediate stages exceed ±1.0**, causing:
1. Soft clipping distortion → harmonic distortion
2. Feedback loop amplification → runaway oscillation
3. Broadband noise accumulation

## Root Cause

The Disyn algorithms (particularly Dirichlet, DSF Single/Double) were designed for the LV2 plugin where:
- No formant cascade follows (different signal chain)
- Envelope and reverb provide different scaling
- No feedback loop

When ported to flues-synth, the algorithms encounter:
- **Formant cascade requiring 3.0× makeup gain**
- **Feedback loop that can amplify spikes**
- **Different scaling assumptions**

## Recommended Solutions

### Option 1: Reduce disyn_level (Quick Fix)

**Current:** `disyn_level = 0.5f`
**Recommended:** `disyn_level = 0.15f` (reduce by 70%)

**Rationale:**
```
Dirichlet Raw: 2.15
    × 0.15 (disyn_level)
    = 0.3225
    × 3.0 (formant makeup)
    = 0.9675  ← Just under unity!
```

**Change in:** `flues-synth/src/synth_engine.c:207`
```c
// OLD:
// engine->disyn_level = 0.5f;

// NEW:
engine->disyn_level = 0.15f;  // Reduced to accommodate formant cascade gain
```

**Pros:**
- One-line fix
- Preserves algorithm behavior
- Works for all algorithms

**Cons:**
- Reduces Disyn contribution to overall sound
- May need to increase noise/DC to compensate

### Option 2: Reduce formant_makeup_gain (Moderate Fix)

**Current:** `makeup_gain = 3.0f`
**Recommended:** `makeup_gain = 1.5f` (reduce by 50%)

**Rationale:**
- Formants naturally attenuate, but 3.0× may be excessive for this signal chain
- Reducing to 1.5× keeps formant presence while preventing clipping

**Change in:** `flues-synth/src/audio/modules/formant_bank_module.c:20`
```c
// OLD:
// bank->makeup_gain = 3.0f;

// NEW:
bank->makeup_gain = 1.5f;  // Reduced to prevent Disyn-induced clipping
```

**Pros:**
- Fixes the bottleneck in the signal chain
- Preserves Disyn contribution
- Reduces overall broadband noise accumulation

**Cons:**
- Formants may sound quieter (but cleaner)
- May need to adjust master_gain upward

### Option 3: Normalize Algorithms in C++ (Best Long-term Fix)

Add per-algorithm normalization in `lv2/disyn/src/modules/OscillatorModule.hpp`:

```cpp
// In OscillatorModule::process(), after algorithm calculation:

float normalized_output = raw_output;

// Per-algorithm normalization
switch (current_algorithm_) {
    case 0:  // Dirichlet Pulse
        normalized_output /= 2.2f;  // Measured max ~2.15
        break;
    case 1:  // DSF Single
        normalized_output /= 1.8f;  // Measured max ~1.71
        break;
    case 2:  // DSF Double
        normalized_output /= 1.8f;  // Measured max ~1.73
        break;
    case 3:  // Tanh Square (already well-behaved)
    case 4:  // Tanh Saw
    case 5:  // PAF
    case 6:  // Modified FM
    default:
        // No additional scaling needed
        break;
}

return normalized_output;
```

**Pros:**
- Fixes problem at the source
- Makes algorithms portable across contexts (LV2, flues-synth, future projects)
- Prevents parameter-induced explosions

**Cons:**
- Requires C++ changes
- Needs rebuild of both LV2 plugin and flues-synth
- May affect existing LV2 plugin users

### Option 4: Hybrid Approach (Recommended)

Combine Option 1 and Option 2:

1. **Reduce disyn_level to 0.2f** (more conservative than 0.15)
2. **Reduce formant_makeup_gain to 2.0f** (less aggressive than 1.5)

**Result:**
```
Dirichlet: 2.15 × 0.2 × 2.0 = 0.86  ← Safe!
DSF:       1.71 × 0.2 × 2.0 = 0.68  ← Safe!
```

**Changes:**
- `flues-synth/src/synth_engine.c:207`: `disyn_level = 0.2f;`
- `flues-synth/src/audio/modules/formant_bank_module.c:20`: `makeup_gain = 2.0f;`

**Pros:**
- Distributes the attenuation across two stages
- Formants still present (2.0× vs 3.0×)
- Disyn still audible (0.2 vs 0.15)
- Two small changes vs one large change

## Parameter Safety Ranges

To prevent future issues, consider clamping CC inputs before mapping:

| Parameter | Current Range | Safe Range | Rationale |
|-----------|---------------|------------|-----------|
| Algorithm 0 Param1 | 0.0-1.0 | 0.0-0.5 | Prevent >64 harmonics (peak +11.3) |
| Algorithm 1 Param1 | 0.0-1.0 | 0.0-0.7 | Prevent decay >0.9 (peak +9.95) |
| Algorithm 2 Param1 | 0.0-1.0 | 0.0-0.7 | Prevent decay >0.9 (peak +6.69) |

## Testing Plan

After implementing fixes:

1. **Run disyn-levels test again:**
   ```bash
   meson test -C builddir disyn-levels
   ```
   Verify effective peaks <1.0 after formant gain

2. **Run engine-smoke test:**
   ```bash
   meson test -C builddir engine-smoke
   ```
   Ensure signal still produces adequate RMS

3. **Live test with MIDI:**
   ```bash
   ./builddir/flues-synth hw:2,0
   ```
   - Test all 7 Disyn algorithms (CC 16)
   - Sweep param1 and param2 (CC 17, CC 18)
   - Monitor for noise/clipping

4. **Feedback stability test:**
   - Set feedback levels to 0.5 (CC 28, 29, 30)
   - Play sustained note
   - Verify no runaway oscillation

## Frequency Independence

Tested across 55Hz-3520Hz: Levels consistent (±0.01 variation).
**Conclusion:** Problem is not frequency-dependent.

## Implementation Priority

**Immediate (choose one):**
- [ ] **Option 4 (Hybrid)** - Best balance, two config changes
- [ ] **Option 1** - Fastest single change

**Short-term:**
- [ ] Add parameter safety clamping to CC handlers
- [ ] Document safe parameter ranges in handover

**Long-term:**
- [ ] Option 3 - Normalize in C++ source
- [ ] Consider adaptive makeup gain based on formant Q values

## References

- Test fixture: `flues-synth/tests/disyn_levels.c`
- Signal flow: `flues-synth/src/synth_engine.c:106-199`
- Formant makeup: `flues-synth/src/audio/modules/formant_bank_module.c:20`
- Disyn source: `lv2/disyn/src/modules/OscillatorModule.hpp`
- Run test: `./builddir/disyn-levels`
