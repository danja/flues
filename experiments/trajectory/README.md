# Trajectory (Polygon Bounce Oscillator)

## High-level idea

Trajectory is a synthesizer whose oscillator is driven by a point bouncing inside a regular polygon. The point moves in straight lines, reflects perfectly off the edges, and the oscillator output is the point's y-position. Changing the polygon sides and the two angular parameters reshapes the orbit, producing tones that range from stable to quasi-chaotic.

The core controls are:
- Sides: number of polygon edges (3-12)
- Start Position: angle from the center to the starting point on the polygon boundary
- Start Angle: launch direction of the moving point

Velocity is derived from the played note so the motion rate tracks pitch.

## Implementation notes

- The polygon is a unit-radius regular shape centered at the origin. Vertices are rotated by pi/sides to align a side horizontally.
- Each sample advances the point by a fixed step (speed) set by the note frequency. The step is reflected when the point would exit the polygon.
- Reflection uses a half-space correction: when a step leaves the polygon, the most-penetrated edge normal is used to reflect and nudge back inside.
- The output waveform is the y-coordinate scaled for audio headroom.
- Parameter updates rebuild the polygon or reset the point and velocity as needed for stable trajectories.
