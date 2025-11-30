# Chatterbox LV2 Plugin

A speech synthesis LV2 instrument plugin based on formant filtering and source-filter vocal tract modeling.

## Overview

Chatterbox is a monophonic speech synthesizer that generates human-like vocal sounds using:
- **Source-filter model:** Larynx oscillator + aspirator noise → formant cascade
- **Four formants (F1-F4):** Control jaw, tongue, lips, and voice quality
- **Vocal modes:** Nasal resonance, vibrato (sing), shout, vocal fry
- **Dynamic processing:** Attack/release envelope, stress control with soft clipping
- **Built-in reverb:** Schroeder reverb with size and level controls

## Features

### Sound Generation
- **Larynx:** Modified sawtooth oscillator with cubic waveshaping (0.15 coefficient)
- **Aspirator:** White noise generator for unvoiced sounds
- **Formant Bank:** Cascade of four biquad bandpass filters (F1-F4) + optional nasal formant

### Vocal Modes
- **Nasal:** Adds 250 Hz parallel formant for nasal resonance
- **Sing (Vibrato):** 5.5 Hz sine LFO, ±1.5% depth
- **Shout:** 15% formant frequency boost + 50% increased noise
- **Fry (Vocal Fry):** f₀/2 subharmonic at 30% amplitude (creaky voice)

### Formant Ranges
- **F1 (Jaw):** 200-1000 Hz - Controls jaw opening
- **F2 (Tongue):** 500-3000 Hz - Controls tongue front/back position
- **F3 (Lips):** 1500-4000 Hz - Controls lip rounding
- **F4 (Quality):** 2500-4500 Hz - Controls voice quality/brightness

### Processing
- **Stress:** 0.5-2.0× gain mapping with tanh soft clipping above 0.6
- **Envelope:** Exponential attack (0.001-1.0s) and release (0.01-3.0s)
- **Reverb:** Schroeder reverb with configurable room size and wet/dry mix

## Installation

### Build from Source

```bash
cd lv2/chatterbox
cmake -S . -B build
cmake --build build
cmake --install build --prefix ~/.lv2
```

### Verify Installation

```bash
lv2ls | grep chatterbox
lv2info https://danja.github.io/flues/plugins/chatterbox
```

## Usage

### Loading in a DAW

Load in any LV2-compatible host:
- **Ardour:** Add instrument track, select "Chatterbox"
- **Carla:** Add plugin → Instrument → Chatterbox
- **Jalv:** `jalv.gtk https://danja.github.io/flues/plugins/chatterbox`

### MIDI Control

The plugin responds to:
- **Note On/Off:** Triggers envelope and sets pitch (overrides pitch knob during note)
- **Velocity:** Controls note amplitude (0-127 → 0.0-1.0)
- **All Notes Off (CC 123):** Panic button

#### MIDI CC Mappings

**Standard CCs:**
- **CC 1 (Mod Wheel):** Stress (0-127 → 0.0-1.0)
- **CC 7 (Volume):** Master Gain (0-127 → 0.0-1.0)
- **CC 10 (Pan):** F2 - Tongue position (0-127 → 500-3000 Hz)

**Sound Controllers:**
- **CC 71 (Resonance):** F1 - Jaw opening (0-127 → 200-1000 Hz)
- **CC 72 (Release):** Envelope Release (0-127 → 0.0-1.0)
- **CC 73 (Attack):** Envelope Attack (0-127 → 0.0-1.0)
- **CC 74 (Brightness):** F3 - Lip rounding (0-127 → 1500-4000 Hz)
- **CC 75 (Decay):** F4 - Voice quality (0-127 → 2500-4500 Hz)

**Vocal Mode Toggles (on when ≥ 64):**
- **CC 80:** Nasal resonance
- **CC 81:** Sing (vibrato)
- **CC 82:** Shout (+15% formants)
- **CC 83:** Fry (vocal fry)

