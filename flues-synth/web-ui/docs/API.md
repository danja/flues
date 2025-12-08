# Flues Web UI API Documentation

## WebSocket Protocol

The Flues Web UI uses WebSocket for bidirectional communication between the browser and Node.js server.

### Connection

**Client connects to:**
```
ws://localhost:8081
```

Or from another device:
```
ws://<raspberry-pi-ip>:8081
```

**Binary Type:**
```javascript
ws.binaryType = 'arraybuffer';
```

---

## Client → Server Messages (JSON)

All control messages from the client are sent as JSON strings.

### MIDI CC

Send MIDI Continuous Controller message.

**Format:**
```json
{
  "type": "cc",
  "cc": 71,
  "value": 63
}
```

**Fields:**
- `type` (string): `"cc"`
- `cc` (number): CC number (0-127)
- `value` (number): CC value (0-127)

**Example:**
```javascript
ws.send(JSON.stringify({
  type: 'cc',
  cc: 7,    // Master Gain
  value: 100
}));
```

### MIDI Note On

Send MIDI Note On message.

**Format:**
```json
{
  "type": "note_on",
  "note": 60,
  "velocity": 96
}
```

**Fields:**
- `type` (string): `"note_on"`
- `note` (number): MIDI note number (0-127, 60 = C4)
- `velocity` (number): Note velocity (0-127)

**Example:**
```javascript
ws.send(JSON.stringify({
  type: 'note_on',
  note: 60,      // C4
  velocity: 96
}));
```

### MIDI Note Off

Send MIDI Note Off message.

**Format:**
```json
{
  "type": "note_off",
  "note": 60
}
```

**Fields:**
- `type` (string): `"note_off"`
- `note` (number): MIDI note number (0-127)

**Example:**
```javascript
ws.send(JSON.stringify({
  type: 'note_off',
  note: 60
}));
```

### All Notes Off

Send All Notes Off (CC 123).

**Format:**
```json
{
  "type": "all_notes_off"
}
```

**Example:**
```javascript
ws.send(JSON.stringify({
  type: 'all_notes_off'
}));
```

---

## Server → Client Messages (Binary)

Audio visualization data is sent as binary frames at 60 Hz (16.67 ms intervals).

### Audio Frame Format

**Total Size:** 2100 bytes

**Layout:**

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 2048 | float32[512] | timeDomain | Time-domain audio samples (-1.0 to +1.0) |
| 2048 | 40 | uint8[40] | freqDomain | Frequency bins (0-255, logarithmic 20Hz-20kHz) |
| 2088 | 4 | float32 | rms | RMS level |
| 2092 | 4 | float32 | peak | Peak level |
| 2096 | 4 | uint32 | voiceCount | Active voice count (0-4) |

**Endianness:** Little-endian

### Parsing Audio Frames

```javascript
ws.onmessage = (event) => {
  if (event.data instanceof ArrayBuffer) {
    const buffer = event.data;
    const view = new DataView(buffer);

    // Parse time-domain samples (512 × float32)
    const timeDomain = new Float32Array(512);
    for (let i = 0; i < 512; i++) {
      timeDomain[i] = view.getFloat32(i * 4, true);  // little-endian
    }

    // Parse frequency bins (40 × uint8)
    const freqDomain = new Uint8Array(40);
    for (let i = 0; i < 40; i++) {
      freqDomain[i] = view.getUint8(2048 + i);
    }

    // Parse stats
    const rms = view.getFloat32(2088, true);
    const peak = view.getFloat32(2092, true);
    const voiceCount = view.getUint32(2096, true);

    // Use the data
    updateVisualizer({ timeDomain, freqDomain, rms, peak, voiceCount });
  }
};
```

---

## MIDI CC Mapping

Complete mapping of all 29 flues-synth parameters to MIDI CC numbers.

### Standard Controls

| CC | Parameter | Range | Mapping | Default | Unit |
|----|-----------|-------|---------|---------|------|
| 1 | Intensity | 0-1 | Linear | 0.5 | - |
| 7 | Master Gain | 0-1 | Linear | 0.5 | - |

### Formants

| CC | Parameter | Range | Mapping | Default | Unit |
|----|-----------|-------|---------|---------|------|
| 71 | F1 (Jaw) | 200-1000 | Exponential | 500 | Hz |
| 10 | F2 (Tongue) | 500-3000 | Exponential | 1500 | Hz |
| 74 | F3 (Lips) | 1500-4000 | Exponential | 2500 | Hz |
| 75 | F4 (Quality) | 2500-4500 | Exponential | 3500 | Hz |

### Vocal Modes (Toggle: ≥64 = ON)

| CC | Parameter | Type | Default |
|----|-----------|------|---------|
| 80 | Nasal | Boolean | OFF |
| 81 | Sing | Boolean | OFF |
| 82 | Shout | Boolean | OFF |
| 83 | Fry | Boolean | OFF |

### Disyn Source

| CC | Parameter | Range | Mapping | Default | Unit |
|----|-----------|-------|---------|---------|------|
| 16 | Algorithm | 0-6 | Discrete | 0 | - |
| 17 | Param 1 | 0-1 | Linear | 0.5 | - |
| 18 | Param 2 | 0-1 | Linear | 0.5 | - |
| 19 | Level | 0-1 | Linear | 0.8 | - |
| 20 | Noise Level | 0-1 | Linear | 0.15 | - |
| 21 | DC Level | 0-1 | Linear | 0.0 | - |

### Interface & Delay

