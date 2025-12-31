# PadSeq User Manual

PadSeq is a Launchpad-driven drum sequencer LV2 plugin for the Novation Launchpad Mini MK3. It turns the full 8x8 grid into a 64-step pattern editor with 8 drum voices, two pattern slots (A/B), and per-voice Euclidean pattern generation.

## Quick Start

1. Load PadSeq on a MIDI track.
2. Route the Launchpad MIDI to the plugin's control input.
3. The plugin switches the Launchpad into Programmer mode automatically.
4. The grid lights show the current pattern state.

## Controls

### Top Row (CC 91-99)

- CC 91: Euclid Pulses  Up (current voice)
- CC 92: Euclid Offset Up (current voice)
- CC 93: Active Columns Down (1-8)
- CC 94: Active Columns Up (1-8)
- CC 95: Pattern A
- CC 96: Pattern B
- CC 97: Clear current voice (keeps Euclid values)
- CC 98: Clear current pattern + Euclid values (all voices)
- CC 99: Play/Stop

### Side Buttons (CC 89-19)

- Buttons 0-7 select the drum voice (top to bottom).
- The selected voice is highlighted.

### Grid

- Tap a pad to toggle a step on/off for the selected voice.
- Active steps are dimly lit; inactive steps are off.
- Inactive columns (past the active length) are gray.
- The playhead flashes as the sequence advances.

## Euclidean Sequencing

- Each voice has its own Euclid pulses and offset values.
- Values are stored per pattern (A/B), so each pattern can have a different Euclid setup.
- Euclid generation uses the current active column count as the pattern length.

## Patterns

- Two pattern slots: A and B.
- Switching patterns saves and restores all voice steps.
- Clearing the pattern (CC 98) also resets all Euclid values for that pattern.

## UI Behavior

- The UI mirrors the Launchpad LED state and updates on hardware interaction.
- The playhead is an overlay flash and does not modify the underlying step state.

## Troubleshooting

- No LEDs: confirm the Launchpad is routed to PadSeq and in Programmer mode.
- UI not updating: press any pad to force a sync and verify the UI state refreshes.
