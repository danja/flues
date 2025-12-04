# Plan: Unified Polyphonic GTK Synthesizer (Disyn + Chatterbox + PM-Synth)

## Project Name: **Flues-Synth** (Hybrid Polyphonic Desktop Synthesizer)

## Executive Summary

Create a standalone GTK4 desktop synthesizer for Raspberry Pi 4 combining three synthesis engines in series:
1. **Disyn** (7 distortion algorithms) → generates harmonically rich excitation
2. **Chatterbox** (formant filtering) → vocal tract resonances shape the spectrum
3. **PM-Synth** (physical modeling) → acoustic feedback and spatial processing

**Target:** 4-voice polyphony (build-configurable), ALSA direct audio, hardware MIDI input, ~60% CPU on Pi 4.

## Confirmed Requirements

- **Signal Flow**: Disyn oscillators → Chatterbox formants → PM-Synth physical modeling (serial)
- **Audio Backend**: ALSA direct (no PulseAudio)
- **Polyphony**: 4 voices (configurable at build time: `#define MAX_VOICES 4`)
- **MIDI Input**: ALSA raw MIDI (hardware) + ALSA sequencer (virtual/software)
- **Platform**: Raspberry Pi 4 (ARM Cortex-A72, 1.5-1.8 GHz, 4GB+ RAM)
- **UI**: GTK4 following gtk-synth patterns
- **Language**: C11 (except Disyn algorithm code may remain C++ and wrapped)

---

## 1. SIGNAL FLOW ARCHITECTURE

### Per-Voice Processing Chain

```
MIDI Note On/Off
    ↓
[Voice Allocation & MIDI→Frequency]
    ↓
┌───────────────────────────────────────────────────────────────┐
│ VOICE PROCESSING (per voice)                                  │
├───────────────────────────────────────────────────────────────┤
│                                                               │
│  1. DISYN OSCILLATOR MODULE (Excitation Source)              │
│     ├─ 7 Algorithms: DirichletPulse, DSF, Tanh, PAF, ModFM  │
│     ├─ Algorithm selector (0-6)                              │
│     ├─ Param1, Param2 (algorithm-specific)                   │
│     ├─ Tone Level (0-1)                                       │
│     └─ Output: harmonically rich waveform                     │
│           ↓                                                    │
│  2. NOISE & DC SOURCES (from PM-Synth)                       │
│     ├─ White noise (LCG-based)                               │
│     ├─ DC level                                               │
│     └─ Mix: disyn_out + noise + dc                           │
│           ↓                                                    │
│  3. ENVELOPE MODULE (Attack/Release)                         │
│     ├─ Gate from MIDI note on/off                            │
│     ├─ Exponential attack time mapping                        │
│     ├─ Exponential release time mapping                       │
│     └─ Output: excitation × envelope                          │
│           ↓                                                    │
│  4. CHATTERBOX FORMANT BANK                                  │
│     ├─ F1 (Jaw): 200-1000 Hz biquad bandpass                │
│     ├─ F2 (Tongue): 500-3000 Hz biquad bandpass             │
│     ├─ F3 (Lips): 1500-4000 Hz biquad bandpass              │
│     ├─ F4 (Quality): 2500-4500 Hz biquad bandpass           │
│     ├─ Nasal (optional parallel): 250 Hz @ 0.3× level       │
│     ├─ Makeup gain: 3.0×                                      │
│     └─ Vocal modes applied:                                   │
│         • Sing: Vibrato on Disyn frequency (5.5 Hz ±1.5%)   │
│         • Shout: Formants × 1.15 (15% boost)                 │
│         • Fry: Disyn adds f₀/2 subharmonic                   │
│         • Nasal: Parallel 250Hz formant                      │
│           ↓                                                    │
│  5. DC BLOCKER (High-pass, R=0.995)                         │
│     └─ Applied to feedback path ONLY (not direct signal)     │
│           ↓                                                    │
│  6. FEEDBACK MIX                                             │
│     ├─ Previous delay1 output × feedback1 level              │
│     ├─ Previous delay2 output × feedback2 level              │
│     ├─ Previous filter output × filter_fb level              │
│     └─ Add to formant output                                  │
│           ↓                                                    │
│  7. INTERFACE MODULE (12 Strategies)                         │
│     ├─ Physical model nonlinearity (Reed, Pluck, etc.)      │
│     ├─ Intensity control                                      │
│     └─ Output: interface-shaped signal                        │
│           ↓                                                    │
│  8. DUAL DELAY LINES (Karplus-Strong)                       │
│     ├─ Delay1: tuned to MIDI pitch                           │
│     ├─ Delay2: tuned to pitch × ratio                        │
│     ├─ Linear interpolation                                   │
│     └─ Store previous outputs for feedback                    │
│           ↓                                                    │
│  9. STATE-VARIABLE FILTER                                    │
│     ├─ Frequency control                                      │
│     ├─ Q (resonance) control                                  │
│     ├─ Shape morph: LP ↔ BP ↔ HP                            │
│     └─ Store previous output for feedback                     │
│           ↓                                                    │
│ 10. MODULATION (LFO)                                         │
│     ├─ LFO frequency                                          │
│     ├─ AM ↔ FM bipolar depth                                │
│     └─ Apply AM to output amplitude                          │
│           ↓                                                    │
│ 11. POST-RELEASE DAMPING                                    │
│     └─ After note off: damping *= 0.995 per sample          │
│           ↓                                                    │
│ 12. VOICE OUTPUT                                             │
│     └─ Single sample ready for summing                        │
│                                                               │
└───────────────────────────────────────────────────────────────┘
    ↓
[Sum All 4 Voices]
    ↓
[Shared REVERB Module - Schroeder]
    ↓
[Master Gain]
    ↓
ALSA Output Buffer
```

### Key Design Decisions

1. **Disyn replaces Chatterbox's LarynxModule** as the tonal excitation source
2. **Noise + DC sources** remain from PM-Synth for added texture
3. **Chatterbox formants** shape the Disyn output into vocal-like spectra
4. **Vocal modes** modify either Disyn (Sing vibrato, Fry subharmonics) or formants (Shout boost, Nasal parallel)
5. **DC blocker** only on feedback path to prevent low-frequency buildup in closed loop
6. **Shared reverb** for all voices (CPU efficiency)
7. **Per-voice modulation** allows independent LFO phasing

