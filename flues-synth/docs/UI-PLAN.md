# Flues-Synth Web UI Implementation Plan

## Overview

A lightweight web-based control interface for flues-synth on Raspberry Pi 4. Uses MIDI passthrough architecture to keep the audio engine completely isolated while providing full parameter control and real-time audio visualization.

## Architecture

```
Browser (Client)
  ↓ WebSocket (MIDI CCs)
  ↓ WebSocket (Audio Samples)
flues-web-server (C)
  ↓ ALSA MIDI (virtual port)
  ↓ ALSA Loopback (audio capture)
flues-synth (unmodified)
```

**Key Design Decision:** MIDI passthrough ensures zero changes to existing flues-synth code. UI and audio engine run as separate processes with complete isolation.

## Technology Stack

### Server (C)
- **HTTP:** libmicrohttpd (85KB, serves static files)
- **WebSocket:** libwebsockets (200KB, binary + text frames)
- **MIDI:** ALSA sequencer API (virtual port creation)
- **Audio:** ALSA PCM capture from hw:Loopback,1
- **FFT:** KissFFT (15KB, BSD license, single-file)
- **JSON:** cJSON (25KB, MIT license)
- **Total:** ~500KB dependencies

### Client (JavaScript)
- **Build:** Vite (fast bundler, HMR for dev)
- **Framework:** Vanilla JS (no React/Vue, keep bundle <50KB)
- **UI Components:** Reuse from experiments/pm-synth, experiments/chatterbox
- **Visualizer:** Canvas 2D API (waveform + spectrum)
- **WebSocket:** Native browser API

## Directory Structure

```
flues-synth/
└── web-ui/                          # NEW
    ├── server/                      # C WebSocket/HTTP server
    │   ├── src/
    │   │   ├── main.c              # Entry point, event loop
    │   │   ├── websocket_handler.c  # WebSocket protocol
    │   │   ├── midi_bridge.c       # ALSA MIDI virtual port
    │   │   ├── audio_tap.c         # ALSA loopback + FFT
    │   │   └── utils.c             # JSON, logging
    │   ├── include/
    │   │   ├── websocket_handler.h
    │   │   ├── midi_bridge.h
    │   │   ├── audio_tap.h
    │   │   └── config.h
    │   ├── vendor/
    │   │   ├── kiss_fft.c          # FFT library (bundled)
    │   │   └── cJSON.c             # JSON parser (bundled)
    │   └── meson.build
    │
    ├── client/                      # JavaScript web UI
    │   ├── src/
    │   │   ├── main.js             # Entry point
    │   │   ├── api/
    │   │   │   ├── WebSocketClient.js
    │   │   │   └── MidiMapper.js
    │   │   └── ui/
    │   │       ├── KnobController.js      # From pm-synth
    │   │       ├── RotarySwitchController.js
    │   │       ├── VoiceMeters.js         # NEW
    │   │       └── Visualizer.js          # Modified from pm-synth
    │   ├── index.html
    │   ├── style.css
    │   ├── vite.config.js
    │   └── package.json
    │
    ├── systemd/
    │   ├── flues-synth.service
    │   ├── flues-web-server.service
    │   └── README.md
    │
    ├── docs/
    │   ├── ARCHITECTURE.md
    │   ├── API.md                   # WebSocket protocol spec
    │   └── DEPLOYMENT.md            # Raspberry Pi setup
    │
    └── README.md
```

## Implementation Steps

### Phase 1: Server Core (Week 1)

**1.1 Project Setup**
- Create directory structure: `flues-synth/web-ui/server/`
- Copy vendor libraries: KissFFT, cJSON
- Create `meson.build` with dependencies:
  - libmicrohttpd
  - libwebsockets
  - libasound2
  - pthreads
  - libm

**1.2 MIDI Bridge (`server/src/midi_bridge.c`)**
- Create ALSA sequencer virtual port "flues-web-midi"
- Implement `midi_bridge_send_cc(cc, value)` → ALSA event
- Thread-safe queue for WebSocket → MIDI
- Test: `aconnect -l` shows port, `aseqdump` shows CC events