**Effects:**
- **CC 84:** Reverb Size (0-127 → 0.0-1.0)
- **CC 85:** Reverb Level (0-127 → 0.0-1.0)
- **CC 91 (Effects Level):** Reverb Level (alternative)
- **CC 102:** Noise Level (0-127 → 0.0-1.0)

### Parameters (20 Control Ports)

#### Source Group
- **Pitch:** Base pitch 80-400 Hz (exponential, default 120 Hz at 0.3)
- **Voiced:** Enable larynx oscillator (toggle, default ON)
- **Aspirated:** Enable noise generator (toggle, default OFF)
- **Noise Level:** Aspiration amount (0-1, default 0.2)

#### Formants Group
- **F1 (Jaw):** 200-1000 Hz (default 0.5 → ~632 Hz)
- **F2 (Tongue):** 500-3000 Hz (default 0.4 → ~1225 Hz)
- **F3 (Lips):** 1500-4000 Hz (default 0.5 → ~2449 Hz)
- **F4 (Quality):** 2500-4500 Hz (default 0.5 → ~3354 Hz)

#### Vocal Modes Group
- **Nasal:** Add 250 Hz nasal formant (toggle, default OFF)
- **Sing:** 5.5 Hz vibrato, ±1.5% depth (toggle, default OFF)
- **Shout:** 15% formant boost + more noise (toggle, default OFF)
- **Fry:** Subharmonic at f₀/2 (toggle, default OFF)

#### Dynamics Group
- **Stress:** Amplitude 0.5-2.0× + soft clipping (0-1, default 0.3)
- **Attack:** Envelope attack time 1-1000 ms (exponential, default 0.33)
- **Release:** Envelope release time 10-3000 ms (exponential, default 0.33)

#### Effects Group
- **Reverb Size:** Room size simulation (0-1, default 0.3)
- **Reverb Level:** Wet/dry mix (0-1, default 0.2)

#### Output Group
- **Master Gain:** Final output level (0-1, default 0.8)

## UI Controls

The X11/Cairo UI provides:
- **Rotary knobs** for continuous parameters (drag vertically or scroll)
- **LED indicators** for toggle switches (click to toggle)
- **Six logical groups** organizing controls by function
- **Real-time visual feedback** for parameter changes

### UI Layout
```
Row 1: [Source: Pitch, Voiced, Aspirated, Noise] [Formants: F1, F2, F3, F4]
Row 2: [Vocal Modes: Nasal, Sing, Shout, Fry] [Dynamics: Stress, Attack, Release]
Row 3: [Reverb: Size, Level] [Output: Master]
```

## Sound Design Tips

### Basic Vowels
Create vowel sounds by adjusting F1 and F2:
- **"ah" (father):** F1 = 0.66, F2 = 0.24
- **"eh" (bed):** F1 = 0.41, F2 = 0.53
- **"ee" (see):** F1 = 0.09, F2 = 0.71
- **"oh" (home):** F1 = 0.46, F2 = 0.14
- **"oo" (boot):** F1 = 0.13, F2 = 0.15

### Expressive Techniques
- **Choir-like:** Enable Sing (vibrato) + moderate Reverb
- **Aggressive:** Enable Shout + increase Stress to 0.7+
- **Creaky voice:** Enable Fry at low pitch
- **Whispered:** Disable Voiced, enable Aspirated, Noise = 0.5
- **Nasal tones:** Enable Nasal + adjust F1 low

### Performance
- Use MIDI velocity for dynamics
- Pitch wheel for expressive pitch bends
- Automate formants for vowel morphing
- Layer with reverb/delay for ambient textures

## Technical Details

### Architecture
- **DSP Language:** C++ (header-only modules)
- **UI Language:** C (X11/Cairo)
- **Sample Rate:** Inherits from host (tested at 44.1/48 kHz)
- **Latency:** Zero latency (real-time synthesis)
- **Polyphony:** Monophonic (last note priority)

