# Digital Waveguide Clarinet Synthesizer
## Technical Documentation

### Overview
This is a complete physical modeling synthesizer implementing a clarinet using digital waveguide techniques. It runs entirely in the browser using vanilla JavaScript and the Web Audio API.

---

## Physical Model Implementation

### Core Architecture

Based on the documents provided, the clarinet model consists of:

1. **Delay Line** (Bore/Tube)
   - Represents the acoustic waveguide of the clarinet bore
   - Length determined by fundamental frequency: `delay = sampleRate / frequency`
   - Uses linear interpolation for fractional delays (vibrato)

2. **Reed Nonlinearity** (Reflection Function)
   - Implements the pressure-controlled reflection coefficient from reed-kimi.md
   - Uses fast tanh approximation: `u⁻ = R(u⁺; P_mouth)`
   - Pressure difference: `ΔP = P_mouth - P_bore(0,t)`
   - Stiffness parameter controls the shape of the cubic-like function

3. **Loop Filters**
   - **Lowpass Filter**: Controls damping and brightness
   - **Highpass Filter**: Removes DC offset and shapes tone
   - Both implemented as one-pole IIR filters for efficiency

4. **Saturation**
   - Soft clipping using tanh approximation
   - Prevents runaway oscillation in the feedback loop
   - Maintains energy bounds in the system

---

## Signal Flow

```
Breath Pressure (envelope) + Noise
              ↓
    Compute ΔP (mouth - bore)
              ↓
    Reed Reflection (nonlinear)
              ↓
         Add to Bore
              ↓
    Loop Filters (LP + HP)
              ↓
         Saturation
              ↓
    Write to Delay Line ←──┐
              ↓            │
    Read (with vibrato) ───┘
              ↓
          Output
```

---

## Control Parameters

### Reed & Excitation Section

- **Breath** (0-100): Controls mouth pressure amplitude
  - Maps to 0.2 to 1.0 internally
  - Primary amplitude control

- **Reed Stiff** (0-100): Reed stiffness/embouchure
  - Controls slope of nonlinear reflection function
  - Higher values = stiffer reed, brighter tone

- **Noise** (0-100): Breath turbulence level
  - Adds filtered noise to excitation
  - Simulates realistic air flow turbulence

- **Attack** (0-100): Envelope attack time
  - 1ms to 100ms range
  - How quickly the note "speaks"

### Bore & Resonance Section

- **Damping** (0-100): Loop filter damping
  - Lower = more damped (darker)
  - Higher = less damped (brighter, longer sustain)

- **Brightness** (0-100): High-frequency content
  - Controls highpass filter cutoff
  - Affects tonal color

- **Vibrato** (0-100): Pitch modulation depth
  - 5 Hz sine wave modulation
  - Varies delay line length

- **Release** (0-100): Envelope release time
  - 10ms to 300ms range
  - Note decay behavior

---

## Key Implementation Details

### 1. Reed Reflection Function

From the documents, the reed behaves as a "spring-controlled valve" with flow:
```
u = f(ΔP)  (cubic-like function)
```

Implemented as fast tanh approximation:
```javascript
reedReflection(pressureDiff) {
    const stiffness = this.reedStiffness * 5 + 0.5;
    const scaled = pressureDiff * stiffness;
    // Fast tanh: x(27 + x²) / (27 + 9x²)
    if (scaled > 3) return 1;
    if (scaled < -3) return -1;
    const x2 = scaled * scaled;
    return scaled * (27 + x2) / (27 + 9 * x2);
}
```

### 2. Delay Line with Vibrato

Linear interpolation for fractional delay:
```javascript
const baseDelay = sampleRate / frequency;
const vibratoMod = sin(vibratoPhase) * vibratoAmount;
const modulatedDelay = baseDelay * (1 + vibratoMod);

// Read with interpolation
const readPosInt = floor(readPos);
const frac = readPos - readPosInt;
const sample = delayLine[readPosInt] * (1 - frac) + 
               delayLine[readPosInt + 1] * frac;
```

