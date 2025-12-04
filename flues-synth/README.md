# Flues-Synth

Unified polyphonic synthesizer combining Disyn distortion algorithms, Chatterbox formant synthesis, and PM-Synth physical modeling. Designed for headless operation on Raspberry Pi 4 with MIDI control.

## Features

- **Disyn Oscillators**: 7 distortion synthesis algorithms (Dirichlet Pulse, DSF, Tanh, PAF, Modified FM)
- **Chatterbox Formants**: 4-formant cascade (F1-F4) with nasal resonance and vocal modes
- **PM-Synth Physical Modeling**: 12 interface strategies (Pluck, Hit, Reed, Flute, Brass, Bow, Bell, Drum, Crystal, Vapor, Quantum, Plasma)
- **ALSA Audio**: Direct PCM output (48kHz, 256 samples, ~5ms latency)
- **ALSA MIDI**: Auto-connects to best external MIDI port on startup
- **Headless Control**: Comprehensive MIDI CC mapping (40+ parameters)
- **Single Voice**: Phase 1 implementation (polyphony coming in Phase 2)

## Signal Flow

```
MIDI Input
  ↓
Disyn Oscillators (7 algorithms)
  ↓
Sources (Noise + DC)
  ↓
Envelope (Attack/Release)
  ↓
Chatterbox Formants (F1→F2→F3→F4 cascade + nasal)
  ↓
Feedback Mix (Delay1 + Delay2 + Filter returns)
  ↓
Interface Strategy (12 physical models)
  ↓
Dual Delay Lines (Karplus-Strong with tuning/ratio)
  ↓
State-Variable Filter (LP/BP/HP morph)
  ↓
LFO Modulation (AM ↔ FM)
  ↓
Master Gain → ALSA Audio Out
```

## Requirements

### Raspberry Pi 4
- Raspberry Pi OS (64-bit recommended)
- 2GB+ RAM

### Dependencies
```bash
sudo apt update
sudo apt install build-essential meson ninja-build libasound2-dev
```

## Build Instructions

### Basic Build
```bash
cd flues-synth
meson setup builddir
meson compile -C builddir
```

### Configuration Options

**Set voice count** (default: 4):
```bash
meson setup builddir -Dmax_voices=8
meson compile -C builddir
```

**Enable ARM NEON optimizations** (Raspberry Pi 4):
```bash
meson setup builddir -Denable_simd=true
meson compile -C builddir
```

**Change audio device** (run-time):
```bash
./builddir/flues-synth hw:1,0
```

### Clean Build
```bash
rm -rf builddir
meson setup builddir
meson compile -C builddir
```

## Running

### Start the Synthesizer
```bash
cd flues-synth
./builddir/flues-synth
```

Expected output:
```
=== Flues-Synth v0.1.0 ===
Unified Polyphonic Synthesizer (Phase 1: Single Voice)
Target: Raspberry Pi 4 (ARM Cortex-A72)

Audio device: default
Sample rate: 48000 Hz
Buffer size: 256 frames (5.3 ms)

Synth engine initialized
MIDI: Found port 20:0 'USB MIDI Device' (score: 18)
MIDI: Auto-connected to port 20:0
MIDI: Initialized (client ID: 128, port: 0)

=== Flues-Synth Running ===
Listening for MIDI input...
Press Ctrl+C to quit
```

### Manual MIDI Connection

If no MIDI device is found, connect manually:

```bash
# List ALSA MIDI ports
aconnect -l

# Connect your MIDI device to Flues-Synth
aconnect 20:0 128:0
```

### Stop the Synthesizer
Press `Ctrl+C` for clean shutdown.

## MIDI CC Mapping

### Standard Controls
| CC  | Parameter       | Range         | Description                    |
|-----|-----------------|---------------|--------------------------------|
| 1   | Intensity       | 0-1           | Interface intensity (mod wheel)|
| 7   | Master Gain     | 0-1           | Output volume                  |
| 10  | F2 (Tongue)     | 500-3000 Hz   | Second formant frequency       |
| 71  | F1 (Jaw)        | 200-1000 Hz   | First formant frequency        |
| 72  | Release         | 0-1           | Envelope release time          |
| 73  | Attack          | 0-1           | Envelope attack time           |
| 74  | F3 (Lips)       | 1500-4000 Hz  | Third formant frequency        |
| 75  | F4 (Quality)    | 2500-4500 Hz  | Fourth formant frequency       |

