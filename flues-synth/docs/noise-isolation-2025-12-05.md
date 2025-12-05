# Noise Isolation Report

**Date:** 2025-12-05
**Issue:** Broadband noise from synthesizer
**Tool:** `tests/noise_isolation.c` - systematic parameter sweep

## Critical Fix Applied

### Noise Generator DC Offset Bug (FIXED)

**File:** `include/dsp_utils.h:89`

**Before:**
```c
return (float)(rng->state >> 16) / 32768.0f;  // WRONG: divides by 32768
```

**After:**
```c
return (float)(rng->state >> 16) / 65536.0f;  // Fixed: divides by 65536
```

**Root Cause:**
Upper 16 bits of LCG state give values 0-65535. Dividing by 32768 gives range [0, 2) instead of [0, 1). After `* 2 - 1` mapping, this becomes [-1, +3) with **+1.0 DC offset**.

**Impact:**
| Noise Level | DC Before | DC After | Reduction |
|-------------|-----------|----------|-----------|
| 0.05 | 0.017012 (34%) | -0.000066 (<0.1%) | **99.6%** |
| 0.15 | 0.052988 (35%) | 0.000189 (<0.1%) | **99.6%** |
| 1.0 | 0.234599 (23%) | 0.001717 (0.2%) | **99.3%** |

**Result:** Noise source now properly centered around zero with negligible DC offset.

## Remaining High-Level Sources

The noise isolation test identified several other components producing excessive RMS levels (>0.1):

### 1. Interface Strategies (Highest Priority)

Several interface strategies produce very high output levels:

| Interface | RMS | DC Offset | Peak | Status |
|-----------|-----|-----------|------|---------|
| **Brass** | **0.433** | **0.150** | 0.573 | ⚠ CRITICAL - 4× expected |
| **Drum** | **0.441** | 0.002 | 0.575 | ⚠ CRITICAL - 4× expected |
| **Bow** | **0.178** | -0.000 | 0.564 | ⚠ HIGH - 1.8× expected |
| **Hit** | **0.157** | -0.002 | 0.524 | ⚠ HIGH - 1.6× expected |
| **Bell** | **0.155** | -0.008 | 0.507 | ⚠ HIGH - 1.5× expected |
| **Vapor** | 0.088 | **0.064** | 0.320 | ⚠ DC offset issue |
| **Crystal** | 0.074 | 0.006 | 0.559 | Marginal |
| Reed | 0.031 | ~0 | 0.404 | ✓ OK |
| Pluck | 0.032 | ~0 | 0.157 | ✓ OK |
| Flute | 0.030 | ~0 | 0.111 | ✓ OK |
| Quantum | 0.050 | ~0 | 0.190 | ✓ OK |
| Plasma | 0.052 | ~0 | 0.226 | ✓ OK |

**Analysis:**
- **Brass, Drum, Bow** produce 2-4× excessive levels compared to well-behaved strategies
- **Brass** also has **0.15 DC offset** (15% of signal!)
- **Vapor** has **0.064 DC offset** (6.4% of signal)
- Reed, Pluck, Flute, Quantum, Plasma are within acceptable bounds

**Recommended Action:**
- Reduce output gains in `brass_strategy.c`, `drum_strategy.c`, `bow_strategy.c`
- Investigate DC offset sources in `brass_strategy.c` and `vapor_strategy.c`
- Normalize all interface strategies to produce similar RMS levels (~0.03-0.05)

### 2. Nasal Formant (High Priority)

**Impact:** Enabling nasal formant increases RMS from 0.03 to **0.20** (7× increase) with **0.022 DC offset**.

| Mode | RMS | DC Offset | Status |
|------|-----|-----------|---------|
| None | 0.028 | 0.000 | ✓ OK |
| **Nasal** | **0.203** | **0.022** | ⚠ CRITICAL |
| Sing | 0.031 | 0.000 | ✓ OK |
| Shout | 0.028 | 0.000 | ✓ OK |
| Nasal + Sing | 0.202 | 0.022 | ⚠ CRITICAL |
| Nasal + Shout | 0.204 | 0.022 | ⚠ CRITICAL |

**Analysis:**
- Nasal formant (250 Hz parallel resonance) is adding significant level
- DC offset suggests asymmetric response or feedback issue
- All combinations with Nasal show same excessive level

**Recommended Action:**
- Check `formant_bank_module.c` nasal formant processing
- Reduce nasal formant gain or adjust Q to prevent resonance buildup
- Investigate DC offset source in nasal parallel path

### 3. Disyn Algorithms (Medium Priority)

Some Disyn algorithms produce higher levels than others:

| Algorithm | RMS | DC Offset | Peak | Status |
|-----------|-----|-----------|------|---------|
| **DSF Single (1)** | **0.170** | **0.014** | 0.393 | ⚠ HIGH |
| **DSF Double (2)** | **0.162** | **0.013** | 0.388 | ⚠ HIGH |
| **PAF (5)** | **0.112** | 0.004 | 0.190 | ⚠ Marginal |
| **Modified FM (6)** | **0.109** | 0.007 | 0.213 | ⚠ Marginal |
| Dirichlet (0) | 0.072 | -0.007 | 0.479 | ✓ OK |
| Tanh Square (3) | 0.044 | 0.001 | 0.065 | ✓ OK |
| Tanh Saw (4) | 0.033 | 0.001 | 0.048 | ✓ OK |

