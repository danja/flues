# Porting Notes: JavaScript to C/GTK

This document describes how the PM Synth was ported from JavaScript/Web Audio to C/GTK4.

## Overview

The GTK version is a direct translation of the JavaScript implementation in `experiments/pm-synth`, preserving the same:
- Modular DSP architecture
- Signal flow and algorithms
- Default parameter values
- Interface strategy patterns

## Translation Patterns

### JavaScript Class → C Struct + Functions

JavaScript:
```javascript
class SourcesModule {
    constructor(sampleRate) {
        this.sampleRate = sampleRate;
        this.dcLevel = 0;
    }

    process() {
        return this.dcLevel + Math.random() * 2 - 1;
    }
}
```

C:
```c
typedef struct {
    float sample_rate;
    float dc_level;
} SourcesModule;

SourcesModule* sources_create(float sample_rate) {
    SourcesModule* s = calloc(1, sizeof(SourcesModule));
    s->sample_rate = sample_rate;
    s->dc_level = 0.0f;
    return s;
}

float sources_process(SourcesModule* sources) {
    return sources->dc_level + white_noise();
}
```

### Strategy Pattern Translation

JavaScript:
```javascript
export class InterfaceStrategy {
    process(input) { throw new Error("Must implement"); }
}

export class ReedStrategy extends InterfaceStrategy {
    process(input) {
        return Math.tanh(input * this.intensity);
    }
}
```

C (using vtables):
```c
typedef struct InterfaceStrategy {
    const InterfaceStrategyVTable* vtable;
    float intensity;
    void* impl_data;
} InterfaceStrategy;

typedef struct {
    float (*process)(InterfaceStrategy* self, float input);
    void (*destroy)(InterfaceStrategy* self);
} InterfaceStrategyVTable;

// Reed implementation
static float reed_process(InterfaceStrategy* self, float input) {
    return fast_tanh(input * self->intensity);
}

static const InterfaceStrategyVTable reed_vtable = {
    .process = reed_process,
    .destroy = reed_destroy
};
```

### Web Audio API → PulseAudio

JavaScript:
```javascript
const audioContext = new AudioContext();
const workletNode = new AudioWorkletNode(audioContext, 'pm-synth-processor');
```

C:
```c
AudioBackend* audio = audio_backend_create(
    AUDIO_BACKEND_PULSEAUDIO,
    sample_rate, buffer_size,
    audio_callback, user_data
);
audio_backend_start(audio);
```

### UI: DOM → GTK4

JavaScript:
```javascript
const slider = document.getElementById('dc-level');
slider.addEventListener('input', (e) => {
    synth.setDCLevel(e.target.value);
});
```

C:
```c
GtkWidget* slider = gtk_scale_new_with_range(
    GTK_ORIENTATION_HORIZONTAL, 0, 100, 1
);
g_signal_connect(slider, "value-changed",
    G_CALLBACK(on_dc_changed), synth);
```

## Key Differences

### Memory Management

- **JavaScript**: Automatic garbage collection
- **C**: Manual allocation/deallocation with `malloc`/`free`
  - Every `create()` function has corresponding `destroy()`
  - Buffers explicitly allocated and freed

### Floating Point

- **JavaScript**: Always 64-bit doubles
- **C**: Using 32-bit floats for audio (lower memory, matches hardware)

### Arrays and Buffers

- **JavaScript**: Dynamic arrays (`Float32Array`)
- **C**: Fixed-size buffers with explicit size tracking
  ```c
  float* buffer = calloc(buffer_size, sizeof(float));
  ```

### Callbacks

- **JavaScript**: Arrow functions, closures
- **C**: Function pointers, explicit user_data pointers
  ```c
  typedef void (*AudioProcessCallback)(float* output, int num_samples, void* user_data);
  ```

## File Mapping

### JavaScript → C Headers

| JavaScript File | C Header | Notes |
|----------------|----------|-------|
| `PMSynthEngine.js` | `pm_synth.h` | Main API |
| `SourcesModule.js` | `dsp_modules.h` | All module interfaces |
| `InterfaceStrategy.js` | `interface_strategy.h` | Strategy pattern |
| Various utility files | `dsp_utils.h` | Inline functions |

### JavaScript → C Implementation

| JavaScript Module | C Source File | Lines |
|-------------------|---------------|-------|
| `SourcesModule.js` | `sources_module.c` | ~60 |
| `EnvelopeModule.js` | `envelope_module.c` | ~60 |
| `InterfaceModule.js` | `interface_module.c` | ~60 |
| `DelayLinesModule.js` | `delay_lines_module.c` | ~100 |
| `FeedbackModule.js` | `feedback_module.c` | ~40 |
| `FilterModule.js` | `filter_module.c` | ~70 |
| `ModulationModule.js` | `modulation_module.c` | ~70 |
| `ReverbModule.js` | `reverb_module.c` | ~100 |
| `PMSynthEngine.js` | `pm_synth_engine.c` | ~250 |

### Total Lines of Code

- **JavaScript version**: ~2000 LOC (excluding tests)
- **C version**: ~2500 LOC (similar, slightly more due to manual memory management)

## Algorithm Fidelity

All DSP algorithms are direct translations:

### State Variable Filter
JavaScript and C both implement the same difference equations:
```
low += f * band
high = input - low - q * band
band += f * high
```

### Schroeder Reverb
Identical parallel comb filters + series allpass structure

### Interface Strategies
Mathematical functions preserved exactly:
- `fast_tanh()` uses same rational approximation
- Reed model uses identical pressure/flow equations
- Delay line interpolation uses same Hermite/cubic methods

## Build System

- **JavaScript**: Vite (bundler)
- **C**: Meson (build system)

Meson provides:
- Dependency management (GTK4, PulseAudio)
- Cross-platform compilation
- Installation targets

## Testing Strategy

1. **Reference Implementation**: Keep JavaScript version as reference
2. **A/B Comparison**: Record output from both versions with same parameters
3. **Visual Inspection**: Compare waveforms in Audacity
4. **Listening Tests**: Verify sound quality matches

## Performance Notes

C version advantages:
- Lower latency (no JavaScript VM overhead)
- Direct hardware access via PulseAudio
- Smaller memory footprint
- No garbage collection pauses

Expected performance:
- Audio thread: ~1-2ms latency
- CPU usage: ~5-10% single core at 44.1kHz

## Next Steps for Complete Port

1. **Complete All Interface Strategies**
   - Currently only Reed is fully implemented
   - Remaining 11 use simple stubs
   - Translate from `reference/modules/interface/strategies/`

2. **Add Remaining UI Controls**
   - Filter parameters
   - Modulation controls
   - Reverb controls
   - Tuning/Ratio controls

3. **Add Visualizer**
   - Port waveform display using GTK DrawingArea
   - Cairo for rendering

4. **MIDI Support**
   - Add ALSA MIDI backend
   - Map MIDI CC to parameters

5. **Preset System**
   - JSON or INI file format
   - Load/save dialog

## References

- Original JavaScript: `../reference/modules/`
- Research docs: `../reference/docs/`
- Algorithm references in code comments
