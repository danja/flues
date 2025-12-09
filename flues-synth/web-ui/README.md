# Flues-Synth Web UI

**Browser-based control interface for flues-synth on Raspberry Pi**

A lightweight web UI that provides full parameter control (29 MIDI CCs) and real-time audio visualization (waveform + spectrum analyzer) without modifying the core flues-synth engine.

## Architecture

```
Browser (HTML/JS)
    ↕ WebSocket (MIDI CCs + Audio)
Node.js Server (flues-web-server)
    ↕ ALSA MIDI virtual port
    ↕ ALSA Loopback audio capture
flues-synth (unchanged)
```

**Key features:**
- **MIDI Passthrough**: UI sends MIDI CCs via WebSocket → ALSA virtual port → flues-synth
- **Audio Visualization**: Captures audio from ALSA loopback → FFT → WebSocket → browser
- **Zero Modifications**: flues-synth runs headless and unmodified
- **Lightweight**: Node.js server (~30MB RAM), vanilla JS client (<50KB)
- **On-screen keyboard**: C4–C5 buttons send Note On/Off for quick smoke tests (velocity 96)

## Quick Start

### 1. Install Dependencies

```bash
# Server dependencies
cd server
npm install

# Client dependencies
cd ../client
npm install
```

### 2. Setup ALSA Loopback

```bash
# Load loopback kernel module
sudo modprobe snd-aloop
echo "snd-aloop" | sudo tee -a /etc/modules-load.d/alsa-loopback.conf

# Configure ALSA to output to both headphones and loopback
# Edit /etc/asound.conf or ~/.asoundrc:
sudo nano /etc/asound.conf
```

Add:
```
pcm.!default {
    type asym
    playback.pcm "multi_out"
}

pcm.multi_out {
    type multi
    slaves {
        a { pcm "hw:Headphones" channels 2 }
        b { pcm "hw:Loopback,0,0" channels 2 }
    }
    bindings {
        0 { slave a channel 0 }
        1 { slave a channel 1 }
        2 { slave b channel 0 }
        3 { slave b channel 1 }
    }
}
```

### 3. Build Client

```bash
cd client
npm run build
```

### 4. Run Servers

**Terminal 1 - Start flues-synth:**
```bash
cd flues-synth
./builddir/flues-synth
```

**Terminal 2 - Start web server:**
```bash
cd flues-synth/web-ui/server
npm start
```

### 5. Open Browser

Open `http://localhost:8080` (or `http://raspberrypi.local:8080` from another device). Use the on-screen C4–C5 keyboard for a quick sound check if no MIDI controller is attached.

## Development

### Server (with auto-reload)
```bash
cd server
npm run dev
```

### Client (with hot-reload)
```bash
cd client
npm run dev  # Opens http://localhost:3000
```

## Project Structure

```
web-ui/
├── server/                   # Node.js WebSocket server
│   ├── index.js             # Main entry point
│   ├── midi-bridge.js       # ALSA MIDI output
│   ├── audio-tap.js         # ALSA loopback capture + FFT
│   └── package.json
│
├── client/                   # Vanilla JS + Vite
│   ├── src/
│   │   ├── main.js          # UI initialization
│   │   ├── api/
│   │   │   └── WebSocketClient.js
│   │   └── ui/
│   │       ├── KnobController.js
│   │       ├── RotarySwitchController.js
│   │       ├── Keyboard.js
│   │       ├── Visualizer.js
│   │       └── VoiceMeters.js
│   ├── index.html
│   ├── style.css
│   ├── vite.config.js
│   └── package.json
│
├── systemd/                  # Service files
│   ├── flues-synth.service
│   └── flues-web-server.service
│
└── docs/
    ├── UI-PLAN.md           # Implementation plan
    ├── API.md               # WebSocket protocol
    └── DEPLOYMENT.md        # Raspberry Pi setup
```

## WebSocket Protocol

### Client → Server (JSON)

```javascript
// MIDI CC
{ type: 'cc', cc: 71, value: 63 }

// Note On/Off
{ type: 'note_on', note: 60, velocity: 96 }
{ type: 'note_off', note: 60 }

// All Notes Off
{ type: 'all_notes_off' }
```

### Server → Client (Binary)

**Frame size: 2100 bytes**
- Bytes 0-2047: `timeDomain` (512 × float32, audio samples)
- Bytes 2048-2087: `freqDomain` (40 × uint8, FFT bars 0-255)
- Bytes 2088-2091: `rms` (float32, RMS level)
- Bytes 2092-2095: `peak` (float32, peak level)
- Bytes 2096-2099: `voiceCount` (uint32, active voices 0-4)

**Update rate:** 60 Hz (16.67 ms per frame)

## MIDI CC Mapping

All 29 parameters from flues-synth are accessible:

| CC  | Parameter       | Range           | Mapping      |
|-----|-----------------|-----------------|--------------|
| 1   | Intensity       | 0-1             | Linear       |
| 7   | Master Gain     | 0-1             | Linear       |
| 10  | F2 (Tongue)     | 500-3000 Hz     | Exponential  |
| 16  | Disyn Algorithm | 0-6             | Discrete     |
| 17-21 | Disyn Params  | Various         | See docs     |
| 24  | Interface Type  | 0-11            | Discrete     |
| 26-27 | Tuning/Ratio  | Various         | See docs     |
| 28-30 | Feedback      | 0-1             | Linear       |
| 32-34 | Filter        | Various         | See docs     |
| 36-37 | Modulation    | Various         | See docs     |
| 71-75 | Formants      | Frequency Hz    | Exponential  |
| 80-83 | Vocal Modes   | Toggle (≥64)    | Boolean      |

See `docs/UI-PLAN.md` for complete parameter mapping table.

## Systemd Services (Auto-start)

```bash
# Copy service files
sudo cp systemd/*.service /etc/systemd/system/

# Enable services
sudo systemctl enable flues-synth flues-web-server

# Start services
sudo systemctl start flues-synth flues-web-server

# Check status
systemctl status flues-synth
systemctl status flues-web-server
```

## Testing

```bash
# Server tests
cd server
npm test

# Client tests
cd client
npm test
```

## Troubleshooting

### No audio visualization

1. Check ALSA loopback is loaded: `lsmod | grep snd_aloop`
2. Verify loopback devices: `aplay -l | grep Loopback`
3. Test capture: `arecord -D hw:Loopback,1 -f FLOAT_LE -c 1 -r 48000 test.wav`

### MIDI not working

1. Check virtual port exists: `aconnect -l | grep flues-web-midi`
2. Manually connect: `aconnect <web-server-port> <flues-synth-port>`
3. Enable MIDI debug: `FLUES_MIDI_DEBUG=1 ./builddir/flues-synth`

### WebSocket connection fails

1. Check server is running: `curl http://localhost:8080`
2. Check WebSocket port: `netstat -an | grep 8081`
3. Check browser console for errors

## Performance

**Raspberry Pi 4 targets:**
- CPU: <50% total (flues-synth 25% + server 15% + browser 10%)
- Memory: Server ~30MB, browser ~100MB
- Latency: ~55ms (parameter change → audio response)
- Bandwidth: ~1 Mbps (60 Hz × 2.1 KB/frame)

## Credits

- **UI Components**: Adapted from `experiments/pm-synth` and `experiments/chatterbox`
- **FFT Library**: fft.js (https://github.com/indutny/fft.js)
- **MIDI Library**: easymidi (cross-platform, wraps ALSA on Linux)
- **Audio Capture**: arecord command (from alsa-utils)

## License

Part of the Flues project. See main repository for license information.