---

## 2. VOICE MANAGEMENT SYSTEM

### Voice State Structure

```c
typedef struct {
    // Identity
    int midi_note;           // -1 if inactive, 0-127 if playing
    uint32_t age_counter;    // Allocation timestamp for stealing priority
    bool active;             // Voice is producing sound
    bool releasing;          // Envelope in release phase

    // Frequency
    float frequency;         // Current pitch in Hz
    float base_frequency;    // Base pitch (for modulation)

    // DSP Modules (per-voice instances)
    DisynModule* disyn;               // Disyn oscillator (7 algorithms)
    SourcesModule* sources;            // Noise + DC
    EnvelopeModule* envelope;          // AR envelope
    FormantBankModule* formant_bank;   // F1-F4 cascade + nasal
    InterfaceModule* interface;        // 12 strategies
    DelayLinesModule* delay_lines;     // Dual delays
    FeedbackModule* feedback;          // 3-tap mixer
    FilterModule* filter;              // SVF
    ModulationModule* modulation;      // LFO AM↔FM

    // State tracking
    float prev_delay1_out;
    float prev_delay2_out;
    float prev_filter_out;
    float post_release_damp;  // 1.0 initially, decays to 0 after release
    float last_output;        // For voice stealing level detection

    // DC blocker (on feedback path)
    float dc_blocker_x1;
    float dc_blocker_y1;

    // Parameter version (optimization)
    uint32_t params_version;  // Skip param sync if unchanged

} Voice;
```

### Voice Allocation Strategy (3-Tier, adapted from Floozy-Poly)

```c
Voice* allocate_voice(int midi_note, float frequency) {
    // Tier 1: Check if note already playing (retrigger)
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].midi_note == midi_note && voices[i].active) {
            retrigger_voice(&voices[i], frequency);
            return &voices[i];
        }
    }

    // Tier 2: Find first idle (inactive) voice
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!voices[i].active) {
            start_voice(&voices[i], midi_note, frequency);
            return &voices[i];
        }
    }

    // Tier 3: Voice stealing (all voices active)
    Voice* victim = select_voice_to_steal();
    if (victim) {
        start_voice(victim, midi_note, frequency);
        return victim;
    }

    return NULL;  // Should never happen
}
```

### Voice Stealing Algorithm (2-Pass for 4 Voices)

```c
Voice* select_voice_to_steal() {
    Voice* candidate = NULL;
    uint32_t oldest_age = UINT32_MAX;
    float lowest_level = FLT_MAX;

    // Pass 1: Prefer oldest releasing voice
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].releasing && voices[i].age_counter < oldest_age) {
            candidate = &voices[i];
            oldest_age = voices[i].age_counter;
        }
    }

    // Pass 2: If no releasing, steal quietest active voice
    if (!candidate) {
        for (int i = 0; i < MAX_VOICES; i++) {
            float level = fabsf(voices[i].last_output);
            if (level < lowest_level) {
                lowest_level = level;
                candidate = &voices[i];
            }
        }
    }

    return candidate;
}
```

### Voice Lifecycle

1. **Note On**: `start_voice()` → sets active=true, releasing=false, resets all modules, sets envelope gate=1
2. **Playing**: `voice_process()` called every sample
3. **Note Off**: `release_voice()` → sets releasing=true, envelope gate=0
4. **Post-Release Damping**: `post_release_damp *= 0.995f` per sample after envelope inactive
5. **Auto-Stop**: When envelope inactive AND post_release_damp < 1e-4 AND output < 1e-5 → `stop_voice()`
6. **Force Stop**: `stop_voice()` → active=false, midi_note=-1, resets all module state

---

## 3. MODULE INTEGRATION STRATEGY

### Module Translation Map

| Module | Source | Language | Translation Strategy |
|--------|--------|----------|---------------------|
| **DisynModule** | lv2/disyn | C++ | **Wrap with C interface** or port algorithms to C |
| **SourcesModule** | gtk-synth/pm-synth | C | **Direct copy** (noise + DC) |
| **EnvelopeModule** | Chatterbox version | C++ → C | **Port to C**, use exponential time mapping |
| **FormantModule** | lv2/chatterbox | C++ → C | **Port to C** (biquad bandpass, Q-based) |
| **FormantBankModule** | lv2/chatterbox | C++ → C | **Port to C** (4 formants + nasal) |
| **InterfaceModule** | gtk-synth/pm-synth | C | **Direct copy** (strategy pattern with vtables) |
| **DelayLinesModule** | gtk-synth/pm-synth | C | **Direct copy** (dual delays with interpolation) |
| **FeedbackModule** | gtk-synth/pm-synth | C | **Direct copy** (3-tap mixer) |
| **FilterModule** | gtk-synth/pm-synth | C | **Direct copy** (SVF) |
| **ModulationModule** | gtk-synth/pm-synth | C | **Direct copy** (LFO AM↔FM) |
| **ReverbModule** | gtk-synth/pm-synth | C | **Direct copy** (Schroeder) |

### Disyn Integration Options

**Option A: C Wrapper Around C++ Algorithms (Recommended)**
```c
// disyn_wrapper.h
typedef struct DisynModule DisynModule;

DisynModule* disyn_create(float sample_rate);
void disyn_destroy(DisynModule* disyn);
void disyn_set_algorithm(DisynModule* disyn, int algorithm);  // 0-6
void disyn_set_param1(DisynModule* disyn, float value);
void disyn_set_param2(DisynModule* disyn, float value);
float disyn_process(DisynModule* disyn, float frequency);

// disyn_wrapper.cpp (compiles as C++)
#include "lv2/disyn/src/modules/OscillatorModule.hpp"
extern "C" {
    struct DisynModule {
        flues::disyn::OscillatorModule osc;
    };
    // ... implement wrapper functions
}
```

**Option B: Port All Disyn Algorithms to C**
- More work upfront but cleaner build
- All code in C11, no C++ linkage needed
- Recommended for phase 2 if Option A causes issues