**1.3 Audio Tap (`server/src/audio_tap.c`)**
- Open ALSA PCM capture: `hw:Loopback,1` at 48kHz, 512 samples
- Implement FFT: 512 samples → zero-pad to 1024 → magnitude spectrum
- Convert 1024 bins → 40 bars (logarithmic scale, 20Hz-20kHz)
- Compute RMS, peak, peak-to-peak
- Double-buffering for thread safety
- Test: Print RMS/peak to console

**1.4 HTTP Server (`server/src/main.c`)**
- Initialize libmicrohttpd on port 8080
- Serve static files from `client/dist/`
- Test: `curl http://localhost:8080/index.html`

### Phase 2: WebSocket Integration (Week 2)

**2.1 WebSocket Handler (`server/src/websocket_handler.c`)**
- Initialize libwebsockets on port 8081
- Implement callback for LWS_CALLBACK_RECEIVE:
  - Parse JSON: `{ "type": "cc", "cc": 71, "value": 63 }`
  - Call `midi_bridge_send_cc()`
- Implement callback for LWS_CALLBACK_SERVER_WRITEABLE:
  - Get audio data from `audio_tap_get_data()`
  - Encode binary frame: timeDomain(512*4) + freqDomain(40) + stats(12)
  - Send to all connected clients
- Test: wscat tool for manual WebSocket testing

**2.2 Main Event Loop (`server/src/main.c`)**
- Combine HTTP + WebSocket in single event loop
- Call `lws_service(ws, 16)` for 60Hz updates
- Handle SIGINT for clean shutdown

**2.3 Integration Test**
- Start server, connect mock WebSocket client
- Send CC message, verify ALSA output with `aseqdump`
- Verify audio frames received at 60Hz

### Phase 3: Client UI (Week 3)

**3.1 Project Setup**
- Create `client/` directory with Vite
- Copy UI components from experiments:
  - `pm-synth/src/ui/KnobController.js`
  - `pm-synth/src/ui/RotarySwitchController.js`
  - `pm-synth/src/ui/Visualizer.js` (modify for WebSocket)
  - Base CSS from `pm-synth/index.html`

**3.2 WebSocket Client (`client/src/api/WebSocketClient.js`)**
- Connect to `ws://raspberrypi.local:8081`
- Auto-reconnect on disconnect (2 second retry)
- Send MIDI CC: `sendCC(cc, value)` → JSON message
- Parse binary audio frames: timeDomain, freqDomain, stats
- Emit events: `onAudio(callback)`, `onConnect()`, `onDisconnect()`

**3.3 Parameter Controls (`client/src/main.js`)**
- Instantiate 29 controls (8 parameter groups):
  1. **Disyn Source (6):** Algorithm (rotary switch), Param1/2, Level, Noise, DC
  2. **Formants (8):** F1-F4 (knobs), Nasal/Sing/Shout/Fry (toggles)
  3. **Envelope (2):** Attack, Release
  4. **Interface (4):** Type (rotary switch), Intensity, Tuning, Ratio
  5. **Feedback (3):** Delay1, Delay2, Filter
  6. **Filter (3):** Frequency, Q, Shape
  7. **Modulation (2):** LFO Freq, AM/FM Depth
  8. **Output (1):** Master Gain
- Wire each control to WebSocket: `onValueChange = (v) => ws.sendCC(cc, v * 127)`
- Apply parameter mapping (exponential for formants/filter, linear for others)

**3.4 Visualizer (`client/src/ui/Visualizer.js`)**
- Adapt from pm-synth:
  - Remove Web Audio API AnalyserNode dependency
  - Accept external audio data from WebSocket
  - Keep waveform rendering (green line, 0-512 samples)
  - Keep spectrum rendering (40 blue bars, 20Hz-20kHz)
  - Add RMS/peak display (text overlay)