| CC | Parameter | Range | Mapping | Default | Unit |
|----|-----------|-------|---------|---------|------|
| 24 | Interface Type | 0-11 | Discrete | 2 | - |
| 26 | Tuning | -12 to +12 | Linear | 0 | semitones |
| 27 | Ratio | 0.5-2.0 | Exponential | 1.0 | - |

### Feedback

| CC | Parameter | Range | Mapping | Default | Unit |
|----|-----------|-------|---------|---------|------|
| 28 | Delay 1 Feedback | 0-1 | Linear | 0.2 | - |
| 29 | Delay 2 Feedback | 0-1 | Linear | 0.2 | - |
| 30 | Filter Feedback | 0-1 | Linear | 0.1 | - |

### Filter

| CC | Parameter | Range | Mapping | Default | Unit |
|----|-----------|-------|---------|---------|------|
| 32 | Frequency | 20-20000 | Exponential | 2000 | Hz |
| 33 | Q | 0.1-10 | Exponential | 1.0 | - |
| 34 | Shape | 0-1 | Linear | 0.0 | LP→BP→HP |

### Envelope

| CC | Parameter | Range | Mapping | Default | Unit |
|----|-----------|-------|---------|---------|------|
| 73 | Attack | 0.001-1.0 | Exponential | 0.01 | seconds |
| 72 | Release | 0.01-3.0 | Exponential | 0.05 | seconds |

### Modulation

| CC | Parameter | Range | Mapping | Default | Unit |
|----|-----------|-------|---------|---------|------|
| 36 | LFO Freq | 0.1-20 | Exponential | 5.0 | Hz |
| 37 | AM/FM Depth | -1 to +1 | Bipolar | 0.0 | - |

---

## Parameter Mapping Functions

### Exponential Mapping

Used for frequency parameters (formants, filter freq, LFO freq).

```javascript
function expMap(value, min, max) {
    return min * Math.pow(max / min, value);
}

// Example: F1 (200-1000 Hz)
const f1Hz = expMap(0.5, 200, 1000);  // ~447 Hz
```

### Linear Mapping

Used for most level/feedback parameters.

```javascript
function lerp(value, min, max) {
    return min + value * (max - min);
}

// Example: Master Gain (0-1)
const gain = lerp(0.75, 0, 1);  // 0.75
```

### Bipolar Mapping

Used for AM/FM depth (-1 to +1).

```javascript
function bipolar(value) {
    return (value - 0.5) * 2;
}

// Example: AM/FM Depth
const depth = bipolar(0.75);  // +0.5 (FM)
const depth = bipolar(0.25);  // -0.5 (AM)
```

### Discrete Mapping

Used for algorithm selector, interface type.

```javascript
function discrete(value, max) {
    return Math.floor(value * (max + 0.999));
}

// Example: Interface Type (0-11)
const type = discrete(0.5, 11);  // 6 (Bell)
```

### MIDI CC Conversion

```javascript
function toMidiCC(value) {
    return Math.max(0, Math.min(127, Math.round(value * 127)));
}

// Example: 0-1 value → 0-127 CC
const ccValue = toMidiCC(0.5);  // 64
```

---

## Error Handling

### Connection Errors

```javascript
ws.onerror = (error) => {
    console.error('WebSocket error:', error);
    // UI should show disconnected state
};

ws.onclose = () => {
    console.warn('WebSocket closed');
    // Implement auto-reconnect with exponential backoff
    setTimeout(() => connect(), 2000);
};
```

### Invalid Messages

Server will log warnings for:
- Unknown message types
- Missing required fields
- Out-of-range values (CC/note numbers)

Client should validate before sending:
```javascript
function sendCC(cc, value) {
    if (cc < 0 || cc > 127) {
        console.error('Invalid CC number:', cc);
        return;
    }
    if (value < 0 || value > 127) {
        console.error('Invalid CC value:', value);
        return;
    }
    ws.send(JSON.stringify({ type: 'cc', cc, value }));
}
```

---

## Performance Notes

### Bandwidth

- Audio frames: 2100 bytes × 60 Hz = 126 KB/s (~1 Mbps)
- MIDI CC: ~50 bytes each (negligible)
- **Total:** ~1 Mbps

### Latency

- WebSocket RTT: <1 ms (localhost)
- MIDI processing: <1 ms
- Synth processing: ~10 ms (buffer size)
- **Total control latency:** ~12 ms (parameter change → audio response)

### Update Rates

- Audio frames: 60 Hz (16.67 ms intervals)
- MIDI CC: On-demand (user interaction)
- No throttling required for CC messages (user-driven)

---

## Examples

### Complete Client Implementation

See `/client/src/api/WebSocketClient.js` for full reference implementation.

### Testing with wscat

```bash
# Install wscat
npm install -g wscat

# Connect
wscat -c ws://localhost:8081

# Send CC
> {"type":"cc","cc":7,"value":100}

# Send Note On
> {"type":"note_on","note":60,"velocity":96}

# Send Note Off
> {"type":"note_off","note":60}
```

### cURL (HTTP API - Not Supported)

Note: This implementation uses WebSocket only. There is no REST API. For HTTP control, you would need to add Express routes to the server.

---

## Version History

- **v0.1.0** (2025-12-08): Initial implementation
  - 29 MIDI CC parameters
  - Binary audio streaming (60 Hz)
  - Note On/Off support
  - Auto-reconnect

---

## See Also

- `web-ui/README.md` - Setup guide
- `docs/UI-PLAN.md` - Implementation plan
- `server/index.js` - Server implementation
- `client/src/api/WebSocketClient.js` - Client implementation