### Chatterbox Formant Porting

**FormantModule (Single Biquad Bandpass):**
```c
typedef struct {
    float sample_rate;
    float frequency;
    float bandwidth;  // Fixed per formant (F1=80, F2=120, F3=150, F4=200 Hz)

    // Biquad coefficients
    float a0, a1, a2;  // Numerator
    float b1, b2;      // Denominator (b0 implicit = 1)

    // State
    float x1, x2;  // Input history
    float y1, y2;  // Output history
} FormantModule;

void formant_set_frequency(FormantModule* f, float freq) {
    f->frequency = freq;
    // Recompute biquad coefficients using RBJ cookbook
    float Q = freq / f->bandwidth;
    float omega = 2.0f * M_PI * freq / f->sample_rate;
    float alpha = sinf(omega) / (2.0f * Q);
    // ... coefficient computation
}

float formant_process(FormantModule* f, float input) {
    float output = f->a0 * input + f->a1 * f->x1 + f->a2 * f->x2
                 - f->b1 * f->y1 - f->b2 * f->y2;
    // Shift state
    f->x2 = f->x1; f->x1 = input;
    f->y2 = f->y1; f->y1 = output;
    return output;
}
```

**FormantBankModule (Cascade + Nasal):**
```c
typedef struct {
    FormantModule f1;  // Jaw
    FormantModule f2;  // Tongue
    FormantModule f3;  // Lips
    FormantModule f4;  // Quality
    FormantModule nasal;  // Optional parallel at 250 Hz

    bool nasal_enabled;
    bool shout_enabled;  // 15% frequency boost
    float makeup_gain;   // 3.0×
} FormantBankModule;

float formant_bank_process(FormantBankModule* fb, float input) {
    // Series cascade
    float signal = input;
    signal = formant_process(&fb->f1, signal);
    signal = formant_process(&fb->f2, signal);
    signal = formant_process(&fb->f3, signal);
    signal = formant_process(&fb->f4, signal);

    // Parallel nasal (if enabled)
    if (fb->nasal_enabled) {
        float nasal_out = formant_process(&fb->nasal, input);
        signal += nasal_out * 0.3f;  // Mix at lower level
    }

    return signal * fb->makeup_gain;
}
```

### Envelope Unification

**Use Chatterbox's exponential time mapping:**
```c
typedef struct {
    float sample_rate;
    float attack_seconds;   // Exponential: 0.001 - 1.0 sec
    float release_seconds;  // Exponential: 0.01 - 3.0 sec
    float value;            // Current envelope level (0-1)
    bool gate;              // Note on/off
} EnvelopeModule;

void envelope_set_attack(EnvelopeModule* e, float normalized) {
    // Exponential mapping: 1ms to 1000ms
    e->attack_seconds = 0.001f * powf(1000.0f, normalized);
}

void envelope_set_release(EnvelopeModule* e, float normalized) {
    // Exponential mapping: 10ms to 3000ms
    e->release_seconds = 0.01f * powf(300.0f, normalized);
}

float envelope_process(EnvelopeModule* e) {
    if (e->gate) {
        // Attack phase
        float attack_rate = 1.0f / (e->attack_seconds * e->sample_rate);
        e->value += attack_rate;
        if (e->value > 1.0f) e->value = 1.0f;
    } else {
        // Release phase
        float release_rate = 1.0f / (e->release_seconds * e->sample_rate);
        e->value -= release_rate;
        if (e->value < 0.0f) e->value = 0.0f;
    }
    return e->value;
}
```

### Vocal Modes Implementation

**Vocal modes span multiple modules:**

1. **Nasal Mode**: Enable parallel nasal formant in FormantBankModule
2. **Sing Mode**: Apply vibrato to Disyn frequency (5.5 Hz ±1.5%)
   ```c
   if (vocal_modes.sing_enabled) {
       float vibrato = sinf(vibrato_phase) * 0.015f;  // ±1.5%
       frequency *= (1.0f + vibrato);
       vibrato_phase += 2.0f * M_PI * 5.5f / sample_rate;
   }
   ```
3. **Shout Mode**: Multiply all formant frequencies by 1.15
4. **Fry Mode**: Add f₀/2 subharmonic in Disyn (separate phase accumulator)

---

## 4. MIDI IMPLEMENTATION

### ALSA MIDI Backend Architecture

**Dual-mode MIDI input:**
1. **Hardware MIDI** (USB MIDI interfaces): ALSA rawmidi API
2. **Virtual MIDI** (DAW, software): ALSA sequencer API

```c
typedef struct {
    // Hardware MIDI
    snd_rawmidi_t* midi_in_handle;
    bool rawmidi_active;

    // ALSA Sequencer (virtual MIDI)
    snd_seq_t* seq_handle;
    int seq_port;
    bool seq_active;

    // MIDI event queue
    MidiEvent event_queue[MIDI_QUEUE_SIZE];
    int queue_read_idx;
    int queue_write_idx;

    pthread_t midi_thread;
    bool running;
} MidiBackend;
```

### MIDI Polling Thread

```c
void* midi_thread_func(void* arg) {
    MidiBackend* midi = (MidiBackend*)arg;

    while (midi->running) {
        // Poll hardware MIDI
        if (midi->rawmidi_active) {
            uint8_t buffer[256];
            int bytes_read = snd_rawmidi_read(midi->midi_in_handle, buffer, sizeof(buffer));
            if (bytes_read > 0) {
                parse_midi_bytes(midi, buffer, bytes_read);
            }
        }

        // Poll sequencer MIDI
        if (midi->seq_active) {
            snd_seq_event_t* ev;
            while (snd_seq_event_input(midi->seq_handle, &ev) >= 0) {
                handle_seq_event(midi, ev);
                snd_seq_free_event(ev);
            }
        }

        usleep(1000);  // 1ms poll interval
    }
    return NULL;
}
```

### MIDI Event Processing (in Audio Callback)