**3.5 Voice Meters (`client/src/ui/VoiceMeters.js`)**
- NEW component: 4 circular indicators
- Light up based on `audioData.voiceCount` (0-4)
- CSS: green glow when active, gray when idle

**3.6 Layout (`client/index.html`)**
- Header: Title + connection status indicator
- Visualizer section: Canvas (1200×200px) + voice meters (4 indicators)
- Control grid: 8 sections in 2×4 layout (responsive: 1 column on mobile)
- Footer: Version info

### Phase 4: Testing & Deployment (Week 4)

**4.1 ALSA Configuration**
- Setup ALSA loopback:
  ```bash
  sudo modprobe snd-aloop
  echo "snd-aloop" | sudo tee -a /etc/modules-load.d/alsa-loopback.conf
  ```
- Configure multi plugin (`/etc/asound.conf`):
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

**4.2 Systemd Services**
- Create `flues-synth.service`: Start synth on boot
- Create `flues-web-server.service`: Start web server after synth
- Enable: `sudo systemctl enable flues-synth flues-web-server`
- Test: Reboot Pi, verify services auto-start

**4.3 Build Scripts**
- Server: `cd server && meson setup builddir && ninja -C builddir`
- Client: `cd client && npm install && npm run build`
- Deploy: Copy `client/dist/*` → `server/builddir/static/`

**4.4 End-to-End Testing**
- Start services on Pi
- Connect from laptop browser: `http://raspberrypi.local:8080`
- Verify:
  - Connection status shows "Connected"
  - Waveform oscillates when MIDI played
  - Spectrum shows frequency content
  - Voice meters light up (1-4 active)
  - Adjust Master Gain → volume changes
  - All 29 parameters control synth

**4.5 Performance Profiling**
- Measure CPU usage: `htop` (target: <50% total)
- Measure latency: Parameter change → audio response (<100ms)
- Measure bandwidth: `iftop` (target: <2 Mbps)

## WebSocket Protocol Specification

### Client → Server (Text/JSON)

```json
// MIDI CC
{ "type": "cc", "cc": 71, "value": 63 }

// Note On/Off (future keyboard support)
{ "type": "note_on", "note": 60, "velocity": 96 }
{ "type": "note_off", "note": 60 }

// All Notes Off
{ "type": "all_notes_off" }
```

### Server → Client (Binary)

**Frame format (2100 bytes):**
- Bytes 0-2047: timeDomain (512 × float32, samples -1.0 to +1.0)
- Bytes 2048-2087: freqDomain (40 × uint8, bars 0-255)
- Bytes 2088-2091: rms (float32)
- Bytes 2092-2095: peak (float32)
- Bytes 2096-2099: voiceCount (uint32)

**Update rate:** 60 Hz (16.67 ms per frame)

## Parameter Mapping Reference

