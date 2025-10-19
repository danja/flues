# Disyn LV2 Plugin - Implementation Plan

## Executive Summary

This document outlines the plan to port the `experiments/disyn` distortion synthesizer to an LV2 plugin. The port follows the established pattern from `lv2/pm-synth`, translating JavaScript AudioWorklet code to C++ header-only modules.

## 1. Source Analysis

### 1.1 Disyn Architecture (JavaScript)

**Core Components:**
- `DisynEngine.js` - Main engine coordinator (on main thread)
- `disyn-worklet.js` - AudioWorklet processor with monophonic voice management
- `AlgorithmRegistry.js` - Algorithm definitions and parameter mappings
- `OscillatorModule.js` - Seven distortion algorithms
- `EnvelopeModule.js` - Attack/Release envelope
- `ReverbModule.js` - Schroeder reverb

**Seven Algorithms:**
1. **Dirichlet Pulse** (bandLimitedPulse) - Band-limited pulse via Dirichlet kernel
   - Params: harmonics (1-64), tilt (-3 to +15 dB/oct)
2. **Single-Sided DSF** (dsfSingleSided) - Moorer discrete summation formula
   - Params: decay (0-0.98), ratio (0.5-4)
3. **Double-Sided DSF** (dsfDoubleSided) - Symmetric sidebands
   - Params: decay (0-0.96), ratio (0.5-4.5)
4. **Tanh Square** (tanhSquare) - Hyperbolic tangent waveshaping
   - Params: drive (0.05-5), trim (0.2-1.2)
5. **Tanh Saw** (tanhSaw) - Square-to-saw transformation
   - Params: drive (0.05-4.5), blend (0-1)
6. **Phase-Aligned Formant** (paf) - PAF oscillator
   - Params: formant (0.5-6 ×f0), bandwidth (50-3000 Hz)
7. **Modified FM** (modFm) - Modified FM synthesis
   - Params: index (0.01-8), ratio (0.25-6)

**Signal Flow:**
```
noteOn/Off (MIDI) → Voice Management
  ↓
OscillatorModule.process(algorithmId, params, frequency)
  ↓
Envelope.process() → amplitude envelope
  ↓
sample * env * velocity * masterGain
  ↓
ReverbModule.process(sample)
  ↓
Audio Output (stereo)
```

### 1.2 Existing PM-Synth LV2 Pattern

**Structure:**
- Header-only C++ modules in `src/modules/`
- Main engine: `PMSynthEngine.hpp`
- Plugin glue: `pm_synth_plugin.cpp`
- LV2 metadata: `manifest.ttl`, `pm-synth.ttl`
- Build system: CMake
- Optional GTK3 UI

**Key Patterns:**
- Modules are self-contained classes with `process()` methods
- Parameter setters use normalized 0-1 values
- Sample-accurate MIDI processing in `run()` function
- Reset on note-on for clean articulation
- Tail detection for voice shutdown

## 2. Port Strategy

### 2.1 Module Translation Plan

**DisynEngine → C++ Modules:**

1. **OscillatorModule.hpp** (direct port of OscillatorModule.js)
   - Seven algorithm implementations
   - Phase accumulators (phase, modPhase, secondaryPhase, secondaryPhaseNeg)
   - Shared helper: `stepPhase()`, `computeDSFComponent()`

2. **EnvelopeModule.hpp** (simplified from pm-synth, reuse if possible)
   - Attack/Release only (no Decay/Sustain)
   - Linear ramps
   - Gate control

3. **ReverbModule.hpp** (port from pm-synth or adapt Schroeder)
   - Schroeder reverb with 4 comb + 2 allpass
   - Size and Level parameters

4. **DisynEngine.hpp** (main coordinator, similar to PMSynthEngine.hpp)
   - Monophonic voice management
   - Algorithm selection (enum)
   - Parameter routing
   - Master gain

### 2.2 Algorithm Implementation Details

**C++ Translations (examples):**