```c
void process_midi_events(MidiBackend* midi, SynthEngine* synth) {
    while (midi->queue_read_idx != midi->queue_write_idx) {
        MidiEvent* ev = &midi->event_queue[midi->queue_read_idx];

        switch (ev->type) {
            case MIDI_NOTE_ON:
                if (ev->velocity > 0) {
                    float freq = midi_note_to_frequency(ev->note);
                    synth_note_on(synth, ev->note, freq);
                } else {
                    synth_note_off(synth, ev->note);
                }
                break;

            case MIDI_NOTE_OFF:
                synth_note_off(synth, ev->note);
                break;

            case MIDI_CONTROL_CHANGE:
                synth_handle_cc(synth, ev->cc_number, ev->cc_value);
                break;

            case MIDI_ALL_NOTES_OFF:
                synth_all_notes_off(synth);
                break;
        }

        midi->queue_read_idx = (midi->queue_read_idx + 1) % MIDI_QUEUE_SIZE;
    }
}
```

### MIDI CC Mapping Strategy

**Organize CCs by functional group:**

| CC Range | Function | Parameters |
|----------|----------|------------|
| **1-10** | Modulation & Expression | Stress (1), F2/Tongue (10) |
| **71-75** | Formants | F1 (71), Release (72), Attack (73), F3 (74), F4 (75) |
| **80-83** | Vocal Mode Toggles | Nasal (80), Sing (81), Shout (82), Fry (83) |
| **16-23** | Disyn Source | Algorithm (16), Param1 (17), Param2 (18), Level (19), Noise (20), DC (21) |
| **24-27** | Interface/Delay | Type (24), Intensity (25), Tuning (26), Ratio (27) |
| **28-31** | Feedback | Delay1 (28), Delay2 (29), Filter (30) |
| **32-35** | Filter | Frequency (32), Q (33), Shape (34) |
| **36-37** | Modulation | LFO Freq (36), AM↔FM Depth (37) |
| **84-85, 91** | Reverb | Size (84), Level (85, 91) |
| **7, 102** | Output | Master Gain (7), Noise Level (102 alt) |

Toggle CCs (80-83): ≥64 = ON, <64 = OFF

---

## 5. ALSA AUDIO BACKEND

### Replacement for PulseAudio

**Architecture:**
```c
typedef struct {
    snd_pcm_t* pcm_handle;
    snd_pcm_hw_params_t* hw_params;

    float sample_rate;
    int buffer_size;     // Frames per period (256 or 512)
    int periods;         // 2-4 periods

    float* audio_buffer;  // Interleaved buffer for ALSA

    AudioProcessCallback callback;
    void* callback_user_data;

    pthread_t audio_thread;
    bool running;
} AudioBackendALSA;
```

### ALSA Configuration

```c
AudioBackendALSA* audio_backend_alsa_create(const char* device_name,
                                             float sample_rate,
                                             int buffer_size,
                                             AudioProcessCallback callback,
                                             void* user_data) {
    AudioBackendALSA* backend = calloc(1, sizeof(AudioBackendALSA));
    backend->sample_rate = sample_rate;
    backend->buffer_size = buffer_size;
    backend->callback = callback;
    backend->callback_user_data = user_data;

    // Open PCM device
    snd_pcm_open(&backend->pcm_handle, device_name, SND_PCM_STREAM_PLAYBACK, 0);

    // Allocate HW params
    snd_pcm_hw_params_malloc(&backend->hw_params);
    snd_pcm_hw_params_any(backend->pcm_handle, backend->hw_params);

    // Set parameters
    snd_pcm_hw_params_set_access(backend->pcm_handle, backend->hw_params,
                                  SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(backend->pcm_handle, backend->hw_params,
                                  SND_PCM_FORMAT_FLOAT_LE);  // 32-bit float
    snd_pcm_hw_params_set_channels(backend->pcm_handle, backend->hw_params, 1);  // Mono
    snd_pcm_hw_params_set_rate_near(backend->pcm_handle, backend->hw_params,
                                     (unsigned int*)&sample_rate, 0);
    snd_pcm_hw_params_set_period_size_near(backend->pcm_handle, backend->hw_params,
                                            (snd_pcm_uframes_t*)&buffer_size, 0);
    snd_pcm_hw_params_set_periods(backend->pcm_handle, backend->hw_params, 2, 0);

    // Apply params
    snd_pcm_hw_params(backend->pcm_handle, backend->hw_params);

    backend->audio_buffer = malloc(buffer_size * sizeof(float));

    return backend;
}
```

### Audio Thread Loop

```c
void* audio_thread_func(void* arg) {
    AudioBackendALSA* backend = (AudioBackendALSA*)arg;

    while (backend->running) {
        // Generate audio via callback
        backend->callback(backend->audio_buffer, backend->buffer_size,
                         backend->callback_user_data);

        // Write to ALSA
        int frames_written = snd_pcm_writei(backend->pcm_handle,
                                           backend->audio_buffer,
                                           backend->buffer_size);

        if (frames_written < 0) {
            // Handle underrun
            if (frames_written == -EPIPE) {
                snd_pcm_prepare(backend->pcm_handle);  // Recover
            }
        }
    }

    return NULL;
}
```

### Buffer Size Selection

**Recommended:** 256 frames @ 48kHz = 5.3ms latency
- Balance between latency and CPU overhead
- Lower = better response, higher CPU usage
- Higher = more CPU headroom, worse feel

**Alternative:** 512 frames @ 48kHz = 10.7ms latency (if 256 causes xruns)

---

## 6. BUILD SYSTEM & CONFIGURATION

### Meson Configuration

