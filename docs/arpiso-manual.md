# ArpIso Manual

ArpIso is an LV2 Euclidean gravity arpeggiator for the Novation Launchpad Mini MK3.
The 8x8 grid is fully reserved for performance pads, while top and right-side buttons handle controls.

## Quick Start

1. Build and install:
   - `./install-arpiso.sh`
2. In your DAW, insert `ArpIso` on a MIDI track.
3. Route:
   - Launchpad DAW Out -> ArpIso `control_in`
   - ArpIso `launchpad_out` -> Launchpad DAW In
   - ArpIso `midi_out` -> downstream instrument
4. Start DAW transport.
5. Hold pads on the 8x8 grid to create gravity wells and generate notes.

## Performance Model

- Held grid pads become gravity wells.
- Up to 5 wells can be active at once (one-hand design limit).
- A sixth simultaneous press is ignored, and a warning flash appears on the top controls.
- Each well gets:
  - Pitch from isomorphic grid mapping + scale quantization.
  - Euclidean pulses from row position.
  - Euclidean phase offset from column position.
- Playheads move between wells and trigger MIDI notes on Euclidean hits.

## Controls

### Top Row (CC 91-99)

- `91`: Start/Stop
- `92`: Clock Division
- `93`: Cycle Length
- `94`: Root Note
- `95`: Scale
- `96`: Motion Mode
- `97`: Clear Wells
- `98`: Panic + Cle ar
- `99` (logo): Panic + Clear

Scale button (`95`) cycles through:
- Major
- Natural Minor
- Dorian
- Major Pentatonic
- Mixolydian
- Phrygian
- Harmonic Minor
- Blues

### Right Column (physical top -> bottom)

- `89`: Pattern A/B toggle
- `79`: Humanize
- `69`: Velocity Curve
- `59`: Gate Length
- `49`: Travel Scale
- `39`: Gravity Strength
- `29`: Phase Bias
- `19`: Density Bias

Reference (bottom -> top MIDI order): `19, 29, 39, 49, 59, 69, 79, 89`

## LED Feedback

- Grid LEDs show held wells and active playheads.
- Top/right controls indicate mode and state.
- Start/Stop state is visible from top-row button color.
- Limit warning (too many held pads) is shown as a red flash on top controls.

## Transport and Timing

- ArpIso follows host transport and BPM via `time:Position`.
- With transport stopped, ArpIso does not advance playhead triggering.

## Build/Install (Manual)

```bash
cmake -S lv2/arpiso -B lv2/arpiso/build
cmake --build lv2/arpiso/build
cmake --install lv2/arpiso/build --prefix ~/.lv2
```

## Troubleshooting

- No LEDs:
  - Confirm Launchpad DAW routing to/from ArpIso control ports.
  - Reload plugin to resend Programmer Mode SysEx.
- No sound:
  - Confirm `midi_out` is routed to an instrument.
  - Confirm host transport is running.
- Unexpected note density:
  - Check cycle length, clock division, and density bias controls.
