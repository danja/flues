# Envelope Attack Bug Fix

**Date:** 2025-12-05
**Issue:** Notes not triggering - zero audio output despite signal present internally
**Status:** ✓ FIXED

## Problem Description

After implementing DC blocking protection, the synth stopped producing audio output entirely. Notes would trigger internally (envelope ramped up, oscillators generated signal) but the final output was always zero.

## Root Cause

The **third DC blocker on the final output** (added for safety) was too aggressive with R=0.999. It treated the slow envelope attack ramp as DC offset and removed it, zeroing the signal.

### Why It Happened

DC blockers work by removing very low-frequency content. The formula is:
```
y[n] = x[n] - x[n-1] + R × y[n-1]
```

With R=0.999:
- DC (0 Hz) is attenuated by −60 dB
- But sub-1Hz content (like envelope ramps) is also heavily attenuated
- During the attack phase, the envelope creates a slow DC-like ramp
- The DC blocker interpreted this as DC offset and removed it

## Solution

**Removed the third DC blocker entirely** from the output stage.

### Rationale

We already have two DC blockers:
1. **After Disyn oscillator** - Catches DC at the source
2. **On feedback path** - Prevents DC accumulation in the loop

A third blocker on the output was redundant and caused more harm than good by killing the envelope attack.

## Changes Made

### 1. Removed Output DC Blocker

**File:** `src/synth_engine.c`

**Before:**
```c
// 15. Final DC Blocker (safety net to prevent any DC from reaching output)
output = dc_blocker_process(&voice->dc_blocker_output, output);

// 16. Master gain
output *= engine->master_gain;
```

**After:**
```c
// 15. Master gain (final DC blocker removed - was too aggressive and killed envelope attack)
output *= engine->master_gain;
```

### 2. Removed dc_blocker_output from Voice Struct

**File:** `src/synth_engine.c:41-43`

**Before:**
```c
// Multi-stage DC blockers to prevent feedback latching
DCBlocker dc_blocker_disyn;    // After Disyn oscillator (catches DC at source)
DCBlocker dc_blocker_feedback; // On feedback path (prevents loop accumulation)
DCBlocker dc_blocker_output;   // On final output (safety net)
```

**After:**
```c
// Dual DC blockers to prevent feedback latching
DCBlocker dc_blocker_disyn;    // After Disyn oscillator (catches DC at source)
DCBlocker dc_blocker_feedback; // On feedback path (prevents loop accumulation)
```

### 3. Updated All Initialization Code

Removed `dc_blocker_init(&voice->dc_blocker_output, DC_BLOCKER_R)` from:
- `voice_init()` - Line 88-89
- `synth_engine_note_on()` - Line 311-312
- `synth_engine_enable_feedback()` - Line 492-493
- `synth_engine_hard_mute()` - Line 518-519

### 4. Increased Default Noise Level

**File:** `src/synth_engine.c:262`

**Before:**
```c
sources_set_noise_level(engine->voice.sources, 0.02f);
```

**After:**
```c
sources_set_noise_level(engine->voice.sources, 0.15f);  // Increased to provide formant excitation
```

**Rationale:** Formant filters need broadband excitation (noise with harmonics across many frequencies). Pure tones from Disyn get heavily attenuated by formants tuned to different frequencies. Higher noise provides better formant response.

## Test Results

### Before Fix
```
1/3 engine-smoke  OK    (RMS: 0.013)
2/3 envelope-test FAIL  (all samples zero)
3/3 disyn-levels  OK
```

### After Fix
```
1/3 engine-smoke  OK    (RMS: 0.019)
2/3 envelope-test OK    (all tests pass)
3/3 disyn-levels  OK
```

**All 3 tests now passing!**

## DC Protection Strategy (Final)

### Two-Stage DC Blocking

1. **Stage 1: After Disyn (R=0.999)**
   - Catches DC from oscillator algorithms at source
   - Prevents DC from entering formant cascade

2. **Stage 2: On Feedback Path (R=0.999)**
   - Prevents DC from circulating in delay/filter loop
   - Critical for preventing latching

### Why Two Stages Are Enough

- **Disyn DC blocker** handles the main DC source (oscillators)
- **Feedback DC blocker** prevents loop accumulation
- **Soft clipping** (tanh) at output provides additional protection
- Output blocker was redundant and harmful

## Signal Flow (Final)

```
Disyn Oscillator (261 Hz + harmonics)
    ↓
[DC BLOCKER 1] ← R=0.999, catches source DC
    ↓ × disyn_level (0.2)
Sources (Noise 0.15 + DC 0)
    ↓
Mix + Envelope (AR)
    ↓
Formant Cascade (F1-F4, makeup 2.0×)
    ↓
Feedback Mix (delay1 + delay2 + filter)
    ↓
[DC BLOCKER 2] ← R=0.999, prevents loop accumulation
    ↓
Interface Module (Reed/Pluck/etc)
    ↓
Dual Delay Lines
    ↓
State Variable Filter
    ↓
AM Modulation
    ↓
Global Pad (×0.5)
    ↓
Soft Clip (tanh guard)
    ↓
Master Gain (×0.35)
    ↓
OUTPUT ← No DC blocker here!
```

## Related Issues Discovered

### Formant Attenuation Issue

Formant filters with high Q (~6) severely attenuate pure tones that don't match their center frequencies:

- Input: 261 Hz sine at 0.007 amplitude
- After F1 (500 Hz), F2 (1500 Hz), F3 (2500 Hz), F4 (3500 Hz): 0.000002 amplitude
- **100× attenuation!**

This is expected behavior - formants are designed for speech synthesis where the excitation is broadband (noise + harmonics). The fix was to increase default noise level from 0.02 to 0.15, providing broadband content for formants to resonate.

**User Control:** Noise level is adjustable via MIDI CC 20, allowing users to balance:
- **Low noise (0.02-0.05):** Pure Disyn tones, formants less effective
- **Medium noise (0.10-0.20):** Balanced, formants respond well
- **High noise (0.25+):** Breathy/whispered tones, strong formant character

## Files Modified

- `src/synth_engine.c` - Removed output DC blocker, increased default noise
- `tests/envelope_test.c` - Fixed variable name conflict, increased test buffer size
- `docs/dc-blocking-protection.md` - Updated documentation (now outdated, needs revision)

## Verification Steps

1. ✓ All 3 test suites pass
2. ✓ engine-smoke confirms signal generation (RMS 0.019)
3. ✓ envelope-test confirms proper attack/sustain/release
4. ✓ disyn-levels confirms all algorithms generate expected levels

## Recommendation for Pi Testing

After deploying to Pi, test:

1. **Basic playback:**
   ```bash
   ./builddir/flues-synth hw:2,0
   ```
   Play notes - should hear clean tones with vocal formant character

2. **Noise level adjustment:**
   - CC 20: 0-30 → Disyn-heavy (pure tones, less formant)
   - CC 20: 40-70 → Balanced (recommended default)
   - CC 20: 80-127 → Noise-heavy (breathy, strong formants)

3. **DC stability:**
   - Play sustained notes (10+ seconds)
   - Should remain stable without latching
   - Use Note 38 (feedback toggle) or Note 41 (hard mute) if any issues

4. **All 7 Disyn algorithms:**
   - CC 16: 0-127 sweeps through algorithms
   - All should produce clean, stable output

## Conclusion

The third DC blocker was well-intentioned but caused more problems than it solved. The dual DC blocker strategy (at source + in feedback loop) provides adequate DC protection without interfering with envelope dynamics.

The system is now stable, tests pass, and ready for Pi deployment.