| CC  | Parameter       | Range           | Mapping      | Default |
|-----|-----------------|-----------------|--------------|---------|
| 1   | Intensity       | 0-1             | Linear       | 0.5     |
| 7   | Master Gain     | 0-1             | Linear       | 0.5     |
| 10  | F2 (Tongue)     | 500-3000 Hz     | Exponential  | 1500    |
| 16  | Disyn Algorithm | 0-6             | Discrete     | 0       |
| 17  | Disyn Param1    | 0-1             | Linear       | 0.5     |
| 18  | Disyn Param2    | 0-1             | Linear       | 0.5     |
| 19  | Disyn Level     | 0-1             | Linear       | 0.8     |
| 20  | Noise Level     | 0-1             | Linear       | 0.15    |
| 21  | DC Level        | 0-1             | Linear       | 0.0     |
| 24  | Interface Type  | 0-11            | Discrete     | 2       |
| 26  | Tuning          | -12 to +12 st   | Linear       | 0       |
| 27  | Ratio           | 0.5-2.0         | Exponential  | 1.0     |
| 28  | Delay1 Feedback | 0-1             | Linear       | 0.2     |
| 29  | Delay2 Feedback | 0-1             | Linear       | 0.2     |
| 30  | Filter Feedback | 0-1             | Linear       | 0.1     |
| 32  | Filter Freq     | 20-20000 Hz     | Exponential  | 2000    |
| 33  | Filter Q        | 0.1-10          | Exponential  | 1.0     |
| 34  | Filter Shape    | 0-1 (LP→BP→HP)  | Linear       | 0.0     |
| 36  | LFO Freq        | 0.1-20 Hz       | Exponential  | 5.0     |
| 37  | AM/FM Depth     | -1 to +1        | Bipolar      | 0.0     |
| 71  | F1 (Jaw)        | 200-1000 Hz     | Exponential  | 500     |
| 72  | Release         | 0-1             | Exponential  | 0.5     |
| 73  | Attack          | 0-1             | Exponential  | 0.1     |
| 74  | F3 (Lips)       | 1500-4000 Hz    | Exponential  | 2500    |
| 75  | F4 (Quality)    | 2500-4500 Hz    | Exponential  | 3500    |
| 80  | Nasal           | Toggle (≥64)    | Boolean      | false   |
| 81  | Sing            | Toggle (≥64)    | Boolean      | false   |
| 82  | Shout           | Toggle (≥64)    | Boolean      | false   |
| 83  | Fry             | Toggle (≥64)    | Boolean      | false   |

**Mapping Functions (JavaScript):**
```javascript
// Exponential: exp_map(value, min, max) = min * (max/min)^value
const expMap = (v, min, max) => min * Math.pow(max / min, v);

// Linear: lerp(value, min, max) = min + value * (max - min)
const lerp = (v, min, max) => min + v * (max - min);

// Bipolar: (value - 0.5) * 2 → -1 to +1
const bipolar = (v) => (v - 0.5) * 2;

// Discrete: Math.floor(value * (max + 0.999))
const discrete = (v, max) => Math.floor(v * (max + 0.999));
```

## Performance Targets

| Metric | Target | Measured |
|--------|--------|----------|
| CPU (synth) | <25% | TBD |
| CPU (server) | <15% | TBD |
| CPU (browser) | <10% | TBD |
| Total CPU | <50% | TBD |
| Control latency | <100ms | ~55ms (estimated) |
| Audio visualization rate | 60 Hz | 60 Hz (target) |
| Network bandwidth | <2 Mbps | ~1 Mbps (estimated) |
| Memory (server) | <50 MB | TBD |
| Memory (browser) | <100 MB | TBD |

## Critical Files

### Server (C) - 5 files, ~1200 lines
1. `web-ui/server/src/main.c` (300 lines) - Entry point, HTTP/WS server
2. `web-ui/server/src/midi_bridge.c` (200 lines) - ALSA MIDI virtual port
3. `web-ui/server/src/audio_tap.c` (250 lines) - ALSA loopback + FFT
4. `web-ui/server/src/websocket_handler.c` (400 lines) - WebSocket protocol
5. `web-ui/server/meson.build` (50 lines) - Build config

### Client (JavaScript) - 5 files, ~1330 lines
1. `web-ui/client/src/api/WebSocketClient.js` (150 lines) - WebSocket client
2. `web-ui/client/src/main.js` (400 lines) - UI controller wiring
3. `web-ui/client/src/ui/Visualizer.js` (180 lines) - Waveform + spectrum
4. `web-ui/client/index.html` (600 lines) - UI layout
5. `web-ui/client/style.css` (400 lines) - Styling

### Reused from experiments/
- `pm-synth/src/ui/KnobController.js` → Copy to client/src/ui/
- `pm-synth/src/ui/RotarySwitchController.js` → Copy to client/src/ui/
- `pm-synth/src/ui/Visualizer.js` → Adapt for WebSocket

**Total new code:** ~2530 lines (excluding vendor libraries and reused components)

## Deployment Checklist

