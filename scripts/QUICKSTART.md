# Flues Project Quick Start Guide

This guide will get you up and running with Flues LV2 plugins and flues-synth in 5 minutes.

## Prerequisites

### Ubuntu/Debian
```bash
sudo apt install libasound2 libx11-6 libcairo2
```

### Arch Linux
```bash
sudo pacman -S alsa-lib libx11 cairo
```

### Fedora
```bash
sudo dnf install alsa-lib libX11 cairo
```

## Installation

### Option 1: Quick Install (Recommended)

```bash
# Download release
wget https://github.com/danja/flues/releases/download/v0.1.0/flues-v0.1.0-linux-x86_64.tar.gz

# Extract
tar xzf flues-v0.1.0-linux-x86_64.tar.gz
cd flues-v0.1.0

# Install (requires root)
sudo ./install.sh
```

### Option 2: User Install (No Root)

```bash
# Extract
tar xzf flues-v0.1.0-linux-x86_64.tar.gz
cd flues-v0.1.0

# Install LV2 plugins to user directory
cp -r lv2-plugins/* ~/.lv2/

# Install flues-synth to user bin
mkdir -p ~/bin
cp flues-synth/flues-synth ~/bin/
chmod +x ~/bin/flues-synth

# Add ~/bin to PATH if not already
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

## Verify Installation

### LV2 Plugins
```bash
# List installed plugins
lv2ls | grep flues

# Expected output:
# https://danja.github.io/flues/plugins/chatterbox
# https://danja.github.io/flues/plugins/chatgen
# https://danja.github.io/flues/plugins/disyn
# https://danja.github.io/flues/plugins/drumkit
# https://danja.github.io/flues/plugins/floozy
# https://danja.github.io/flues/plugins/flues-control
# https://danja.github.io/flues/plugins/pm-synth
```

### Flues-Synth
```bash
flues-synth --help || echo "flues-synth installed at: $(which flues-synth)"
```

## Usage Examples

### Example 1: Test Disyn Plugin (Standalone)

```bash
# Requires jalv
sudo apt install jalv  # Ubuntu/Debian

# Launch Disyn with GUI
jalv.gtk https://danja.github.io/flues/plugins/disyn
```

**Play with it:**
1. Click "Algorithm" knob, select different algorithms (0-6)
2. Adjust Param1/Param2 to shape the sound
3. Play notes on your MIDI keyboard (or click virtual keyboard)
4. Adjust Attack/Release envelopes
5. Add reverb with Size/Level knobs

### Example 2: Text-to-Speech (Chatterbox + ChatGen)

**In Ardour/Reaper/Carla:**

1. Create MIDI track
2. Add instruments in order:
   - Insert: ChatGen (MIDI generator)
   - Insert: Chatterbox (speech synth)
3. Open ChatGen UI:
   - Type: "hello world"
   - Press Enter
4. Press Play in DAW
5. Hear synthesized speech!

**Standalone test:**
```bash
# Launch ChatGen
jalv.gtk https://danja.github.io/flues/plugins/chatgen &

# Launch Chatterbox
jalv.gtk https://danja.github.io/flues/plugins/chatterbox &

# Route MIDI: ChatGen → Chatterbox
# (Use qjackctl or similar MIDI patchbay)
```

### Example 3: Drumkit Plugin

```bash
jalv.gtk https://danja.github.io/flues/plugins/drumkit
```

**MIDI Note Mapping:**
- 36 (C2) - Kick
- 38 (D2) - Snare
- 39 (Eb2) - Clap
- 42 (F#2) - Closed Hi-Hat
- 45 (A2) - Lo Tom
- 46 (A#2) - Open Hi-Hat
- 49 (C#3) - Crash
- 50 (D3) - Hi Tom

**Tips:**
- Kick/Snare/Toms are velocity sensitive
- Closed Hi-Hat chokes Open Hi-Hat
- Use Bit Crush/Drive for industrial sound
- General MIDI drum maps work

### Example 4: Flues-Synth (Raspberry Pi or Desktop)

```bash
# Auto-detect audio device
flues-synth

# Or specify device
flues-synth hw:2,0

# Enable MIDI debug
FLUES_MIDI_DEBUG=1 flues-synth
```

**MIDI Control:**
- **Program Change 0-28**: Select synthesis program
  - 0: Disyn Echo
  - 3: Formant Voice
  - 6: Full Hybrid (all modules)
  - 8: ModFM Formant
  - 18: Hybrid Formant Engine (Algorithm 7)
- **Hardware Sliders**: CC 73, 72, 28, 30, 74, 71, 1, 27, 7
  - Dynamically remap to different parameters per program
- **Keyboard**: Notes 60-84 (C4-C6)

### Example 5: Flues-Control (DAW Controller)

**Setup in Reaper/Ardour:**

1. Create MIDI track with flues-synth running on Raspberry Pi
2. Add flues-control plugin to track
3. Configure MIDI output to Pi
4. Use flues-control UI:
   - Program selector: Choose synthesis program (0-28)
   - 9 sliders: Auto-mapped to relevant parameters
   - Check DAW manual for routing MIDI plugin → external synth

## Recommended DAW Workflow

### Ardour
```
Track 1: [Flues-Control] → MIDI Out to flues-synth
Track 2: Audio In from flues-synth → Record
```

### Reaper
```
Track 1: [Flues-Control] → MIDI Out to hardware
Track 2: Audio In → Record Enable
```

### Carla (Standalone)
```
Add flues-control → Route MIDI → Add Disyn/Floozy/Chatterbox → Audio Out
```

## Troubleshooting

### "Plugin not found" in DAW

```bash
# Check LV2_PATH
echo $LV2_PATH

# Manually set if needed
export LV2_PATH=/usr/local/lib/lv2:~/.lv2
```

### Flues-Synth: "Failed to create audio backend"

```bash
# List audio devices
aplay -l

# Use specific device
flues-synth hw:0,0

# Check ALSA
aplay /usr/share/sounds/alsa/Front_Center.wav
```

### No MIDI input in flues-synth

```bash
# List MIDI devices
aconnect -l

# Auto-connect should work, but manual connect:
aconnect [your-controller-port] [flues-synth-port]
```

### Plugin UI doesn't show

```bash
# Check X11 display
echo $DISPLAY

# If running remote/headless
export DISPLAY=:0

# Or use SSH X forwarding
ssh -X user@host
```

## Next Steps

- **Read the docs**: `docs/` directory has complete reference
  - `algorithms.md` - All 17 distortion algorithms
  - `midi.md` - MIDI CC mapping
  - `PROGRAM_CHANGE.md` - 29 program descriptions
- **Join the community**: https://github.com/danja/flues/discussions
- **Report issues**: https://github.com/danja/flues/issues
- **Sound design tips**: See plugin README files in `docs/`

## Uninstall

```bash
sudo ./uninstall.sh
```

Or manually:
```bash
rm -rf ~/.lv2/disyn.lv2 ~/.lv2/floozy.lv2 ~/.lv2/chatterbox.lv2 \
       ~/.lv2/chatgen.lv2 ~/.lv2/drumkit.lv2 ~/.lv2/pm-synth.lv2 \
       ~/.lv2/flues-control.lv2
rm ~/bin/flues-synth
```

## Support

- Documentation: https://danja.github.io/flues/
- Issues: https://github.com/danja/flues/issues
- Discussions: https://github.com/danja/flues/discussions

Happy synthesizing! 🎹🎛️🔊
