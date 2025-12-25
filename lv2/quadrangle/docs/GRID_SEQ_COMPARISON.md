# Grid-Seq vs Quadrangle Comparison

## MIDI Handling Comparison

### ✅ Identical Patterns

Both plugins use the same MIDI handling approach:

#### 1. MIDI Data Access
**grid-seq** (src/grid_seq.c:444):
```c
const uint8_t* msg = (const uint8_t*)(ev + 1);
```

**quadrangle** (src/quadrangle_plugin.cpp:203):
```cpp
const uint8_t *midi = (const uint8_t*)(ev + 1);
```

#### 2. SysEx to Both Outputs
**grid-seq** (src/grid_seq.c:698-699):
```c
send_sysex_programmer_mode(gs, &gs->forge, true);  // Main MIDI output
send_sysex_programmer_mode(gs, &gs->launchpad_forge, true);  // Launchpad output
```

**quadrangle** (src/quadrangle_plugin.cpp:279-285):
```cpp
lv2_atom_forge_frame_time(&self->forge, 0);
lv2_atom_forge_atom(&self->forge, sizeof(sysex), self->urids.midi_Event);
lv2_atom_forge_write(&self->forge, sysex, sizeof(sysex));

lv2_atom_forge_frame_time(&self->launchpad_forge, 0);
lv2_atom_forge_atom(&self->launchpad_forge, sizeof(sysex), self->urids.midi_Event);
lv2_atom_forge_write(&self->launchpad_forge, sysex, sizeof(sysex));
```

#### 3. Port Structure
**grid-seq** (ttl/grid_seq.ttl:20-29):
```turtle
lv2:port [
    a lv2:InputPort , atom:AtomPort ;
    atom:bufferType atom:Sequence ;
    atom:supports midi:MidiEvent , time:Position ;
    lv2:index 0 ;
    lv2:symbol "midi_in" ;
    lv2:name "MIDI In" ;
    lv2:designation lv2:control
]
```

**quadrangle** (quadrangle.lv2/quadrangle.ttl:32-41):
```turtle
lv2:port [
    a lv2:InputPort , atom:AtomPort ;
    atom:bufferType atom:Sequence ;
    atom:supports midi:MidiEvent ,
                  time:Position ;
    lv2:index 0 ;
    lv2:symbol "control_in" ;
    lv2:name "Control In" ;
    lv2:designation lv2:control ;
]
```

#### 4. Note On Detection
**grid-seq** (src/grid_seq.c:446-447):
```c
// Note On (0x90)
if ((msg[0] & 0xF0) == 0x90 && msg[2] > 0) {
```

**quadrangle** (src/quadrangle_plugin.cpp:206):
```cpp
// Note On (0x90)
if ((midi[0] & 0xF0) == 0x90 && midi[2] > 0) {
```

#### 5. LED Updates
**grid-seq** (src/grid_seq.c:293-301):
```c
static void send_launchpad_led(GridSeq* gs, LV2_Atom_Forge* forge, uint8_t note, uint8_t color) {
    uint8_t msg[3] = {0x90, note, color};
    lv2_atom_forge_frame_time(forge, 0);
    lv2_atom_forge_atom(forge, 3, gs->midi_MidiEvent);
    lv2_atom_forge_write(forge, msg, 3);
}
```

**quadrangle** (src/quadrangle_plugin.cpp:309-312):
```cpp
uint8_t msg[3] = {0x90, note, color};
lv2_atom_forge_frame_time(&self->launchpad_forge, 0);
lv2_atom_forge_atom(&self->launchpad_forge, 3, self->urids.midi_Event);
lv2_atom_forge_write(&self->launchpad_forge, msg, 3);
```

## Conclusion

**The MIDI handling code is functionally identical.** Both plugins:
- Parse MIDI the same way
- Send SysEx to both outputs
- Declare transport support
- Handle Note On/Off events identically
- Update LEDs with individual Note On messages

Since grid-seq works with the Launchpad, the issue with quadrangle is almost certainly **MIDI routing configuration** in the host, not the code itself.

## Next Steps

1. **Test grid-seq** to confirm Launchpad works in the same environment
2. **Compare MIDI routing** between working grid-seq and non-working quadrangle
3. **Verify connections** in QjackCtl or Reaper MIDI routing panel
4. **Check console output** for both plugins to compare event flow

## Expected MIDI Routing

For both plugins, the host should route:
- **Launchpad DA out** → **Plugin Control In** (for pad presses)
- **Plugin Launchpad Control** → **Launchpad DA in** (for LED updates)
- **Plugin MIDI Out** → **DAW MIDI track** (for musical notes)