### Vocal Modes (Toggle: ≥64 = ON)
| CC  | Parameter | Description                              |
|-----|-----------|------------------------------------------|
| 80  | Nasal     | Enable nasal formant (250 Hz)           |
| 81  | Sing      | Vibrato (5.5 Hz ±1.5%)                  |
| 82  | Shout     | 15% formant frequency boost             |
| 83  | Fry       | Vocal fry (f₀/2 subharmonic)            |

### Disyn Source
| CC  | Parameter     | Range   | Description                |
|-----|---------------|---------|----------------------------|
| 16  | Algorithm     | 0-6     | Disyn synthesis algorithm  |
| 17  | Param1        | 0-1     | Algorithm parameter 1      |
| 18  | Param2        | 0-1     | Algorithm parameter 2      |
| 19  | Disyn Level   | 0-1     | Oscillator mix level       |
| 20  | Noise Level   | 0-1     | White noise mix level      |
| 21  | DC Level      | 0-1     | DC offset mix level        |

### Interface & Delay
| CC  | Parameter       | Range        | Description                    |
|-----|-----------------|--------------|--------------------------------|
| 24  | Interface Type  | 0-11         | Physical model (see below)     |
| 26  | Tuning          | -12 to +12   | Delay line pitch offset        |
| 27  | Ratio           | 0.5 to 2.0   | Delay2/Delay1 length ratio     |

### Feedback
| CC  | Parameter         | Range | Description                    |
|-----|-------------------|-------|--------------------------------|
| 28  | Delay1 Feedback   | 0-1   | First delay return level       |
| 29  | Delay2 Feedback   | 0-1   | Second delay return level      |
| 30  | Filter Feedback   | 0-1   | Filter output return level     |

### Filter
| CC  | Parameter     | Range          | Description                |
|-----|---------------|----------------|----------------------------|
| 32  | Frequency     | 20-20000 Hz    | Filter cutoff frequency    |
| 33  | Q             | 0.1-10         | Filter resonance           |
| 34  | Shape         | 0-1            | LP (0) → BP (0.5) → HP (1) |

### Modulation
| CC  | Parameter    | Range         | Description                     |
|-----|--------------|---------------|---------------------------------|
| 36  | LFO Freq     | 0.1-20 Hz     | Modulation LFO frequency        |
| 37  | AM ↔ FM      | -1 to +1      | AM (-1) ↔ None (0) ↔ FM (+1)   |

## Interface Types

| ID  | Name     | Description                        | Character                    |
|-----|----------|------------------------------------|------------------------------|
| 0   | Pluck    | Karplus-Strong plucked string     | Bright, metallic attack      |
| 1   | Hit      | Struck/mallet percussion          | Sharp transient, decay       |
| 2   | Reed     | Clarinet-style reed instrument    | Woody, breathy               |
| 3   | Flute    | Jet-edge aerophone                | Airy, hollow                 |
| 4   | Brass    | Lip-buzz brass instrument         | Buzzy, brassy                |
| 5   | Bow      | Bowed string (friction)           | Sustained, singing           |
| 6   | Bell     | Metallic resonator                | Bright, ringing              |
| 7   | Drum     | Membrane percussion               | Thuddy, resonant             |
| 8   | Crystal  | Golden-ratio coupled oscillators  | Glassy, shimmering           |
| 9   | Vapor    | Chaotic turbulent flow            | Unstable, evolving           |
| 10  | Quantum  | Phase-locked harmonic coupling    | Metallic, interference       |
| 11  | Plasma   | Amplitude-driven energy feedback  | Aggressive, growling         |

## Disyn Algorithms

| ID  | Name              | Description                              |
|-----|-------------------|------------------------------------------|
| 0   | Dirichlet Pulse   | Sum of harmonics (Param1: harmonics)     |
| 1   | DSF Single        | Discrete summation formula               |
| 2   | DSF Double        | Two DSF oscillators (Param2: detune)     |
| 3   | Tanh Square       | Waveshaped square wave                   |
| 4   | Tanh Saw          | Waveshaped sawtooth wave                 |
| 5   | PAF               | Phase-aligned formant                    |
| 6   | Modified FM       | Phase-modulated FM synthesis             |

## Troubleshooting

### No Audio Output
```bash
# List audio devices
aplay -l

# Test audio device
speaker-test -D default -c 1

# Try different device
./builddir/flues-synth hw:1,0
```

### No MIDI Connection
```bash
# List MIDI ports
aconnect -l

# Check if device is detected
cat /proc/asound/cards

# Manual connection
aconnect <source-port> <flues-synth-port>
```