**Analysis:**
- DSF algorithms (1, 2) produce 2× higher RMS than expected
- DSF algorithms also have small DC offsets (~0.013)
- PAF and Modified FM are marginally high
- Tanh algorithms are well-behaved

**Recommended Action:**
- Review `disyn_wrapper.cpp` DSF implementations
- Normalize DSF algorithm output levels to match other algorithms
- Investigate DC offset source in DSF summation

### 4. High-Q Filters (Low Priority)

High Q filters (Q=5) boost signal levels:

| Filter | RMS | Peak | Status |
|--------|-----|------|---------|
| LP 2kHz Q=5 | 0.070 | 0.258 | Marginal |
| BP 2kHz Q=5 | 0.081 | 0.304 | Marginal |
| LP 2kHz Q=1 | 0.028 | 0.113 | ✓ OK |

**Analysis:**
- Q=5 filters produce ~2.5× higher RMS than Q=1
- This is expected behavior for resonant filters
- Levels are still within safe bounds (<0.1)

**Recommended Action:**
- Document expected level increase with high Q
- Consider applying post-filter gain compensation if Q > 3
- Monitor for clipping with Q=10 and high input levels

### 5. High Feedback Levels (Low Priority)

Feedback level 0.6 produces RMS 0.058 vs 0.034 at level 0.0:

| Feedback | RMS | Peak | Status |
|----------|-----|------|---------|
| 0.6 | 0.058 | 0.204 | Marginal |
| 0.4 | 0.040 | 0.153 | ✓ OK |
| 0.2 | 0.034 | 0.132 | ✓ OK |
| 0.0 | 0.034 | 0.132 | ✓ OK |

**Analysis:**
- 60% feedback increases RMS by 70%
- Still within safe bounds (<0.1)
- Typical behavior for feedback systems

**Recommended Action:**
- Current implementation is acceptable
- Document expected level increase at high feedback
- Consider soft limiting at feedback sum if clipping occurs

## Summary of Findings

### Critical Issues (Fix Immediately):
1. ✅ **Noise Generator DC Offset** - FIXED (99.6% reduction)
2. ⚠ **Brass/Drum/Bow Interface** - RMS 0.18-0.44 (2-4× excessive)
3. ⚠ **Nasal Formant** - RMS 0.20 (7× excessive) + DC offset 0.022

### High Priority (Fix Soon):
4. ⚠ **DSF Algorithms** - RMS 0.17 (2× excessive) + DC offset 0.013
5. ⚠ **Hit/Bell Interface** - RMS 0.15-0.16 (1.5× excessive)

### Medium Priority (Review):
6. ⚠ **Vapor Interface** - DC offset 0.064
7. ⚠ **PAF/Modified FM** - RMS 0.11 (marginal)

### Low Priority (Document):
8. ℹ High-Q filters boost levels (expected behavior)
9. ℹ High feedback increases levels (expected behavior)

## Test Configuration

All tests performed with:
- Sample rate: 32000 Hz (from config.h)
- Test duration: 1.0 second
- Note: MIDI 60 (C4, 261.6 Hz)
- RMS threshold: 0.1 (10% of full scale)
- DC threshold: 0.01 (1% of full scale)

## Recommendations

### Immediate Actions:
1. ✅ **Apply noise fix** - Already done (divide by 65536 instead of 32768)
2. **Reduce Brass/Drum/Bow output** - Scale by 0.3-0.5× in strategy implementations
3. **Fix Nasal formant** - Reduce parallel formant gain or Q

### Follow-up Actions:
4. **Normalize Disyn DSF algorithms** - Scale output by 0.5× or fix summation
5. **Normalize Hit/Bell output** - Scale by 0.5-0.6×
6. **Fix DC offsets** - Investigate Brass, Vapor, Nasal, DSF sources

### Testing Protocol:
- Run `./builddir/noise-isolation` after each fix
- Verify RMS levels stay below 0.1 for all configurations
- Verify DC offsets stay below 0.01
- Rerun full test suite (`meson test -C builddir`)

## Test Suite Status

After noise generator fix:
```
1/4 engine-smoke    ✓ OK  (RMS verification)
2/4 envelope-test   ✓ OK  (envelope behavior)
3/4 disyn-levels    ✓ OK  (algorithm levels)
4/4 noise-isolation ✗ FAIL (detected remaining issues)
```

The noise-isolation test is designed to fail when excessive levels are detected. This is working as intended - it has successfully identified the remaining noise sources that need attention.

## Files Modified

- `include/dsp_utils.h:89` - Fixed noise generator DC offset (32768 → 65536)
- `tests/noise_isolation.c` - New comprehensive noise isolation test (added to meson.build)
- `docs/noise-isolation-2025-12-05.md` - This document

## Next Steps

1. Review and reduce interface strategy output levels (brass_strategy.c, drum_strategy.c, bow_strategy.c)
2. Investigate nasal formant excessive level and DC offset
3. Normalize Disyn DSF algorithm outputs
4. Rerun noise-isolation test after each fix
5. Update documentation with expected signal levels for each module
