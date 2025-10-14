# Breath Pressure vs Noise Level

## Breath Pressure

**What it is**: The steady air pressure from the player's lungs flowing into the mouthpiece.

**Physical role**: This is the primary energy source that drives the reed oscillation. In the Karplus-Strong/waveguide model, breath pressure creates a pressure difference across the reed, causing it to vibrate. The breath pressure interacts with the reflected wave in the bore through the nonlinear reed reflection function.

**In the code** (experiments/clarinet-synth/src/audio/ClarinetEngine.js:229-240):
```javascript
// Breath pressure is the main driving force
const breathInput = this.breathPressure * envelope + noiseInput * envelope - delayedSample;
const reedOutput = this.reedReflection(breathInput);
```

**Sound effect**:
- **Higher breath pressure** (counterintuitively, lower in our inverted UI) = more energy = potentially louder, but can become unstable/squeaky with high reed stiffness
- **Lower breath pressure** = less energy = quieter, more stable tone

The breath pressure is a **DC (constant) component** - it's a steady value that doesn't change rapidly.

## Noise Level

**What it is**: Random turbulence in the air stream - the "breathiness" or "air noise" you hear in real wind instruments.

**Physical role**: Real breath isn't perfectly smooth - it contains random fluctuations from turbulence in the airflow, irregularities in the reed vibration, and acoustic noise. This adds realism and warmth to the sound.

**In the code** (experiments/clarinet-synth/src/audio/ClarinetEngine.js:212-214):
```javascript
generateNoise() {
    return (Math.random() * 2 - 1) * this.noiseLevel;
}
```

Then mixed in (line 229):
```javascript
const noiseInput = this.generateNoise();
const breathInput = this.breathPressure * envelope + noiseInput * envelope - delayedSample;
```

**Sound effect**:
- **Higher noise** = more breathiness, "airier" tone, less pure
- **Lower noise** = cleaner, more pure sine-like tone (less realistic)

The noise is an **AC (varying) component** - it's random and changes every sample.

## Key Differences Summary

| Aspect | Breath Pressure | Noise Level |
|--------|----------------|-------------|
| **Type** | DC (steady) | AC (random) |
| **Role** | Primary energy source | Turbulence/imperfection |
| **Effect** | Volume and oscillation stability | Breathiness and realism |
| **Real instrument** | How hard you blow | Turbulence in your breath |
| **Signal** | Constant value | White noise |

## Try This Experiment

1. Set **Noise = 0**, **Breath = mid-range**: You'll get a very pure, almost synthesizer-like tone
2. Set **Noise = high** (30-40): You'll hear breathiness and air noise, more like a real instrument
3. The two work together: `totalInput = steadyBreathPressure + randomNoise`

The noise adds the "imperfections" that make wind instruments sound organic and alive rather than purely electronic.

## Mathematical Representation

In the signal processing chain:

```
Input Signal = (Breath Pressure × Envelope) + (Noise × Envelope) - Delayed Sample
              └─────── DC component ──────┘   └──── AC component ────┘
```

Where:
- **Breath Pressure**: Constant value (e.g., 0.4)
- **Noise**: `random() * noiseLevel` - changes every sample
- **Envelope**: ADSR envelope (0.0 to 1.0) for attack/release shaping
- **Delayed Sample**: Feedback from the waveguide delay line

The reed reflection function then processes this combined input to create the nonlinear behavior that generates the rich harmonic content characteristic of reed instruments.