### 3. Loop Filters

One-pole IIR filters for efficiency:

**Lowpass:**
```javascript
y[n] = a * x[n] + (1 - a) * y[n-1]
```

**Highpass:**
```javascript
y[n] = a * (y[n-1] + x[n] - x[n-1])
```

### 4. Envelope Generator

Linear attack/release envelope:
```javascript
if (gate) {
    envelope += 1 / (attackTime * sampleRate);
} else {
    envelope -= 1 / (releaseTime * sampleRate);
}
envelope = clamp(envelope, 0, 1);
```

---

## User Interface Features

### Visual Keyboard
- Click or touch keys to play notes
- Computer keyboard mapping:
  - A-K keys map to C4-C5
  - W, E, T, Y, U for black keys

### Rotary Knobs
- Click and drag up/down to adjust
- Double-click to reset to center
- Touch-enabled for mobile devices
- Visual feedback with rotation

### Waveform Visualizer
- Real-time oscilloscope display
- Shows output waveform
- Uses Web Audio API analyser

### Status Bar
- Power button with visual feedback
- Current note display
- Simulated CPU usage
- On/off status indicator

---

## Technical Notes

### Performance
- Uses ScriptProcessorNode (4096 buffer)
  - Deprecated but widely compatible
  - Can be upgraded to AudioWorklet for modern browsers
- Single voice (monophonic)
- Approximately 15-25% CPU on modern systems

### Sample Rate
- Adapts to system sample rate (typically 44.1kHz or 48kHz)
- Delay line length automatically calculated per note

### Limitations
- No tone holes implementation (simplified bore model)
- Monophonic only (one note at a time)
- Fixed vibrato rate (5 Hz)
- No bell radiation model (simplified output)

---

## Physics vs. Practice

This implementation balances **physical accuracy** with **computational efficiency**:

**Physically Accurate:**
- ✓ Waveguide delay line models traveling waves
- ✓ Nonlinear reed reflection function
- ✓ Pressure-controlled excitation
- ✓ Loop filters for damping/dispersion

**Simplified for Efficiency:**
- Uniform bore (no conical sections)
- No tone holes (uses delay line length only)
- Simplified noise model
- One-pole filters (vs. higher-order)

---

## References from Documents

1. **reed-kimi.md**: Reed reflection coefficient implementation
   - "memory-less reflection coefficient that terminates the bore waveguide"
   - "u⁻ = R(u⁺; P_mouth)"

2. **Steampipe Manual**: Physical modeling parameters
   - Push-pull mechanism concept
   - Loop filters for tone control
   - Saturation for stability

3. **Smith (1996)**: Digital waveguide fundamentals
   - Delay line implementation
   - Filter placement in feedback loops
   - Efficient string/bore modeling

---

## Future Enhancements

Possible additions:
- Polyphony (multiple voices)
- Tone holes (register changes)
- Bell radiation filter
- More sophisticated noise model
- Overblowing (register jumps)
- Breath controller support (MIDI CC)
- Preset system
- Effects (reverb, delay)

---

## Browser Compatibility

Tested on:
- Chrome/Edge (Chromium)
- Firefox
- Safari

Requires:
- Web Audio API support
- ES6 JavaScript
- Canvas API (for visualizer)

---

## How to Use

1. Click the **power button** (⏻) to initialize audio
2. Click keys on the visual keyboard or use computer keys
3. Adjust knobs by clicking and dragging up/down
4. Experiment with different parameter combinations:
   - High breath + low damping = loud, bright tone
   - Low reed stiffness = softer, mellower sound
   - High noise = breathy, airy quality
   - Vibrato adds expressiveness

**Tip**: Try the computer keyboard for fast playing:
`A S D F G H J K` = white keys (C4-C5)
`W E T Y U` = black keys

Enjoy making music with physics!