- [ ] Install dependencies: `sudo apt install libmicrohttpd-dev libwebsockets-dev libasound2-dev`
- [ ] Load ALSA loopback: `sudo modprobe snd-aloop`, add to `/etc/modules-load.d/`
- [ ] Configure ALSA multi plugin in `/etc/asound.conf`
- [ ] Build server: `cd web-ui/server && meson setup builddir && ninja -C builddir`
- [ ] Build client: `cd web-ui/client && npm install && npm run build`
- [ ] Copy static files: `cp -r client/dist/* server/builddir/static/`
- [ ] Copy systemd services: `sudo cp systemd/*.service /etc/systemd/system/`
- [ ] Enable services: `sudo systemctl enable flues-synth flues-web-server`
- [ ] Start services: `sudo systemctl start flues-synth flues-web-server`
- [ ] Verify: Open `http://raspberrypi.local:8080` in browser
- [ ] Test: Adjust parameters, verify MIDI control, check visualizations

## Testing Strategy

**Unit Tests:**
- Server: MIDI bridge CC output (mock ALSA)
- Server: Audio tap FFT computation (known waveform)
- Client: WebSocket reconnect logic (mock server)
- Client: Parameter mapping functions (exp_map, lerp, bipolar)

**Integration Tests:**
- Server: WebSocket → MIDI pipeline (verify with aseqdump)
- Server: ALSA loopback → WebSocket audio (verify frame rate)
- Client: Browser → WebSocket → MIDI → Synth (end-to-end control)

**Performance Tests:**
- CPU profiling: `perf stat -p $(pidof flues-web-server)`
- Latency measurement: Oscilloscope on control input → audio output
- Load test: 100 parameter changes/second (stress test)

**Smoke Test:**
1. Reboot Pi
2. Wait 30 seconds (services auto-start)
3. Open browser: `http://raspberrypi.local:8080`
4. Check connection status: "Connected" (green)
5. Play MIDI keyboard → voice meters light up
6. Adjust Master Gain → volume changes
7. Check waveform oscillates, spectrum shows bands

## Future Enhancements (Post-MVP)

1. **Preset Management** - Save/load parameter states
2. **Multi-Client Support** - Broadcast state changes to all connected browsers
3. **MIDI Learn** - Right-click parameter → learn CC mapping
4. **Advanced Visualizations** - Spectrogram, phase scope, per-voice envelopes
5. **PWA Support** - Install as mobile app (manifest.json, service worker)
6. **Touch Gestures** - Multi-touch XY pad for formants, swipe to change preset

## Notes

- **No modifications to flues-synth:** Audio engine remains headless and unmodified
- **Lightweight:** Total server binary <1MB, client bundle <50KB gzipped
- **Low latency:** 55ms control latency (parameter change → audio response)
- **Raspberry Pi optimized:** ALSA loopback, systemd auto-start, WiFi/Ethernet support
- **Code reuse:** Maximum reuse from existing web experiments (pm-synth, chatterbox)
- **Production ready:** Systemd services, auto-reconnect, error handling

## References

- **Existing Synth:** `/home/danny/github/flues/flues-synth/`
- **Handover Doc:** `/home/danny/github/flues/flues-synth/docs/flues-synth-handover.md`
- **Web Experiments:** `/home/danny/github/flues/experiments/pm-synth/`, `clarinet-synth/`, `chatterbox/`
- **MIDI Mapping:** `/home/danny/github/flues/flues-synth/src/main.c` (lines 150-300)
- **Visualizer Pattern:** `/home/danny/github/flues/experiments/pm-synth/src/ui/Visualizer.js`

---

**Implementation Timeline:** 4 weeks (one developer, full-time)
**Estimated New Code:** ~2530 lines (server: 1200, client: 1330)
**Dependencies:** ~500KB (libmicrohttpd, libwebsockets, KissFFT, cJSON)
**Raspberry Pi CPU Target:** <50% total (synth 25% + server 15% + browser 10%)