### Buffer Underruns (ALSA XRUN)
- Increase buffer size in `include/config.h`:
  ```c
  #define DEFAULT_BUFFER_SIZE 512  // Double from 256
  ```
- Rebuild: `meson compile -C builddir`

### High CPU Usage
- Disable ARM optimizations: `meson setup builddir -Denable_simd=false`
- Reduce voice count: `meson setup builddir -Dmax_voices=1`

### Build Errors
```bash
# Clean build
rm -rf builddir
meson setup builddir
meson compile -C builddir

# Check dependencies
dpkg -l | grep -E 'meson|ninja|alsa'
```

## Performance

**Raspberry Pi 4 (Cortex-A72):**
- Single voice: ~5-10% CPU @ 48kHz
- Target (4 voices): ~20-30% CPU
- Latency: ~5.3ms (256 samples)

## Notes for Raspberry Pi bring-up

- The app now auto-falls back through common Pi headphone devices if no CLI device is given: `hw:Headphones`, `plughw:Headphones`, `hw:2,0`, `plughw:2,0`, `hw:1,0`, then `default`. You can still override: `./builddir/flues-synth hw:2,0`.
- A smoke test verifies the default DSP path produces non-zero output: `meson test -C builddir engine-smoke`.
- Quick rebuild on Pi: `meson setup builddir --reconfigure && meson compile -C builddir && meson test -C builddir engine-smoke`.

 meson compile -C flues-synth/builddir
  meson test -C flues-synth/builddir engine-smoke
  ./flues-synth/builddir/flues-synth hw:2,0

  
## Architecture

```
flues-synth/
├── include/
│   ├── config.h                    # Build-time configuration
│   ├── dsp_modules.h               # Module interfaces
│   ├── dsp_utils.h                 # DSP utilities
│   ├── synth_engine.h              # Main coordinator
│   ├── audio_backend_alsa.h        # ALSA audio
│   └── midi_backend_alsa.h         # ALSA MIDI
├── src/
│   ├── main.c                      # Entry point + CC mapping
│   ├── synth_engine.c              # Voice management + signal flow
│   └── audio/
│       ├── audio_backend_alsa.c    # ALSA PCM output
│       ├── midi_backend_alsa.c     # ALSA sequencer input
│       └── modules/
│           ├── disyn_wrapper.cpp   # C++ Disyn → C interface
│           ├── sources_module.c    # Noise + DC
│           ├── envelope_module.c   # Attack/Release
│           ├── formant_module.c    # Biquad bandpass filter
│           ├── formant_bank_module.c # 4-formant cascade
│           ├── interface_module.c  # Strategy pattern
│           ├── delay_lines_module.c # Dual delays
│           ├── feedback_module.c   # 3-way mixer
│           ├── filter_module.c     # State-variable filter
│           ├── modulation_module.c # LFO
│           └── strategies/         # 12 interface implementations
├── meson.build                     # Build configuration
├── meson_options.txt               # Build options
└── README.md                       # This file
```

## Development Roadmap

### Phase 1 (Current)
- ✅ Single-voice operation
- ✅ Disyn + Chatterbox + PM-Synth integration
- ✅ ALSA audio/MIDI backends
- ✅ MIDI CC control
- ✅ Auto-connect to MIDI devices

### Phase 2 (Next)
- ⚠ 4-voice polyphony
- ⚠ Voice allocation and stealing
- ⚠ Per-voice parameter management
- ⚠ Polyphonic aftertouch

### Phase 3 (Future)
- ☐ GTK4 UI (optional)
- ☐ MIDI learn
- ☐ Preset system
- ☐ Performance profiling

## License

Part of the Flues project. See main repository for license information.

## Related Projects

- **experiments/disyn**: JavaScript distortion synthesizer
- **experiments/chatterbox**: JavaScript formant speech synthesizer
- **experiments/pm-synth**: JavaScript physical modeling synthesizer
- **lv2/disyn**: Disyn LV2 plugin
- **lv2/chatterbox**: Chatterbox LV2 plugin
- **lv2/floozy**: Hybrid Disyn+PM LV2 plugin
- **gtk-synth**: GTK4 desktop PM-Synth

## Credits

- **Disyn**: Distortion synthesis algorithms
- **Chatterbox**: Formant synthesis and vocal modes
- **PM-Synth**: Physical modeling framework
- Built with: Meson, ALSA, C11/C++17