**meson.build (root project file):**
```meson
project('flues-synth', 'c', 'cpp',
        version: '0.1.0',
        default_options: ['c_std=c11', 'cpp_std=c++17', 'warning_level=2'])

# Build-time voice configuration
max_voices = get_option('max_voices')
add_project_arguments('-DMAX_VOICES=' + max_voices.to_string(), language: 'c')

# ARM-specific optimizations for Raspberry Pi
if host_machine.cpu_family() == 'aarch64' or host_machine.cpu_family() == 'arm'
    add_project_arguments(['-mcpu=cortex-a72', '-mfpu=neon', '-ftree-vectorize'],
                          language: ['c', 'cpp'])
endif

# Dependencies
gtk4_dep = dependency('gtk4')
alsa_dep = dependency('alsa')
threads_dep = dependency('threads')
math_dep = meson.get_compiler('c').find_library('m', required: true)

# Source files
sources = [
    'src/main.c',
    'src/synth_engine.c',
    'src/audio/audio_backend_alsa.c',
    'src/audio/midi_backend_alsa.c',
    'src/audio/modules/disyn_wrapper.cpp',  # C++ wrapper
    'src/audio/modules/sources_module.c',
    'src/audio/modules/envelope_module.c',
    'src/audio/modules/formant_module.c',
    'src/audio/modules/formant_bank_module.c',
    'src/audio/modules/interface_module.c',
    'src/audio/modules/delay_lines_module.c',
    'src/audio/modules/feedback_module.c',
    'src/audio/modules/filter_module.c',
    'src/audio/modules/modulation_module.c',
    'src/audio/modules/reverb_module.c',
    'src/audio/modules/strategies/reed_strategy.c',
    # ... other 11 strategies
    'src/ui/synth_window.c',
]

executable('flues-synth',
           sources,
           dependencies: [gtk4_dep, alsa_dep, threads_dep, math_dep],
           install: true)
```

**meson_options.txt:**
```meson
option('max_voices', type: 'integer', value: 4, min: 1, max: 16,
       description: 'Maximum polyphony (voices)')
option('enable_simd', type: 'boolean', value: false,
       description: 'Enable ARM NEON SIMD optimizations')
option('buffer_size', type: 'integer', value: 256, min: 64, max: 2048,
       description: 'ALSA buffer size in frames')
```

### Build Commands

```bash
# Standard build
meson setup builddir
meson compile -C builddir

# Configure for 8 voices
meson setup builddir -Dmax_voices=8

# Configure for maximum performance
meson setup builddir -Dmax_voices=4 -Denable_simd=true -Dbuffer_size=512

# Install
meson install -C builddir
```

### Compile-Time Configuration Header

**src/config.h (generated):**
```c
#ifndef FLUES_SYNTH_CONFIG_H
#define FLUES_SYNTH_CONFIG_H

// Voice count (from meson option)
#ifndef MAX_VOICES
#define MAX_VOICES 4
#endif

// Audio configuration
#define DEFAULT_SAMPLE_RATE 48000.0f
#define DEFAULT_BUFFER_SIZE 256

// MIDI configuration
#define MIDI_QUEUE_SIZE 256

// Delay line size
#define MAX_DELAY_LENGTH 8192

// Enable optimizations
#ifdef ENABLE_SIMD
#include <arm_neon.h>
#endif

#endif
```

---

## 7. UI DESIGN

### Control Organization (8 Groups)

**Layout:** Vertical stack of 8 control groups, each in a GtkFrame

#### Group 1: DISYN SOURCE (7 controls)
- **Algorithm Selector**: GtkDropDown (7 options)
  - Dirichlet Pulse, DSF Single, DSF Double, Tanh Square, Tanh Saw, PAF, Modified FM
- **Param1**: GtkScale (0-1, algorithm-specific meaning)
- **Param2**: GtkScale (0-1, algorithm-specific meaning)
- **Tone Level**: GtkScale (0-1)
- **Noise Level**: GtkScale (0-1)
- **DC Level**: GtkScale (0-1)
- **Vibrato (Sing Mode)**: GtkCheckButton

#### Group 2: FORMANTS (6 controls)
- **F1 (Jaw)**: GtkScale (200-1000 Hz, exponential)
- **F2 (Tongue)**: GtkScale (500-3000 Hz, exponential)
- **F3 (Lips)**: GtkScale (1500-4000 Hz, exponential)
- **F4 (Quality)**: GtkScale (2500-4500 Hz, exponential)
- **Vowel Preset Buttons**: A, E, I, O, U (5 × GtkButton, sets F1-F4)

#### Group 3: VOCAL MODES (4 controls)
- **Nasal**: GtkCheckButton
- **Sing (Vibrato)**: GtkCheckButton (redundant with Group 1, keep in one place)
- **Shout**: GtkCheckButton
- **Fry**: GtkCheckButton

#### Group 4: ENVELOPE (3 controls)
- **Attack**: GtkScale (0.001-1.0s, exponential)
- **Release**: GtkScale (0.01-3.0s, exponential)
- **Stress**: GtkScale (0-1, affects amplitude + soft clipping)

#### Group 5: INTERFACE & DELAY (4 controls)
- **Interface Type**: GtkDropDown (12 options: Pluck, Hit, Reed, Flute, Brass, Bow, Bell, Drum, Crystal, Vapor, Quantum, Plasma)
- **Intensity**: GtkScale (0-1)
- **Tuning**: GtkScale (-12 to +12 semitones)
- **Ratio**: GtkScale (0.5 to 2.0×)

#### Group 6: FEEDBACK & FILTER (6 controls)
- **Delay1 Feedback**: GtkScale (0-1)
- **Delay2 Feedback**: GtkScale (0-1)
- **Filter Feedback**: GtkScale (0-1)
- **Filter Frequency**: GtkScale (20-20000 Hz, exponential)
- **Filter Q**: GtkScale (0.1-10)
- **Filter Shape**: GtkScale (0=LP, 0.5=BP, 1=HP)

#### Group 7: MODULATION & REVERB (4 controls)
- **LFO Frequency**: GtkScale (0.1-20 Hz)
- **AM↔FM Depth**: GtkScale (-1 to +1, bipolar)
- **Reverb Size**: GtkScale (0-1)
- **Reverb Level**: GtkScale (0-1)

#### Group 8: OUTPUT & STATUS (4 controls + display)
- **Master Gain**: GtkScale (0-1)
- **CPU Meter**: GtkProgressBar (displays audio callback CPU %)
- **Voice Activity**: 4 × GtkLabel (shows MIDI note for each active voice)
- **MIDI Activity**: GtkLabel (blinks on MIDI event)

### Preset System

**Simple file-based presets:**
- Save all ~40 parameters to JSON or INI file
- Preset browser: GtkFileChooserButton
- Default presets: Vowel A-E-I-O-U, Brass, Reed, Pad, etc.

