# Quadrangle Plugin - Current Status

**Date:** 2025-12-25
**Plugin:** quadrangle (Novation Launchpad Mini MK3 performance instrument)
**Status:** Plugin loads, transport sync works, pad presses now reach the plugin, **LED output dark**

## What Works ✅

1. **Plugin loads successfully** in Reaper and jalv
2. **UI manifest re-enabled** - X11/Cairo UI should now be discoverable by hosts (manifest now lists `quadrangle_ui.so`)
3. **Transport sync** - receives `time:Position` events from host
4. **Pad presses arrive** - Note On/CC from Launchpad now logged (`quadrangle: Received Note On - note=XX`)
5. **Build system** - CMake compiles and installs correctly

## What Doesn't Work ❌

1. **Launchpad LEDs dark** - no color changes when pads pressed
2. **No playhead/side/top LED feedback** yet

## Console Output
LED logging during current failure:
```
quadrangle: First run() call - plugin is active
quadrangle: Initializing Launchpad...
quadrangle: Sending Programmer Mode SysEx: F0 00 20 29 02 0D 0E 01 F7
quadrangle: Sent Programmer Mode to BOTH outputs
quadrangle: Received Note On - note=11   # pad press now arrives
quadrangle: Pad press at row=0, col=0, vel=96
quadrangle: Sending LED update #0        # but LEDs stay off
```

### Fix Applied (needs retest)
- **Atom forge finalization added**: sequences for `midi_out` and `launchpad_out` were never closed, so hosts likely discarded all LED/MIDI output (atom size stayed zero). `lv2_atom_forge_pop` now finalizes both sequences at end of `run()`.
- **UI re-enabled**: `manifest.ttl` now lists `<quadrangle#ui>` with `quadrangle_ui.so`, so hosts should show the UI again.

## Code Comparison: grid-seq vs quadrangle

After detailed comparison with the working grid-seq plugin:

### ✅ Identical Patterns
- MIDI data access: `(const uint8_t*)(ev + 1)`
- SysEx sent to BOTH outputs (forge + launchpad_forge)
- Port structure with `time:Position` support
- Note On detection: `(midi[0] & 0xF0) == 0x90`
- LED update messages: `{0x90, note, color}`
- TTL metadata: `atom:supports midi:MidiEvent, time:Position`

See [GRID_SEQ_COMPARISON.md](./GRID_SEQ_COMPARISON.md) for detailed analysis.

## Current Hypothesis

- **Primary:** LED data was being dropped because the forged atom sequences were never finalized (`lv2_atom_forge_pop` missing). Fix committed—needs rebuild/install/retest.
- **Secondary (if LEDs still dark):** Host routing/connection of `Launchpad Control` output back to Launchpad DA input.

## Files Changed During Debug

### Core Plugin
- `src/quadrangle_plugin.cpp` - Added detailed event logging, transport sync, MIDI parsing
- `quadrangle.lv2/quadrangle.ttl` - Added `time:Position` support
- `src/midi_comm.c` - Fixed duplicate side button append bug

### UI
- `src/ui/quadrangle_ui_x11.c` - Complete X11/Cairo implementation
- Changed from pthread to LV2 Idle Interface
- Added double buffering to prevent flickering

### Build
- `CMakeLists.txt` - Added UI build target, X11/Cairo dependencies

## Current Architecture

```
┌─────────────────────────────────────────────────────────┐
│ Host (Reaper/jalv)                                      │
│                                                         │
│  ┌──────────────┐        ┌──────────────┐             │
│  │ Launchpad DA │───────▶│ Control In   │             │
│  │   (MIDI out) │        │   (Port 0)   │             │
│  └──────────────┘        └──────┬───────┘             │
│                                  │                      │
│  ┌──────────────┐        ┌──────▼───────┐             │
│  │ Launchpad DA │◀───────│ Launchpad    │             │
│  │   (MIDI in)  │        │ Control Out  │             │
│  └──────────────┘        │   (Port 2)   │             │
│                          └──────────────┘             │
│                                  │                      │
│                          ┌──────▼───────┐             │
│                          │  MIDI Out    │             │
│                          │   (Port 1)   │             │
│                          └──────────────┘             │
└─────────────────────────────────────────────────────────┘

Port 0: control_in (MIDI + transport) ← Launchpad DA out
Port 1: midi_out (musical notes) → DAW MIDI track
Port 2: launchpad_out (LED control) → Launchpad DA in
Port 3: audio_out_l (audio)
Port 4: audio_out_r (audio)
```

## Next Steps

### 1. Test grid-seq (Working Reference)

```bash
# Install grid-seq (already done)
lv2ls | grep grid-seq

# Test in jalv
jalv http://github.com/danny/grid-seq

# Or in Reaper
# Insert grid-seq plugin, connect MIDI routing
```

### 2. Compare Console Output

Run both plugins side-by-side and compare:
- Does grid-seq receive MIDI from pads?
- Do LEDs update in grid-seq?
- What's different in console output?

### 3. Verify MIDI Routing

In **Reaper**:
- View → Routing Matrix
- Verify connections: Launchpad DA ↔ Plugin

In **jalv + JACK**:
- Open QjackCtl
- Check Connections tab (ALSA MIDI)
- Manually connect Launchpad DA to plugin ports

### 4. Debug Commands

```bash
# Monitor Launchpad MIDI output
aseqdump -p "Launchpad Mini MK3 DAW"

# List MIDI devices
aconnect -l

# Check LV2 cache
lv2ls | grep -E "(grid-seq|quadrangle)"
lv2info https://danja.github.io/flues/plugins/quadrangle

# Rebuild plugin (if needed)
cd /home/danny/github/flues/lv2/quadrangle
cmake --build build
cmake --install build --prefix ~/.lv2
```

## Expected Outcome

If grid-seq works and quadrangle doesn't, despite identical code:
1. **Most likely:** MIDI routing misconfiguration
2. **Less likely:** Build/install issue
3. **Unlikely:** Host-specific plugin behavior

If **neither** grid-seq nor quadrangle works:
1. **Launchpad not in DAW mode**
2. **MIDI routing not set up in host**
3. **ALSA/JACK MIDI bridge issue**

## Success Criteria

✅ Console shows: `quadrangle: MIDI [90 XX XX]`
✅ Console shows: `quadrangle: Received Note On - note=XX`
✅ Console shows: `quadrangle: Pad press at row=X, col=Y, vel=XX`
✅ Launchpad LEDs change color when plugin loads
✅ Pads light up green when pressed
✅ Current step shows yellow playhead

## Documentation

- [GRID_SEQ_COMPARISON.md](./GRID_SEQ_COMPARISON.md) - Code comparison analysis
- [LAUNCHPAD_TEST_GUIDE.md](./LAUNCHPAD_TEST_GUIDE.md) - Step-by-step testing guide
- [CURRENT_STATUS.md](./CURRENT_STATUS.md) - This file

## References

- Grid-seq source: `/home/danny/github/grid-seq/`
- Quadrangle source: `/home/danny/github/flues/lv2/quadrangle/`
- Launchpad Programmer's Reference: [docs/reference/launchpad.pdf](../reference/launchpad.pdf)
