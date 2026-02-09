# Ants LV2 Plugin Plan

Ants is a MIDI generator based on cellular automata and pheromone trails. It hosts a 16x16 grid where each row maps to a scale degree and each column is a spatial/time position. Ants (2–8) move across the grid at each beat and trigger polyphonic notes when they land on cells.

## Requirements Summary
- 16x16 grid mapped to scale degrees (not raw chromatic)
- Scale selector (common scales)
- 2–8 ants
- Polyphonic MIDI output
- Pheromone trail system with decay and deposit
- Movement based on pheromone + randomness + cell state
- Host tempo sync via LV2 time:Position

## System Design

### Grid + Mapping
- Grid dimensions: 16 columns x 16 rows.
- Rows map to scale degrees across multiple octaves:
  - Use 16 rows → distribute 2–3 octaves of the selected scale.
  - Example: row 0 = root (low), row 15 = top scale degree (high).
- Columns are spatial; ants can move across columns to influence rhythm.

### Scale System
- Scale selector control (chromatic, major, natural minor, harmonic minor, melodic minor, pentatonic major/minor, blues, dorian, mixolydian, etc.).
- Scale degrees precomputed into a 16-step pitch table for the current root + scale.

### Ant Movement
- Each ant stores current (x,y), last direction, and energy.
- On each beat:
  1. Update pheromone decay.
  2. For each ant: evaluate neighboring cells (4-way or 8-way) and compute score:
     `score = w_pheromone * pheromone + w_state * cell_state + w_random * rand()`
  3. Choose next cell by weighted probability.
  4. Deposit pheromone at new cell.

### Pheromone Field
- 16x16 float grid.
- Decay each beat: `pheromone *= decay`.
- Deposit: `pheromone += deposit` (clamped to max).
- Optional “avoid” mode by inverting pheromone weight.

### MIDI Output
- Polyphonic: each ant emits a Note On when it lands.
- Note Off after fixed duration (e.g., fraction of beat) or a per-note length parameter.
- Velocity derived from pheromone intensity or ant energy.

## Parameters (Initial Set)
- Ant Count (2–8)
- Scale (selector)
- Root Note (C–B)
- Move Steps (1–4)
- Randomness (0–1)
- Pheromone Deposit
- Pheromone Decay
- Trail Bias (follow/avoid)
- Velocity Scale
- Note Length

## LV2 Implementation Plan

### 1) Metadata + Ports
- Create `lv2/ants/ants.lv2/manifest.ttl` and `ants.ttl`.
- Ports:
  - MIDI out (atom:Sequence)
  - Control ports for params listed above
  - Optional MIDI in for sync/seed reset

### 2) DSP Engine
- `AntsEngine` module:
  - Grid state, pheromone grid
  - Ant array
  - Scale mapping table
- Beat sync using `time:Position` from host.
- On each beat: update ants + emit MIDI events.

### 3) UI (X11/Cairo)
- 16x16 grid visualization (optional in v1)
- Controls: ant count, scale, root, randomness, decay, deposit
- Start simple with knobs + dropdowns, add grid visualization later.

### 4) Tests
- Unit tests for scale mapping
- Deterministic ant movement with fixed RNG seed
- MIDI event timing sanity check

## Build Steps
- CMake target `lv2/ants`
- Link LV2 + X11/Cairo
- Install bundle to `~/.lv2/ants.lv2`

## Future Enhancements
- Different movement modes (4-way, 8-way, wrap, bounce)
- Per-ant behaviors (leader, follower, random)
- Pheromone visualization in UI
- Export/record generated MIDI