### Signal Flow
```
MIDI → Note Frequency
         ↓
    [Larynx (sawtooth)] ←—— Sing (vibrato)
         +              ←—— Fry (subharmonic)
    [Aspirator (noise)]
         ↓
    [Formant Cascade]  ←—— Shout (15% boost)
    (F1 → F2 → F3 → F4) ←—— Nasal (parallel 250Hz)
         ↓
    [Envelope AR]
         ↓
    [Stress Processing]
    (gain + soft clip)
         ↓
    [Reverb]
         ↓
    [Master Gain] → Output
```

### DSP Modules (src/modules/)
- `LarynxModule.hpp` - Modified sawtooth with vibrato and vocal fry
- `AspiratorModule.hpp` - LCG white noise generator
- `FormantModule.hpp` - Biquad bandpass filter (Q-based design)
- `FormantBankModule.hpp` - Cascade of 4 formants + nasal
- `EnvelopeModule.hpp` - Linear AR envelope with exponential time mapping
- `ReverbModule.hpp` - Schroeder reverb (shared from pm-synth)

### Files
```
lv2/chatterbox/
├── CMakeLists.txt              # Build configuration
├── chatterbox.lv2/
│   ├── manifest.ttl            # LV2 plugin registration
│   └── chatterbox.ttl          # Port definitions (20 params)
├── src/
│   ├── ChatterboxEngine.hpp    # Main DSP coordinator
│   ├── chatterbox_plugin.cpp   # LV2 wrapper with MIDI
│   ├── modules/                # DSP modules
│   │   ├── LarynxModule.hpp
│   │   ├── AspiratorModule.hpp
│   │   ├── FormantModule.hpp
│   │   ├── FormantBankModule.hpp
│   │   └── EnvelopeModule.hpp
│   └── ui/
│       └── chatterbox_ui_x11.c # X11/Cairo UI
└── README.md                   # This file
```

## Credits

- **Port from:** `experiments/chatterbox` (JavaScript/AudioWorklet web app)
- **Reverb module:** Shared from `lv2/pm-synth`
- **UI pattern:** Based on `lv2/disyn` X11/Cairo implementation
- **License:** MIT
- **Project:** Flues - Physical modeling synthesizers

## Related Projects

- **Chatterbox Web App:** `experiments/chatterbox` - Browser-based version with IPA vowel joystick
- **PM Synth:** `lv2/pm-synth` - General-purpose physical modeling synth
- **Disyn:** `lv2/disyn` - Distortion synthesis plugin

## Known Limitations

- **Monophonic only** - No polyphony support (single voice)
- **No presets** - Manual parameter recall (DAW automation recommended)
- **No joystick UI** - Formants controlled separately (web app has IPA vowel canvas)
- **Fixed bandwidths** - Formant Q is preset per formant (F1=80Hz, F2=120Hz, F3=150Hz, F4=200Hz)

## Future Enhancements

Potential improvements:
- [ ] Polyphonic voice allocation (4-8 voices)
- [ ] IPA vowel quadrilateral joystick (X11/Cairo canvas)
- [ ] Preset system (LV2 state extension)
- [ ] MIDI CC mapping for formants and modes
- [ ] Breath controller support (CC 2)
- [ ] Glottal pulse shaping options
- [ ] Additional formant (F5) for high-frequency detail

## Troubleshooting

**Plugin doesn't appear in DAW:**
```bash
# Verify installation
ls ~/.lv2/chatterbox.lv2/
# Should show: chatterbox.so, chatterbox_ui.so, manifest.ttl, chatterbox.ttl

# Check LV2 cache
lv2ls | grep chatterbox
```

**No sound output:**
1. Check MIDI routing - plugin requires MIDI notes to trigger
2. Ensure "Voiced" or "Aspirated" is enabled
3. Check Master Gain is not at 0
4. Verify envelope Attack/Release are reasonable values

**UI doesn't appear:**
- Check host supports X11UI
- Verify X11 and Cairo libraries are installed
- Check terminal for error messages

**Crackling/distortion:**
- Lower Stress parameter
- Reduce Reverb Level
- Lower Master Gain
- Check formant frequencies aren't creating resonance buildup

## Support

Report issues at: https://github.com/anthropics/flues/issues

For questions about the Flues project, see `CLAUDE.md` in the repository root.
