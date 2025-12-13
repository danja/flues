# Flues Project - Release v0.1.0

Binary distribution for desktop (x86_64) + Raspberry Pi (aarch64).

## Contents

- **lv2-plugins/** - Seven LV2 instrument plugins (built on x86_64)
  - disyn.lv2 - Distortion synthesis (7 algorithms)
  - floozy.lv2 - Hybrid distortion + physical modeling
  - chatterbox.lv2 - Formant speech synthesizer
  - chatgen.lv2 - Text-to-speech MIDI generator
  - drumkit.lv2 - Industrial drum synthesizer (8 voices)
  - pm-synth.lv2 - Physical modeling synthesizer
  - flues-control.lv2 - MIDI CC controller with 29 programs

- **flues-synth/** - Headless Raspberry Pi synthesizer binary (built on aarch64)
  - Optimized for Raspberry Pi 4 (Cortex-A72)
  - 29 MIDI programs with dynamic slider remapping
  - ALSA MIDI/audio backends

- **docs/** - Complete documentation

## Installation

### Desktop (LV2 Plugins Only)

```bash
# Install plugins
sudo cp -r lv2-plugins/* /usr/local/lib/lv2/

# Or user install
cp -r lv2-plugins/* ~/.lv2/
```

### Raspberry Pi (Flues-Synth Only)

```bash
# Install synth
sudo cp flues-synth/flues-synth /usr/local/bin/
sudo chmod +x /usr/local/bin/flues-synth

# Run
flues-synth
```

### Full Install (Both)

```bash
sudo ./install.sh
```

## Usage

### LV2 Plugins (Desktop)

Load in any LV2 host (Ardour, Reaper, Carla):

```bash
lv2ls | grep flues
jalv.gtk https://danja.github.io/flues/plugins/disyn
```

### Flues-Synth (Raspberry Pi)

```bash
flues-synth              # Auto-detect audio device
flues-synth hw:2,0       # Specify device
FLUES_MIDI_DEBUG=1 flues-synth  # Enable debug
```

## Build Information

- Version: 0.1.0
- Desktop arch: x86_64
- Raspberry Pi arch: aarch64
- Build type: Release (O3 optimization)
- Built: 2025-12-13 09:25:36 UTC

## Documentation

See `docs/` directory for complete reference.

## Uninstall

```bash
sudo ./uninstall.sh
```