```c
typedef struct {
    // Disyn
    int algorithm;
    float param1, param2, tone_level, noise_level, dc_level;

    // Formants
    float f1, f2, f3, f4;
    bool nasal, sing, shout, fry;

    // Envelope
    float attack, release, stress;

    // Interface
    int interface_type;
    float intensity, tuning, ratio;

    // Feedback & Filter
    float delay1_fb, delay2_fb, filter_fb;
    float filter_freq, filter_q, filter_shape;

    // Modulation
    float lfo_freq, am_fm_depth;

    // Reverb
    float reverb_size, reverb_level;

    // Output
    float master_gain;
} SynthPreset;
```

### Total Control Count: ~40 parameters

---

## 8. FILE STRUCTURE

### Proposed Directory Layout

```
flues-synth/                      # New GTK synthesizer project
├── meson.build                   # Build configuration
├── meson_options.txt             # Configurable options
├── README.md                     # User documentation
├── docs/
│   ├── ARCHITECTURE.md           # System design overview
│   ├── PORTING_NOTES.md          # JavaScript→C translation guide
│   └── USER_GUIDE.md             # End-user manual
├── include/
│   ├── config.h                  # Build-time configuration
│   ├── synth_engine.h            # Main engine API
│   ├── audio_backend_alsa.h      # ALSA audio interface
│   ├── midi_backend_alsa.h       # ALSA MIDI interface
│   ├── dsp_modules.h             # All DSP module interfaces
│   └── dsp_utils.h               # Fast math, DC blocker, etc.
├── src/
│   ├── main.c                    # Application entry, GTK app init
│   ├── synth_engine.c            # Voice management, param routing
│   ├── audio/
│   │   ├── audio_backend_alsa.c  # ALSA PCM management + thread
│   │   ├── midi_backend_alsa.c   # ALSA rawmidi + sequencer + thread
│   │   └── modules/
│   │       ├── disyn_wrapper.h   # C interface to Disyn algorithms
│   │       ├── disyn_wrapper.cpp # C++ implementation
│   │       ├── sources_module.c/h          # Noise + DC
│   │       ├── envelope_module.c/h         # AR envelope
│   │       ├── formant_module.c/h          # Single biquad formant
│   │       ├── formant_bank_module.c/h     # 4 formants + nasal
│   │       ├── interface_module.c/h        # Strategy pattern coordinator
│   │       ├── delay_lines_module.c/h      # Dual delays
│   │       ├── feedback_module.c/h         # 3-tap mixer
│   │       ├── filter_module.c/h           # SVF
│   │       ├── modulation_module.c/h       # LFO AM↔FM
│   │       ├── reverb_module.c/h           # Schroeder reverb
│   │       └── strategies/
│   │           ├── interface_strategy.h    # Base strategy + vtable
│   │           ├── interface_factory.c/h   # Strategy factory
│   │           ├── reed_strategy.c         # Reed implementation
│   │           ├── pluck_strategy.c        # Pluck stub→implementation
│   │           └── ... (11 more strategies)
│   └── ui/
│       ├── synth_window.c/h      # Main GTK window + all controls
│       └── preset_manager.c/h    # Preset load/save
├── reference/                    # Copied source for reference
│   ├── lv2-disyn/                # Disyn algorithms (C++)
│   ├── lv2-chatterbox/           # Chatterbox formants (C++)
│   ├── lv2-floozy-poly/          # Voice management patterns (C++)
│   └── gtk-synth/                # Existing PM-Synth C code
└── presets/                      # Default preset files
    ├── vowel_a.json
    ├── vowel_e.json
    ├── brass.json
    └── ...
```

### Files to Copy/Adapt

| Source File | Destination | Changes Needed |
|-------------|-------------|----------------|
| `gtk-synth/src/audio/modules/sources_module.c` | `src/audio/modules/sources_module.c` | None (direct copy) |
| `gtk-synth/src/audio/modules/interface_module.c` | `src/audio/modules/interface_module.c` | None (direct copy) |
| `gtk-synth/src/audio/modules/delay_lines_module.c` | `src/audio/modules/delay_lines_module.c` | None (direct copy) |
| `gtk-synth/src/audio/modules/feedback_module.c` | `src/audio/modules/feedback_module.c` | None (direct copy) |
| `gtk-synth/src/audio/modules/filter_module.c` | `src/audio/modules/filter_module.c` | None (direct copy) |
| `gtk-synth/src/audio/modules/modulation_module.c` | `src/audio/modules/modulation_module.c` | None (direct copy) |
| `gtk-synth/src/audio/modules/reverb_module.c` | `src/audio/modules/reverb_module.c` | None (direct copy) |
| `lv2/chatterbox/src/modules/EnvelopeModule.hpp` | `src/audio/modules/envelope_module.c` | **Port C++ → C** |
| `lv2/chatterbox/src/modules/FormantModule.hpp` | `src/audio/modules/formant_module.c` | **Port C++ → C** |
| `lv2/chatterbox/src/modules/FormantBankModule.hpp` | `src/audio/modules/formant_bank_module.c` | **Port C++ → C** |
| `lv2/disyn/src/modules/OscillatorModule.hpp` | `src/audio/modules/disyn_wrapper.cpp` | **Wrap with C interface** |
| `gtk-synth/src/ui/synth_window.c` | `src/ui/synth_window.c` | **Extend with new controls** |
| `gtk-synth/docs/PORTING_NOTES.md` | `docs/PORTING_NOTES.md` | **Update with formant porting notes** |

---

## 9. PERFORMANCE OPTIMIZATION STRATEGY

### CPU Budget Estimation (Per Voice @ 48kHz)

| Module | Cycles/Sample | Notes |
|--------|---------------|-------|
| Disyn Oscillator | 20-40 | Algorithm-dependent (DSF most expensive) |
| Noise + DC | 2 | LCG + adds |
| Envelope | 3 | Linear ramp + compare |
| 4× Formant Cascade | 20 | 5 MACs × 4 filters |
| Nasal (conditional) | 5 | 1 biquad |
| DC Blocker | 2 | 1 high-pass |
| Interface Strategy | 10-20 | Algorithm-dependent |
| Dual Delays | 10 | Linear interpolation × 2 |
| Feedback Mix | 3 | 3 multiplies + adds |
| Filter (SVF) | 8 | State-variable topology |
| Modulation LFO | 5 | Sine lookup + AM/FM |
| Post-Release Damping | 1 | Multiply |
| **Total per voice** | **~90-120** | **~0.2% CPU @ 1.5 GHz** |

