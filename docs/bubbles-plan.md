# Bubbles: Physical-Modelling Water Synth Plugin Plan

## Goal
Design an LV2 instrument/effect plugin that synthesizes water-like sounds in real time: flowing, splashing, bubbling, dripping, and related textures.

## Scope and Constraints
- Real-time safe DSP (no heap allocation in `run()` callback).
- Stable at 44.1-96 kHz.
- CPU target: under 5% for stereo at 48 kHz on a typical laptop for default preset.
- Modulation-friendly: parameters should respond smoothly to host automation.
- Physical plausibility over strict physical accuracy.

## Sound Palette, Simulation Requirements, and Implementation Approaches

| Sound | Audible Features | Simulation Requirements | Practical Approach |
|---|---|---|---|
| Gentle stream / flowing creek | Broad noise, moving resonances, micro-transients | Turbulent broadband source, distributed resonators, slow stochastic modulation | Filtered noise + modal bank (8-24 bandpass modes) + low-rate random walk modulation of mode gains/frequencies |
| Fast river / rapids | Brighter noise, denser impacts, stronger spray | Higher turbulence energy, frequent micro-impacts, splash tails | Multi-band noise source + Poisson-triggered micro-impulse events feeding short resonators/reverb |
| Surface bubbling (small bubbles) | Random short plops, high-mid emphasis | Bubble birth events, radius-dependent pitch/decay | Event model: sample bubble radius from distribution, map radius -> resonant frequency and decay, excite damped sinusoid |
| Boiling / dense bubbling | Continuous popping cloud, noisy body | Very high event density, overlapping bubble modes, pressure-dependent rate | Layered bubble event generators with rate tied to "heat" control + soft limiting/companding |
| Single droplets / drips | Isolated ping + short ambience | Drop impact impulse, cavity/body resonance, occasional double-hit | Impact impulse -> 2-3 resonant modes + short early reflections; probabilistic secondary droplet |
| Splash hits | Broadband transient + decaying noisy tail | Impact force control, spray burst, cavity response | Force-scaled impulse + filtered noise burst through transient envelope + modal resonator + short convolution/FDN tail |
| Shore wash / wave lap | Slow swell noise, periodic surge, foam crackle | Low-frequency envelope cycles, correlated noise, sparse crackle events | Quasi-periodic LFO/envelope controlling colored noise level + random crackle transients |
| Underwater gurgle | Muffled low-mid motion, formant-like movement | Band-limited turbulent source, moving formants, reduced highs | Lowpass/noise source + 2-4 moving formant filters + occasional bubble transients |
| Pouring liquid | Semi-steady stream with pitch drift and splatter | Continuous jet model, container resonance, intermittent droplets | Continuous filtered noise jet + container modal resonator + random drip/splash sidechain |
| Rain textures | Sparse-to-dense droplet field | Stochastic drop arrival process, impact variation by size/material | Multi-stream Poisson drop generators; each event excites impact model with random pitch/Q |

## Physical Modelling Building Blocks

1. Excitation sources
- Turbulence source: white/pink noise with controllable spectral tilt.
- Impact source: impulses with force-dependent amplitude and brightness.
- Bubble source: event-based damped oscillators with radius-dependent tuning.

2. Resonance/body models
- Modal resonator bank for water/body/pipe/container modes.
- Optional short waveguide (delay + damping + dispersion) for tube/channel coloration.
- Formant cluster for underwater character.

3. Propagation and space
- Early reflections: small tapped delay network.
- Tail: lightweight FDN or Schroeder reverb.
- Optional stereo decorrelation via slightly different delay/modulation per channel.

4. Event/statistical systems
- Poisson/Bernoulli event clocks for droplets/bubbles/splashes.
- Correlated random walk for slow environment drift.
- Distributions for event size/energy (log-normal or exponential work well).

5. Nonlinearities and safety
- Soft saturation (`tanh` or cubic) for dense events.
- DC blocker on output.
- Parameter smoothing (1-pole) for all user controls.

## Candidate Plugin Modes
- `Flow`: continuous stream and river textures.
- `Bubble`: sparse to boiling bubbling beds.
- `Drip`: individual droplets and rain-like patterns.
- `Splash`: transient-rich hits and bursts.
- `Underwater`: muted gurgle and submerged ambience.
- `Hybrid`: crossfade among all models.