```cpp
// processDirichletPulse (JavaScript → C++)
float processDirichletPulse(const DirichletParams& p, float frequency) {
    const int harmonics = static_cast<int>(std::clamp(p.harmonics, 1.0f, 64.0f));
    const float tilt = p.tilt; // -3 to +15 dB/oct

    phase = stepPhase(phase, frequency);
    const float theta = phase * TWO_PI;

    const float numerator = std::sin((2 * harmonics + 1) * theta * 0.5f);
    const float denominator = std::sin(theta * 0.5f);

    float value;
    if (std::abs(denominator) < EPSILON) {
        value = 1.0f;
    } else {
        value = (numerator / denominator) - 1.0f;
    }

    const float tiltFactor = std::pow(10.0f, tilt / 20.0f);
    return (value / harmonics) * tiltFactor;
}
```

**Strategy Pattern for Algorithms:**
- Consider a strategy pattern similar to pm-synth's InterfaceModule
- Alternative: Switch statement in OscillatorModule (simpler, fewer files)

### 2.3 Parameter Mapping

**LV2 Control Ports:**
```
PORT_AUDIO_OUT       0  (output, audio)
PORT_MIDI_IN         1  (input, atom:Sequence)
PORT_ALGORITHM_TYPE  2  (input, control, 0-6, integer enum)
PORT_PARAM_1         3  (input, control, 0-1 normalized)
PORT_PARAM_2         4  (input, control, 0-1 normalized)
PORT_ENVELOPE_ATTACK 5  (input, control, 0-1 normalized)
PORT_ENVELOPE_RELEASE 6 (input, control, 0-1 normalized)
PORT_REVERB_SIZE     7  (input, control, 0-1)
PORT_REVERB_LEVEL    8  (input, control, 0-1)
PORT_MASTER_GAIN     9  (input, control, 0-1)
PORT_TOTAL_COUNT     10
```

**Parameter Normalization:**
Each algorithm has 2 parameters that are normalized 0-1 in LV2, then mapped to algorithm-specific ranges inside the module (matching JavaScript `mapValue` functions).

**Algorithm Enum:**
```cpp
enum class AlgorithmType : int {
    DIRICHLET_PULSE = 0,
    DSF_SINGLE = 1,
    DSF_DOUBLE = 2,
    TANH_SQUARE = 3,
    TANH_SAW = 4,
    PAF = 5,
    MOD_FM = 6
};
```

## 3. File Structure

```
lv2/disyn/
├── CMakeLists.txt                     # Build configuration
├── README.md                          # Build/install instructions
├── disyn.lv2/
│   ├── manifest.ttl                   # LV2 manifest
│   └── disyn.ttl                      # Port definitions
└── src/
    ├── disyn_plugin.cpp               # LV2 plugin entry point
    ├── DisynEngine.hpp                # Main engine
    ├── AlgorithmTypes.hpp             # Enum and param structs
    └── modules/
        ├── OscillatorModule.hpp       # Seven algorithms
        ├── EnvelopeModule.hpp         # AR envelope
        └── ReverbModule.hpp           # Schroeder reverb
```

## 4. Implementation Steps

### Phase 1: Core Module Translation
1. Create directory structure `lv2/disyn/`
2. Port `OscillatorModule.js` → `OscillatorModule.hpp`
   - Seven algorithm methods
   - Phase accumulator management
   - Parameter structs for each algorithm
3. Port `EnvelopeModule.js` → `EnvelopeModule.hpp` (or reuse from pm-synth)
4. Port `ReverbModule.js` → `ReverbModule.hpp` (or reuse from pm-synth)

### Phase 2: Engine Integration
5. Create `AlgorithmTypes.hpp` with enums and param structures
6. Create `DisynEngine.hpp`
   - Algorithm selection logic
   - Parameter routing (normalized → algorithm-specific)
   - Monophonic voice management
   - Signal chain: oscillator → envelope → reverb