### 4-Voice Polyphony Budget

- Per-voice: ~100 cycles/sample
- 4 voices: ~400 cycles/sample
- Shared reverb: ~20 cycles/sample
- **Total: ~420 cycles/sample**
- **@ 48kHz = 20.2M cycles/sec = ~1.3% CPU @ 1.5 GHz**
- **Safety margin: 4× overhead → ~5% CPU**
- **Leaves 95% for GTK, MIDI, OS**

### Optimization Priorities

**Phase 1: Get it working**
- No premature optimization
- Use straightforward C implementations
- Profile on real Pi 4 hardware

**Phase 2: Targeted optimization (if needed)**
1. **Delay line interpolation**: Consider NEON SIMD for batch processing
2. **Formant bank cascade**: Vectorize 4 biquad filters (ARM NEON 4×float)
3. **Disyn algorithms**: Optimize hot paths (DSF sinc loop)
4. **LFO computation**: Use lookup table instead of `sinf()` per sample

**Phase 3: Advanced (if CPU still tight)**
- Cache-line alignment for Voice struct array
- Inline critical functions with `__attribute__((always_inline))`
- Use `-O3 -ffast-math` compilation flags
- Consider `restrict` pointers for aliasing hints

### Profiling Approach

```bash
# Build with profiling symbols
meson setup builddir -Dbuildtype=debugoptimized

# Run with perf on Pi 4
perf record -g ./builddir/flues-synth
perf report

# Identify hotspots, optimize iteratively
```

---

## 10. DEVELOPMENT PHASES

### Phase 1: Core Engine + Single Voice (2-3 weeks)

**Goal:** Audio output from one voice, no UI, hardcoded test parameters

**Tasks:**
1. Create project structure and meson.build
2. Port Disyn wrapper (C++ → C interface)
3. Port Chatterbox formants (C++ → C)
4. Port envelope module (C++ → C)
5. Copy PM-Synth modules (interface, delays, filter, modulation, reverb)
6. Implement SynthEngine with single voice
7. Implement ALSA audio backend
8. Write main.c with hardcoded note test
9. Verify signal flow: Disyn → Formants → PM pipeline → Audio out

**Success Criteria:**
- Builds on Ubuntu/Debian (then test on Pi 4)
- Produces sound when run
- Can hear Disyn algorithm, formants, and delay/reverb

### Phase 2: Polyphonic Voice Management (1-2 weeks)

**Goal:** 4-voice polyphony with voice stealing

**Tasks:**
1. Extend SynthEngine with Voice array[MAX_VOICES]
2. Implement 3-tier voice allocation (retrigger → idle → steal)
3. Implement 2-pass voice stealing algorithm
4. Add post-release damping and auto-stop
5. Test with programmatic note sequence (arpeggios, chords)
6. Verify voice stealing priority and smoothness

**Success Criteria:**
- 4 simultaneous notes play correctly
- Voice stealing is inaudible (or minimal click)
- CPU stays under 10% on Pi 4 @ 48kHz

### Phase 3: MIDI Implementation (1 week)

**Goal:** Hardware MIDI input and virtual MIDI support

**Tasks:**
1. Implement ALSA rawmidi backend (hardware USB MIDI)
2. Implement ALSA sequencer backend (virtual MIDI)
3. Create MIDI event queue and polling thread
4. Implement note on/off handling
5. Implement CC mapping for all ~40 parameters
6. Implement All Notes Off / Panic (CC 120, 123)
7. Test with hardware MIDI keyboard
8. Test with virtual MIDI from DAW (aconnect)

**Success Criteria:**
- USB MIDI keyboard controls synth
- Virtual MIDI from DAW works
- All CCs mapped and functional
- No MIDI jitter or stuck notes

### Phase 4: GTK UI (2-3 weeks)

**Goal:** Full control panel with all parameters

**Tasks:**
1. Design GTK window layout (8 control groups)
2. Implement Group 1: Disyn Source (algorithm dropdown, 6 sliders, 1 checkbox)
3. Implement Group 2: Formants (4 sliders, 5 vowel preset buttons)
4. Implement Group 3: Vocal Modes (4 checkboxes)
5. Implement Group 4: Envelope (3 sliders)
6. Implement Group 5: Interface & Delay (1 dropdown, 3 sliders)
7. Implement Group 6: Feedback & Filter (6 sliders)
8. Implement Group 7: Modulation & Reverb (4 sliders)
9. Implement Group 8: Output & Status (1 slider, CPU meter, voice display)
10. Connect all GTK signals to synth parameter setters
11. Implement preset save/load system
12. Add keyboard note input (A-K keys → MIDI notes)

**Success Criteria:**
- All 40+ parameters controllable from UI
- UI updates don't cause audio glitches
- Preset save/load works
- CPU meter displays accurate load

### Phase 5: Optimization & Testing (1-2 weeks)

**Goal:** Polish, optimize, and prepare for release

**Tasks:**
1. Profile on Raspberry Pi 4 with real workloads
2. Optimize hot paths if CPU > 20%
3. Consider NEON SIMD for critical modules
4. Test all 7 Disyn algorithms
5. Test all 12 interface strategies
6. Test all vocal modes (nasal, sing, shout, fry)
7. Test preset system with 10+ presets
8. Write user documentation (README, USER_GUIDE)
9. Create systemd service file for autostart
10. Package for Debian/Raspbian (deb package)

**Success Criteria:**
- CPU < 15% with 4 voices on Pi 4
- No audio dropouts or xruns
- All features functional
- Documentation complete

### Total Estimated Duration: 7-11 weeks

---

## 11. CRITICAL FILES TO REFERENCE DURING IMPLEMENTATION