## Suggested User Controls
- `Intensity`: global energy into all exciters.
- `Density`: event rate for bubbles/drops/splash grains.
- `Size`: average bubble/drop size (affects pitch + decay).
- `Flow Rate`: continuous turbulence level.
- `Brightness`: source spectral tilt and damping coupling.
- `Resonance`: modal Q / body sustain.
- `Depth`: underwater lowpass/formant emphasis.
- `Stereo Spread`: decorrelation and channel variance.
- `Space`: early reflection + reverb send.
- `Randomness`: variance of timing/size/pitch.
- `Heat`: boiling tendency (bubble clustering and rate bursts).
- `Output`: master gain.

## DSP Architecture Proposal

### Module layout
- `WaterSourceModule`: turbulence and impact/noise excitation.
- `BubbleEventModule`: event scheduler and radius-based bubble synthesis.
- `DropletEventModule`: droplet/splash event generation.
- `ModalBodyModule`: modal resonator bank with configurable mode sets.
- `SpatialModule`: early reflections + FDN/Schroeder tail.
- `DynamicsModule`: soft clipper, compander (optional), DC blocker.
- `WaterEngine`: coordinates modes, parameter mapping, note/transport hooks.

### Signal flow
`Events + Turbulence -> Body Resonance -> Spatial -> Dynamics -> Output`

### Real-time strategies
- Pre-allocate event pools and mode buffers.
- Use fixed-size arrays and branch-light inner loops.
- Update stochastic/event logic at control rate (e.g. every 16-64 samples).
- Keep denormal protection active for quiet tails.

## Parameter Mapping Ideas
- `Size -> Bubble pitch`: inverse relation (smaller bubble => higher resonance).
- `Intensity -> Density`: nonlinear mapping to avoid too-busy low settings.
- `Brightness -> Damping`: brighter also slightly lowers damping for liveliness.
- `Depth -> LPF cutoff`: exponential mapping to perceptual range.
- `Randomness`: increases jitter in event interval, amplitude, and tuning.

## Preset Concepts
- `Creek Morning`: low density, mid brightness, gentle flow.
- `Mountain Rapids`: high flow, bright, frequent micro-splashes.
- `Hot Springs`: high heat, medium size, dense bubbles.
- `Cave Drips`: low flow, sparse droplets, long resonances.
- `Underwater Vent`: high depth, low brightness, clustered gurgles.
- `Rain Gutter`: medium-high drip density, metallic body modes.

## Implementation Roadmap

1. Prototype core generators
- Build turbulence + bubble event model in isolation.
- Verify stability and level behavior.

2. Add body resonance
- Implement modal bank and tune default mode tables.
- Expose `Resonance`, `Brightness`, `Size` mappings.

3. Add droplet/splash transients
- Integrate impact events and transient envelopes.
- Introduce `Density`/`Randomness` interaction.

4. Add space + dynamics
- Early reflections and lightweight tail reverb.
- Soft clipping and DC blocker.

5. LV2 integration + UI
- Add ports, MIDI/automation handling, and X11/Cairo UI.
- Group controls by Source, Events, Body, Space, Output.

6. Presets + calibration
- Make 8-12 factory presets with gain-normalized output.
- Validate against CPU/latency budget.

## Testing and Validation Requirements
- Null/idle test: no denormals, no runaway DC.
- Stress test: max density/intensity without clipping explosions.
- Sample-rate test: same character at 44.1/48/96 kHz.
- Automation test: zipper-noise-free parameter sweeps.
- Host test: load/save state, transport start/stop robustness.
- Listening test set: compare presets on monitors + headphones.

## Risks and Mitigations
- Risk: Sound becomes generic filtered noise.
- Mitigation: Emphasize event-based physics (bubble/drop processes) and tuned modal responses.

- Risk: Excess CPU from many active events/modes.
- Mitigation: Voice/event cap, adaptive quality mode, control-rate updates.

- Risk: Harshness in dense splash presets.
- Mitigation: Dynamic high-frequency damping and soft saturation.

## Minimum Viable Version (MVP)
- One mode (`Hybrid`) with these components:
  - Turbulence noise source
  - Bubble event generator
  - Droplet impact generator
  - 10-mode resonator bank
  - Simple stereo reverb
  - Output limiter/DC blocker
- 8 core controls: Intensity, Density, Size, Brightness, Resonance, Depth, Space, Output.
- 6 curated presets.

## Future Extensions
- Mesh/surface model for wave slosh in virtual containers.
- Granular sample assist layer (blended quietly under physical model).
- Sidechain input to "excite" splashes from external audio.
- MIDI note mapping for pitched water percussion.
- Multichannel/ambisonic output variant.