### Phase 3: LV2 Plugin Glue
7. Create `disyn_plugin.cpp` (based on `pm_synth_plugin.cpp`)
   - Port enumeration and connection
   - MIDI handling (note on/off, monophonic)
   - Parameter application
   - Sample-accurate event processing
8. Create `disyn.ttl` with port definitions
9. Create `manifest.ttl` with plugin metadata
10. Create `CMakeLists.txt` for build system

### Phase 4: Testing & Validation
11. Build plugin
12. Test in LV2 host (Jalv, Ardour, Carla)
13. Verify each algorithm produces expected output
14. Test MIDI note handling
15. Test parameter changes during playback
16. Check for audio artifacts, clicks, pops

### Phase 5: Documentation
17. Write `README.md` with build/install instructions
18. Document parameter ranges and algorithm behaviors
19. Optional: Add this to root `CLAUDE.md`

## 5. Key Differences from PM-Synth

| Aspect | PM-Synth | Disyn |
|--------|----------|-------|
| Voice count | Monophonic | Monophonic |
| Module count | 8 (Sources, Envelope, Interface, DelayLines, Feedback, Filter, Modulation, Reverb) | 3 (Oscillator, Envelope, Reverb) |
| Complexity | High (physical modeling, delay feedback loops) | Medium (distortion algorithms) |
| Algorithm selection | 12 interface strategies | 7 oscillator algorithms |
| Parameter count | ~21 control ports | ~10 control ports |
| DSP pattern | Feedback delay network | Direct synthesis + effects |