### For Signal Flow Design
- `lv2/floozy-poly/src/FloozyEngine.hpp` (lines 123-173: voice process loop)
- `lv2/chatterbox/src/ChatterboxEngine.hpp` (lines 227-280: formant cascade + vocal modes)
- `gtk-synth/src/audio/pm_synth_engine.c` (lines 180-250: single-voice processing)

### For Voice Management
- `lv2/floozy-poly/src/FloozyEngine.hpp` (lines 315-403: allocation + stealing)
- Voice struct definition (lines 59-110)

### For Disyn Algorithms
- `lv2/disyn/src/modules/OscillatorModule.hpp` (full file: all 7 algorithms)
- Algorithm enum (lines 11-19)

### For Formant Porting
- `lv2/chatterbox/src/modules/FormantModule.hpp` (complete biquad implementation)
- `lv2/chatterbox/src/modules/FormantBankModule.hpp` (cascade + nasal)
- Coefficient computation (RBJ cookbook in updateCoefficients methods)

### For MIDI Implementation
- `lv2/chatterbox/src/chatterbox_plugin.cpp` (lines 231-353: CC mapping)
- `lv2/floozy-poly/src/floozy_plugin.cpp` (lines 125-157: MIDI handling)

### For ALSA Audio Backend
- `gtk-synth/src/audio/audio_backend_pulse.c` (adapt threading pattern)
- ALSA documentation: `/usr/share/doc/libasound2-dev/html/pcm.html`

### For Build System
- `gtk-synth/meson.build` (base meson configuration)
- `lv2/floozy-poly/CMakeLists.txt` (mixed C/C++ build patterns)

### For UI Design
- `gtk-synth/src/ui/synth_window.c` (GTK4 control patterns)
- GTK4 documentation: https://docs.gtk.org/gtk4/

---

## 12. RISK MITIGATION

### Risk 1: Disyn C++ → C Translation Complexity
**Mitigation:** Use C wrapper around C++ (Option A). Keep Disyn algorithms as-is in C++, expose simple C interface. Only port to C if build issues arise.

### Risk 2: CPU Overload on Pi 4
**Mitigation:** Start with 4 voices, configurable at build time. Profile early (Phase 2). Optimize only if needed. Consider reducing formant count or reverb quality.

### Risk 3: ALSA Audio Dropouts (xruns)
**Mitigation:** Use 512-frame buffer if 256 causes issues. Increase period count. Disable realtime priority if Linux RT patches unavailable. Test with `stress-ng` CPU load.

### Risk 4: MIDI Latency
**Mitigation:** Use 1ms polling interval. Process MIDI events at start of audio callback. Consider using ALSA sequencer timestamp ordering.

### Risk 5: GTK UI Blocking Audio Thread
**Mitigation:** Never call audio code from GTK thread. Use atomic operations or simple floats for parameter updates. No mutexes in audio callback.

---

## 13. SUCCESS CRITERIA

### Functional Requirements
- ✅ Produces audio output on Raspberry Pi 4
- ✅ 4-voice polyphony with smooth voice stealing
- ✅ All 7 Disyn algorithms working
- ✅ All 4 formants (F1-F4) + nasal functional
- ✅ All 4 vocal modes (nasal, sing, shout, fry) working
- ✅ All 12 interface strategies available
- ✅ Hardware MIDI input functional
- ✅ Virtual MIDI input functional
- ✅ Full GTK UI with ~40 controls
- ✅ Preset save/load working

### Performance Requirements
- ✅ CPU usage < 20% with 4 voices @ 48kHz on Pi 4
- ✅ Audio latency < 15ms (buffer size ≤ 512 frames)
- ✅ No audio dropouts during normal operation
- ✅ MIDI response latency < 5ms

### Quality Requirements
- ✅ No audible clicks during voice stealing
- ✅ No DC offset in output signal
- ✅ No stuck notes or hanging voices
- ✅ Smooth parameter changes (no zipper noise)

---

## 14. NEXT STEPS AFTER APPROVAL

1. Create `flues-synth/` directory structure
2. Set up initial meson.build with dependencies
3. Copy reference code to `reference/` directory
4. Begin Phase 1: Core Engine implementation
5. Set up git repository with proper .gitignore
6. Create initial documentation (ARCHITECTURE.md)

---

## APPENDIX: Key Translation Patterns from PORTING_NOTES.md

### Pattern 1: Class → Struct + Functions
```cpp
// JavaScript/C++
class MyModule {
    float value;
    float process(float input) { return input * value; }
};
```
```c
// C
typedef struct {
    float value;
} MyModule;

MyModule* my_module_create() { /* allocate */ }
float my_module_process(MyModule* m, float input) { return input * m->value; }
void my_module_destroy(MyModule* m) { free(m); }
```

### Pattern 2: Strategy Pattern with Vtables
```c
typedef struct {
    float (*process)(struct InterfaceStrategy* self, float input);
    void (*reset)(struct InterfaceStrategy* self);
    void (*destroy)(struct InterfaceStrategy* self);
} InterfaceStrategyVTable;

typedef struct InterfaceStrategy {
    const InterfaceStrategyVTable* vtable;
    void* impl_data;  // Private strategy-specific data
} InterfaceStrategy;

// Usage
float output = strategy->vtable->process(strategy, input);
```

### Pattern 3: Exponential Parameter Mapping
```c
// Attack: 1ms to 1000ms exponential
float attack_seconds = 0.001f * powf(1000.0f, normalized_param);

// Frequency: 20Hz to 20kHz exponential
float frequency = 20.0f * powf(1000.0f, normalized_param);
```

### Pattern 4: Biquad Filter Implementation
```c
typedef struct {
    float a0, a1, a2;  // Numerator coefficients
    float b1, b2;      // Denominator (b0 = 1 implicit)
    float x1, x2;      // Input history
    float y1, y2;      // Output history
} BiquadFilter;

float biquad_process(BiquadFilter* f, float input) {
    float output = f->a0 * input + f->a1 * f->x1 + f->a2 * f->x2
                 - f->b1 * f->y1 - f->b2 * f->y2;
    f->x2 = f->x1; f->x1 = input;
    f->y2 = f->y1; f->y1 = output;
    return output;
}
```

---

**End of Plan**
