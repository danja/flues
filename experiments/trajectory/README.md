# Trajectory : Polygon Bounce Oscillator

## Algorithm

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

## Deeper derivation

### Geometry setup

The polygon is defined as a regular N-gon on the unit circle:
```
v_i = (cos(2*pi*i/N + pi/N), sin(2*pi*i/N + pi/N))
```
The extra `pi/N` rotation aligns a flat edge to the horizontal axis, which makes the bounce feel more symmetric around the y-axis.

Each edge has a direction vector `e = v_{i+1} - v_i` and an outward unit normal:
```
n = normalize( (e_y, -e_x) )
```

### Inside/outside test

For a convex polygon, a point is inside if it lies on the inner side of every edge:
```
inside = all( cross(e, p - v_i) >= 0 )
```
This uses a consistent winding order of vertices (counter-clockwise).

### Motion and pitch scaling

A note at frequency `f` determines a step size per sample:
```
speed = (f * 4) / sampleRate
position' = position + velocity
```
The constant `4` is a practical tuning factor: it makes one traversal of the polygon roughly align to audible periods without driving the point too fast to resolve the reflections.

### Reflection via half-space correction

When the next position is outside, we select the edge with the maximum penetration distance:
```
distance = dot(p - v_i, n)
```
The edge with the largest positive distance is the deepest violation.

We then reflect velocity across that edge normal:
```
v_reflected = v - 2 * dot(v, n) * n
```
And project the point back inside:
```
p_corrected = p - n * (distance + eps)
```
The `eps` nudge prevents the point from sitting exactly on the boundary, which avoids repeated collisions that can lock the orbit.

### Output signal

The oscillator output is the y-position:
```
output = y * 0.9
```
This keeps the waveform centered and leaves headroom for downstream processing.
