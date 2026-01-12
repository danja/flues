# Trajectory: A Geometric Approach to Audio Synthesis

**January 10, 2026**

Most audio oscillators generate waveforms by evaluating mathematical functions—sine waves, sawtooth ramps, or band-limited pulse trains. The Trajectory oscillator takes a different approach: it simulates a point bouncing inside a polygon and converts its motion into sound.

## The Core Idea

Imagine a billiard ball moving inside a regular polygon—a triangle, square, hexagon, or any shape with 3 to 12 sides. The ball travels in a straight line until it hits an edge, then reflects perfectly and continues. The Trajectory oscillator tracks this motion in real-time, using the x and y coordinates of the moving point as the audio signal.

The algorithm runs at audio rate (48,000 times per second), updating the point's position on each sample:

```
speed = (frequency × 4) / sampleRate
position[n+1] = position[n] + velocity
if outside polygon: reflect velocity and nudge point back inside
output = (x × mixX + y × mixY) × 1.0
```

The speed of movement scales with the note frequency—higher notes produce faster traversal, creating higher-frequency waveforms. This coupling between pitch and motion makes the oscillator musically playable.

## Reflection Mechanics

The reflection algorithm determines which edge the point has penetrated, calculates the perpendicular normal to that edge, and mirrors the velocity vector. A small inward nudge prevents the point from getting stuck on edges during subsequent collisions. The polygon is normalized to unit radius and centered at the origin, simplifying the geometry.

## Musical Parameters

The oscillator exposes five control parameters:

- **Sides**: The polygon's edge count (3-12). A triangle produces angular, chaotic paths. A dodecagon (12 sides) approaches circular motion.
- **Start Position**: The launch angle from center (0-360°), determining where the point begins.
- **Start Angle**: The initial direction of travel (0-360°).
- **Mix X**: How much the x-coordinate contributes to the output (0-1).
- **Mix Y**: How much the y-coordinate contributes to the output (0-1).

The mix controls are particularly useful. Setting `mixX = 1.0, mixY = 0` produces a waveform based solely on horizontal motion. Blending both coordinates creates more complex trajectories.

## Spectral Character

Unlike traditional oscillators with fixed harmonic structures, the Trajectory oscillator's spectrum depends on the geometric path. Low-sided polygons (triangles, squares) produce more abrupt reflections, generating brighter spectra with stronger high-frequency content. Higher-sided polygons create smoother motion and gentler harmonic profiles.

The start position and angle parameters determine the trajectory's periodicity. Some combinations produce perfectly periodic paths that loop cleanly, while others create quasi-periodic or chaotic motion. This variability makes the oscillator suitable for both tonal and textural synthesis.

## Implementation in Flues-Synth

The Trajectory oscillator appears as **Program 2** in the flues-synth headless Raspberry Pi synthesizer. It replaces the Disyn distortion synthesis module, operating as the primary excitation source. The output feeds into the formant filters and physical modeling pipeline, where it can be shaped by resonance, feedback, and modulation.

The algorithm was developed in the `experiments/trajectory/` directory as a JavaScript prototype before being ported to C for the embedded synthesizer. The full derivation and implementation notes are documented in that project's README.

## Practical Use

In practice, the Trajectory oscillator excels at creating animated, evolving timbres. Automating the polygon side count during a note creates smooth morphing between geometric behaviors. Modulating the mix controls shifts the spectral balance in real-time. Paired with feedback and filtering, it becomes a source for self-oscillating patches that drift between stability and chaos.

The geometric foundation makes it visually intuitive—you can imagine the bouncing point and anticipate how parameter changes will affect the sound. This directness contrasts with abstract waveshaping or spectral formulas, offering a different design workflow.

## References

The complete algorithm specification, including coefficient calculations and stability requirements, is documented in `flues-synth/docs/algorithms.md` under "Trajectory Oscillator (Polygon Bounce)." The implementation resides in `flues-synth/src/audio/modules/` as part of the modular DSP engine.

The Trajectory oscillator demonstrates that synthesis algorithms don't need complex mathematics to produce interesting results. Sometimes a simple physical model—a point, a polygon, and a rule—is enough.
