# ArpIso User Manual

ArpIso is a Launchpad-driven drum sequencer LV2 plugin for the Novation Launchpad Mini MK3. It turns the full 8x8 grid into a 64-step pattern editor with 8 drum voices, two pattern slots (A/B), and per-voice Euclidean generation.

## Quick Start

1. Load ArpIso on a MIDI track.
2. Route Launchpad DAW MIDI to ArpIso **Control In** and ArpIso **Launchpad Control** back to the Launchpad.
3. Route ArpIso **MIDI Out** to a drum instrument.
4. Press play in the host; ArpIso follows tempo and transport.

## Grid Layout

- **Grid (8x8)** = 64 steps for the selected voice
- **Side buttons** = select voice 0-7
- **Top row** = Euclid, length, pattern, clear

## Controls

### Top Row (CC 91-99)

- CC 91: Euclid pulses up (current voice)
- CC 92: Euclid offset up (current voice)
- CC 93: Active columns down (1-8)
- CC 94: Active columns up (1-8)
- CC 95: Pattern A
- CC 96: Pattern B
- CC 97: Clear current voice + Euclid
- CC 98: Clear pattern + Euclid
- CC 99 (logo): Clear pattern + Euclid

### Side Buttons (CC 19-89)

- Buttons 0-7 select the drum voice (top to bottom).
- Selected voice is highlighted.
- Default MIDI notes per voice (0-7): 36, 40, 39, 50, 42, 46, 53, 51.

### Grid Behavior

- Tap a pad to toggle a step on/off for the selected voice.
- Pad velocity becomes step velocity.
- Active steps are yellow, inactive steps are off.
- Inactive columns (past the active length) are gray.
- The playhead is green and advances on 16th notes.

## Euclidean Sequencing

- Each voice has its own **pulses** and **offset**.
- Euclid generation is based on the **current active column count**.
- Changing length re-computes Euclid steps for voices with pulses enabled.

## Patterns

- Two pattern slots: **A** and **B**.
- Switching patterns saves/restores all voice steps.
- Clearing a pattern also resets Euclid values for that pattern.

## Transport Sync

- ArpIso follows the host transport (`time:Position`).
- Tempo changes update the step clock automatically.
- If the host does not send transport info, the sequencer will not run.

## UI Behavior

- The UI mirrors Launchpad LED state.
- Click pads in the UI to toggle steps.
- UI status shows current voice, pattern, Euclid values, and playhead.

## Troubleshooting

- **No LEDs**: confirm Launchpad DAW routing to ArpIso control ports.
- **No playback**: start the host transport and verify time sync.
- **No MIDI notes**: route ArpIso MIDI Out to a drum instrument.
