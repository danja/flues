# Flues Drumkit - Hardcore Industrial TR-909

A hardcore industrial drum synthesizer LV2 plugin with 8 voices inspired by the TR-909 but pushed into aggressive territory. Designed for industrial, EBM, and harsh electronic music.

## Features

- **8 Synthesized Drum Voices** (not samples!)
- **18 Parameters** with X11/Cairo GUI
- **Velocity Sensitivity** on kick, snare, and toms
- **Hi-Hat Choke Group** (closed kills open)
- **Master FX Chain**: Bit Crusher, Distortion, Reverb
- **MIDI Channel Omni** (responds to all channels)

## MIDI Note Mapping

| MIDI Note | Note Name | Drum Voice | Velocity Sensitive |
|-----------|-----------|------------|-------------------|
| 36 | C2 | Kick | Yes |
| 38 | D2 | Snare | Yes |
| 39 | Eb2 | Clap | No (fixed level) |
| 42 | F#2 | Closed Hi-Hat | No (fixed level) |
| 45 | A2 | Lo Tom | Yes |
| 46 | A#2 | Open Hi-Hat | No (fixed level) |
| 49 | C#3 | Crash | No (fixed level) |
| 50 | D3 | Hi Tom | Yes |

**Note**: When Closed Hi-Hat (42) is triggered, it immediately silences Open Hi-Hat (46) for realistic behavior.

## Synthesis Methods

### Kick Drum (4 Parameters)
- **Pitch**: Starting frequency 60-250 Hz (exponential pitch envelope sweep)
- **Decay**: Envelope decay time 50-1500ms
- **Drive**: Tanh saturation 1.0-10× (adds harmonics)
- **Punch**: High-frequency click enhancement 0-100%

**Synthesis**: Pitch envelope → Sine oscillator → Distortion → HPF noise burst → DC blocker

### Snare Drum (2 Parameters)
- **Tone**: Body/shell resonator mix + Q factor (4-20) - 0=body, 1=shell
- **Snap**: Noise level + HPF cutoff 500Hz-4kHz

**Synthesis**: Dual bandpass resonators (180Hz + 330Hz) + filtered noise burst

### Clap (2 Parameters)
- **Density**: Impulse count (3-7) + spacing tightness (30-10ms)
- **Tone**: Bandpass center frequency 800Hz-3.5kHz

**Synthesis**: Multi-impulse burst → Bandpass filter → Short reverb

### Toms (2 Parameters, Shared)
- **Pitch**: Base frequency (Lo: 60-150Hz, Hi: 150-400Hz)
- **Decay**: Envelope decay time 80-800ms

**Synthesis**: Pitch envelope → Triangle wave → Resonant bandpass (Q=20)

### Hi-Hats (2 Parameters, Shared)
- **Brightness**: HPF cutoff 4kHz-12kHz (exponential)
- **Decay**: Envelope decay (Closed: 50-200ms, Open: 200-1200ms)

**Synthesis**: 6× inharmonic square wave oscillators → Ring modulation → Noise mix → HPF

### Crash (2 Parameters)
- **Brightness**: Bandpass center shift 1.5kHz-10kHz
- **Decay**: Envelope decay 300-2500ms

**Synthesis**: White noise → 3× bandpass cascade → Soft clipping

### Master FX (4 Parameters)
- **Bit Crush**: Bit depth reduction 0=off, 1=4-bit (lo-fi grunge)
- **Drive**: Master distortion 1.0-5.0× tanh saturation
- **Reverb**: Schroeder reverb level 0-60% (fixed room size)
- **Gain**: Master output level 0-100%

## Signal Flow

```
MIDI Note → Voice Synthesis → Voice Mixer
    ↓
Bit Crusher (global)
    ↓
Master Distortion (tanh)
    ↓
Reverb (Schroeder, shared)
    ↓
DC Blocker (R=0.999)
    ↓
Master Gain
    ↓
Audio Out
```

## Installation

```bash
cd lv2/drumkit
cmake -S . -B build
cmake --build build
cmake --install build --prefix ~/.lv2
```

Verify installation:
```bash
lv2ls | grep drumkit
lv2info https://danja.github.io/flues/plugins/drumkit
```

## Usage

### In a DAW (Reaper, Ardour, Carla, etc.)

1. Load "Flues Drumkit" as an instrument track
2. Send MIDI notes in the range C2-D3 (notes 36-50)
3. Adjust parameters via the GUI or DAW automation
4. **Tip**: Use MIDI velocity on kick/snare/toms for dynamics

### Sound Design Tips

**Aggressive Kick**:
- Pitch: 0.2-0.4 (60-100 Hz starting point)
- Decay: 0.3-0.5 (medium decay)
- Drive: 0.5-0.8 (heavy distortion)
- Punch: 0.3-0.5 (add click attack)

**Metallic Snare**:
- Tone: 0.7-0.9 (emphasize shell resonator, high Q)
- Snap: 0.6-0.8 (bright, cutting snap)

**Dense Clap**:
- Density: 0.7-0.9 (7 impulses, tight spacing)
- Tone: 0.3-0.5 (telephone-like bandpass)

**Harsh Hi-Hats**:
- Brightness: 0.7-1.0 (8-12kHz, very bright)
- Decay: Adjust per pattern (short for techno, long for ambient)

**Master Processing**:
- Bit Crush: 0.2-0.4 for subtle grit, 0.7+ for lo-fi destruction
- Drive: 0.3-0.5 for cohesive saturation
- Reverb: 0.1-0.3 for industrial ambience

### Performance Tips

1. **Hi-Hat Choke**: Play closed HH to cut off open HH naturally
2. **Velocity Expression**: Use MIDI velocity on kick/snare/toms for dynamics
3. **Layering**: Combine multiple voices (e.g., kick + clap) for hybrid sounds
4. **Automation**: Automate Drive and Bit Crush for evolving textures

## Technical Details

- **Sample Rate**: 48kHz (native)
- **Latency**: Zero (real-time synthesis)
- **Polyphony**: 8 voices (one per drum, monophonic)
- **CPU Usage**: ~5-8% (single core, all voices active)
- **Output Level**: Calibrated to ~-6dB peak with all drums

## Architecture

**DSP Modules** (7 total):
- NoiseGenerator.hpp - LCG white noise
- ADEnvelope.hpp - Attack/decay envelope
- PitchEnvelope.hpp - Exponential pitch sweep
- BiquadFilter.hpp - Bandpass/highpass filtering
- Distortion.hpp - Tanh soft clipping
- Bitcrusher.hpp - Bit depth reduction
- DCBlocker.hpp - DC offset removal

**Drum Voices** (6 types, 8 instances):
- KickVoice.hpp - 4 parameters
- SnareVoice.hpp - 2 parameters
- ClapVoice.hpp - 2 parameters
- TomVoice.hpp - 2 parameters (shared between lo/hi)
- HiHatVoice.hpp - 2 parameters (shared between closed/open)
- CrashVoice.hpp - 2 parameters

**UI**: X11/Cairo with 18 rotary knobs organized in 4 rows by function

## References

- Inspired by Roland TR-909 (1984)
- Industrial sound design from NIN, Ministry, Front 242
- Physical modeling techniques from Karplus-Strong and resonant synthesis

## License

MIT License - Part of the Flues synthesis project

## Author

Danny Ayers - https://danja.github.io/flues/