**Simplifications:**
- Disyn is simpler: no delay lines, no feedback routing, no filter, no LFO
- Cleaner signal flow: `oscillator → envelope → reverb → output`
- Fewer parameters per algorithm (2 vs. pm-synth's variable counts)

## 6. Testing Strategy

### Unit Testing (Optional)
- Test individual algorithms with known inputs
- Verify parameter mapping (normalized → real values)
- Check phase accumulator wrapping

### Integration Testing
```bash
# Build plugin
cd lv2/disyn
cmake -S . -B build
cmake --build build

# Install locally
cmake --install build --prefix ~/.lv2

# Test with jalv
jalv.gtk https://danja.github.io/flues/plugins/disyn

# Test with lv2ls
lv2ls | grep disyn
```

### Audio Testing
1. Load in Ardour/Reaper/Carla
2. Send MIDI notes
3. Sweep through all 7 algorithms
4. Adjust parameters during playback
5. Test envelope attack/release
6. Test reverb
7. Record output and compare spectrograms with JavaScript version (optional)

## 7. Expected Challenges

### 7.1 Algorithm Fidelity
**Challenge:** Ensuring C++ implementations match JavaScript math precisely.

**Mitigation:**
- Line-by-line translation
- Use same constant definitions (TWO_PI, EPSILON)
- Test with identical input frequencies and parameters
- Compare waveform outputs if needed

### 7.2 Parameter Mapping
**Challenge:** JavaScript uses complex mapping functions (`expoMap`, custom functions).

**Mitigation:**
- Port mapping functions exactly
- Create helper functions (e.g., `expoMap(value, min, max)`)
- Document parameter ranges in TTL and code comments

### 7.3 Phase Management
**Challenge:** OscillatorModule maintains multiple phase accumulators (phase, modPhase, secondaryPhase, secondaryPhaseNeg).

**Mitigation:**
- Reset all phases on note-on
- Careful wrapping logic in `stepPhase()`
- Consider using `std::fmod` or manual wrapping

### 7.4 Real-time Safety
**Challenge:** No allocations in `process()` callback.

**Mitigation:**
- Pre-allocate all buffers in constructor (reverb comb/allpass buffers)
- Use stack-allocated parameter structs
- Avoid std::vector, use fixed-size arrays

## 8. Future Enhancements

### 8.1 Polyphony
- Add voice stealing (note priority)
- Implement 4-8 voice polyphony
- Requires per-voice state duplication

### 8.2 GTK UI (Optional)
- Similar to `pm_synth_ui.c`
- Algorithm selector dropdown
- Two knobs per algorithm (dynamic based on selection)
- Envelope, Reverb, Master knobs

### 8.3 Preset System
- LV2 state extension
- Save/load algorithm + parameters

### 8.4 Additional Features
- Velocity sensitivity per parameter
- Pitch bend support
- Modulation wheel → parameter mapping

## 9. Success Criteria

**Minimum Viable Product:**
- ✅ Plugin builds and installs
- ✅ Recognized by LV2 hosts
- ✅ All 7 algorithms functional
- ✅ MIDI note on/off works
- ✅ Parameters respond to host automation
- ✅ No audio artifacts (clicks, pops, NaNs)
- ✅ Clean note articulation

**Stretch Goals:**
- ✅ GTK UI with real-time parameter feedback
- ✅ Algorithmic equivalence to JavaScript (spectral analysis)
- ✅ Polyphony (4+ voices)
- ✅ Preset save/load

## 10. Timeline Estimate

Assuming focused development:

| Phase | Tasks | Estimated Time |
|-------|-------|----------------|
| 1. Core Modules | Port 3 modules | 2-3 hours |
| 2. Engine | DisynEngine integration | 1-2 hours |
| 3. LV2 Glue | Plugin wrapper, TTL | 1-2 hours |
| 4. Testing | Build, test, debug | 2-3 hours |
| 5. Documentation | README, comments | 0.5-1 hour |
| **Total** | | **6.5-11 hours** |

Faster if modules can be reused from pm-synth (Envelope, Reverb).

## 11. References

### Source Files
- `experiments/disyn/src/audio/DisynEngine.js`
- `experiments/disyn/src/audio/disyn-worklet.js`
- `experiments/disyn/src/audio/AlgorithmRegistry.js`
- `experiments/disyn/src/audio/modules/OscillatorModule.js`
- `experiments/disyn/src/audio/modules/EnvelopeModule.js`
- `experiments/disyn/src/audio/modules/ReverbModule.js`

### Reference Implementation
- `lv2/pm-synth/` - Complete working LV2 plugin
- `lv2/pm-synth/src/pm_synth_plugin.cpp` - Plugin entry point pattern
- `lv2/pm-synth/src/PMSynthEngine.hpp` - Engine pattern
- `lv2/pm-synth/CMakeLists.txt` - Build configuration

### LV2 Documentation
- [LV2 Specification](https://lv2plug.in/ns/)
- [LV2 Book](https://lv2plug.in/book/)
- [LV2 Atom Extension](https://lv2plug.in/ns/ext/atom/)
- [LV2 MIDI Extension](https://lv2plug.in/ns/ext/midi/)

## 12. Open Questions

1. **Module Reuse:** Can we directly reuse `EnvelopeModule.hpp` and `ReverbModule.hpp` from pm-synth?
   - **Decision:** Yes, if they match the disyn behavior (AR envelope, Schroeder reverb). May need minor tweaks.

2. **Algorithm Architecture:** Strategy pattern vs. switch statement?
   - **Recommendation:** Switch statement in `OscillatorModule::process()` for simplicity (only 7 algorithms, no runtime polymorphism needed).

3. **Parameter Struct Design:** Per-algorithm structs vs. generic array?
   - **Recommendation:** Per-algorithm structs (type-safe, self-documenting).

4. **Plugin URI:** Follow pm-synth pattern?
   - **Recommendation:** `https://danja.github.io/flues/plugins/disyn`

5. **Plugin Name:** "Disyn" or "Flues Disyn" or "Distortion Synth"?
   - **Recommendation:** "Flues Disyn" (consistent with "Stove Synth" from pm-synth).

## 13. Next Steps

Once this plan is approved:

1. Create `lv2/disyn/` directory structure
2. Start with `OscillatorModule.hpp` (most critical)
3. Iteratively build, test, and validate each component
4. Follow the Phase 1-5 implementation plan above

---

**Document Status:** Ready for review and approval
**Author:** Claude Code (AI Assistant)
**Date:** 2025-10-19
