# P-Mix Manual

P-Mix is a probabilistic mixer LV2 plugin that toggles audio on or off at bar boundaries. It is designed to create rhythmic dropouts, stutters, and probabilistic mutes across up to eight channels at once. The plugin uses host transport/tempo data to decide when a new bar starts, then rolls weighted probabilities to decide whether to keep playing, fade, or cut.

## Quick Start

1. Insert P-Mix on an audio track or bus.
2. Ensure your DAW transport is running (bar timing is required).
3. Set Granularity to decide how often decisions happen (in bars).
4. Use Maintain/Fade/Cut to shape behavior, and Bias to target the overall play ratio.

## Signal Flow

Input channels (1-8) -> probabilistic gain -> output channels (1-8)

## Controls

- Granularity (1-32, bars): How often to evaluate a new decision. 1 means every bar, 4 means every 4 bars.
- Maintain (0-100): Weight to keep the current state.
- Fade (0-100): Weight to fade to the next target state.
- Cut (0-100): Weight to instantly switch to the next target state.
- Fade Dur Max (0.125-1.0): Maximum fade length as a fraction of the decision window. The actual fade is randomized between 12.5% and this value.
- Bias (0-100): Target long-term play ratio. Higher values favor the track being on more of the time.

Notes on behavior:
- Maintain/Fade/Cut are normalized, so only their relative weights matter.
- Bias selects the target state at each decision point. Example: Bias 70 aims for the track to be playing about 70% of the time on average.
- Without host transport (no bar timing), P-Mix will not perform transitions and audio stays on.

## Tips

- For subtle movement: Granularity 4, Maintain 70, Fade 25, Cut 5, Bias 60.
- For glitchy chops: Granularity 1, Maintain 20, Fade 30, Cut 50, Bias 50.
- For sparse dropouts: Granularity 8, Maintain 60, Fade 20, Cut 20, Bias 40.

## Troubleshooting

- No changes happen: verify the DAW is playing and sending bar/tempo info.
- Abrupt jumps: increase Fade and reduce Cut, or increase Fade Dur Max.
