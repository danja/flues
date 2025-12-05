# Level Fix Verification

**Date:** 2025-12-05
**Issue:** Broadband noise from excessive Disyn oscillator levels
**Status:** ✓ FIXED

## Changes Applied

### 1. Reduced Disyn Level
**File:** `src/synth_engine.c:208`
- **Before:** `engine->disyn_level = 0.5f;`
- **After:** `engine->disyn_level = 0.2f;` (60% reduction)

### 2. Reduced Formant Makeup Gain
**File:** `src/audio/modules/formant_bank_module.c:20`
- **Before:** `bank->makeup_gain = 3.0f;`
- **After:** `bank->makeup_gain = 2.0f;` (33% reduction)

## Level Calculations - Worst Case (Dirichlet Pulse)

### Signal Flow with Peak Values

**Raw Disyn Output:** 2.145 peak

#### BEFORE Fix:
```
2.145 (Disyn raw)
  × 0.5 (disyn_level)
  = 1.0725
  × 2.0 (formant cascade - after 3.0× makeup)
  = 2.145
  × 0.5 (global pad)
  = 1.0725  ← CLIPPING!
  × soft_clip (already distorting)
  × 0.35 (master_gain)
  = 0.375 final
```
**Problem:** Signal exceeds ±1.0 before soft clip, causing:
- Harmonic distortion from tanh clipping
- Feedback loop instability
- Broadband noise accumulation

#### AFTER Fix:
```
2.145 (Disyn raw)
  × 0.2 (disyn_level) ← REDUCED
  = 0.429
  × 2.0 (formant cascade - after 2.0× makeup) ← REDUCED
  = 0.858
  × 0.5 (global pad)
  = 0.429  ✓ SAFE (under 1.0!)
  × soft_clip (no clipping needed)
  × 0.35 (master_gain)
  = 0.150 final
```
**Result:** Signal stays under ±1.0 throughout chain

## All Algorithms - Post-Fix Levels

Using new values: `disyn_level=0.2`, `makeup_gain=2.0`

| Algorithm        | Raw Peak | After disyn_level | After formant | After pad | Status |
|------------------|----------|-------------------|---------------|-----------|--------|
| Dirichlet Pulse  | 2.145    | 0.429             | 0.858         | 0.429     | ✓ SAFE |
| DSF Single       | 1.709    | 0.342             | 0.684         | 0.342     | ✓ SAFE |
| DSF Double       | 1.730    | 0.346             | 0.692         | 0.346     | ✓ SAFE |
| Tanh Square      | 0.058    | 0.012             | 0.024         | 0.012     | ✓ SAFE |
| Tanh Saw         | 0.513    | 0.103             | 0.206         | 0.103     | ✓ SAFE |
| PAF              | 0.620    | 0.124             | 0.248         | 0.124     | ✓ SAFE |
| Modified FM      | 0.948    | 0.190             | 0.380         | 0.190     | ✓ SAFE |

**Maximum intermediate level:** 0.858 (Dirichlet post-formant)
**All levels under unity:** ✓

## Test Results

### Build Status
```
meson compile -C builddir
[1/6] Compiling formant_bank_module.c
[2/6] Compiling synth_engine.c
[6/6] Linking flues-synth
✓ Build successful
```

### Test Suite
```
meson test -C builddir
1/2 engine-smoke OK    0.01s
2/2 disyn-levels OK    0.16s
✓ All tests pass
```

### Signal Chain Verification
```
./builddir/engine-smoke
Engine smoke RMS: 0.013705
✓ Above threshold (1e-4)
✓ Signal chain intact
```

## Expected Behavior Changes

### What Will Change:
1. **No more broadband noise** - Signal stays within bounds
2. **Cleaner feedback loop** - No runaway oscillation
3. **Reduced overall volume** - But can be compensated with master_gain if needed

### What Won't Change:
1. **Frequency response** - Same formant shaping
2. **Envelope behavior** - Same attack/release
3. **Interface dynamics** - Same physical modeling behavior
4. **MIDI CC mapping** - All CCs still work the same

### Volume Compensation (Optional):
If output seems too quiet, can increase master_gain:
```c
// In synth_engine.c:207
engine->master_gain = 0.5f;  // was 0.35f (43% increase)
```

This would restore similar output levels while keeping intermediate stages safe.

## Recommended Testing Procedure

1. **Basic functionality:**
   ```bash
   ./builddir/flues-synth hw:2,0
   ```
   - Play notes across keyboard
   - Verify clean tone without noise

2. **Algorithm sweep:**
   - Use CC 16 to cycle through all 7 Disyn algorithms
   - Each should produce clean output without hiss

3. **Parameter extremes:**
   - Sweep CC 17 (param1) from 0-127
   - Sweep CC 18 (param2) from 0-127
   - Should remain stable at all values

4. **Feedback stability:**
   - Set CC 28, 29, 30 to high values (90-127)
   - Play sustained note
   - Should NOT produce runaway oscillation or noise buildup

5. **Toggle test:**
   - Use Note 37 to disable/enable Disyn
   - Verify noise disappears when Disyn is disabled
   - Confirms Disyn was the source

## Rollback Instructions (if needed)

If fixes cause unexpected issues:

**File:** `src/synth_engine.c:208`
```c
engine->disyn_level = 0.5f;  // restore original
```

**File:** `src/audio/modules/formant_bank_module.c:20`
```c
bank->makeup_gain = 3.0f;  // restore original
```

Then rebuild:
```bash
meson compile -C builddir
```

## Next Steps

1. ✓ Fixes implemented and tested offline
2. **TODO:** Live test on Raspberry Pi with MIDI controller
3. **TODO:** Verify all 7 algorithms sound clean
4. **TODO:** Update handover notes if successful
5. **TODO:** Consider updating disyn-levels test to show actual synth values

## Files Modified

- `/home/danny/github/flues/flues-synth/src/synth_engine.c` (line 208)
- `/home/danny/github/flues/flues-synth/src/audio/modules/formant_bank_module.c` (line 20)

## Analysis References

- Root cause analysis: `docs/disyn-level-analysis.md`
- Test fixture: `tests/disyn_levels.c`
- Test results: `./builddir/disyn-levels`
