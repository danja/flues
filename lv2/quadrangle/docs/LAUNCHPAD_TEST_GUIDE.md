# Launchpad MIDI Routing Test Guide

## Current Issue

Quadrangle plugin loads successfully but:
- ✅ Receives transport events (time:Position)
- ❌ Does NOT receive MIDI events from Launchpad pads
- ❌ Launchpad LEDs don't update

Console shows: `quadrangle: Event 1: Object/Blank (transport)` flooding
Console does NOT show: `quadrangle: Received Note On - note=XX`

## Hypothesis

Based on code comparison with grid-seq, the issue is **MIDI routing**, not code.

## Test 1: Verify Launchpad Hardware

```bash
# Check if Launchpad is connected
aconnect -l

# Expected output should include:
# client XX: 'Launchpad Mini MK3' [type=kernel,card=X]
#     0 'Launchpad Mini MK3 MI'
#     1 'Launchpad Mini MK3 DAW '  # <-- This is the port we need
```

## Test 2: Test grid-seq (Known Working)

### In jalv (Command Line)

```bash
# Terminal 1: Start JACK (if not running)
qjackctl &

# Terminal 2: Launch grid-seq
jalv http://github.com/danny/grid-seq

# Watch console for:
# "grid-seq: Sending SysEx to ENTER Programmer Mode"
# "grid-seq: Sent Programmer Mode SysEx to both outputs"
```

### In Reaper

1. **Insert grid-seq** on a MIDI track
2. **Open Routing Panel** (Reaper → View → Routing Matrix)
3. **Connect MIDI:**
   - Launchpad DA → grid-seq MIDI In
   - grid-seq Launchpad Control → Launchpad DA
4. **Watch console** for MIDI events when pressing pads
5. **Check LEDs** - should light up in green/yellow

### Expected Console Output (grid-seq working)

```
grid-seq: Sending SysEx to ENTER Programmer Mode
grid-seq: Sent Programmer Mode SysEx to both outputs
grid-seq: Received Note On - note=11, checking if grid button
grid-seq: Launchpad pad pressed - MIDI note=11 -> grid x=0 y=0
  -> Toggling grid[0][0], page=0, pitch_offset=0, new_value=1
```

## Test 3: Test quadrangle (Currently Not Working)

### In jalv

```bash
jalv https://danja.github.io/flues/plugins/quadrangle
```

### In Reaper

1. **Insert quadrangle** on a MIDI track
2. **Open Routing Panel**
3. **Connect MIDI** (same as grid-seq):
   - Launchpad DA → quadrangle Control In
   - quadrangle Launchpad Control → Launchpad DA
4. **Watch console** - should see similar output to grid-seq

### Expected Console Output (if working)

```
quadrangle: First run() call - plugin is active
quadrangle: Initializing Launchpad...
quadrangle: Sending Programmer Mode SysEx: F0 00 20 29 02 0D 0E 01 F7
quadrangle: Sent Programmer Mode to BOTH outputs
quadrangle: Received Note On - note=11
quadrangle: Pad press at row=0, col=0, vel=96
```

### Current Console Output (not working)

```
quadrangle: First run() call - plugin is active
quadrangle: Initializing Launchpad...
quadrangle: Sending Programmer Mode SysEx: F0 00 20 29 02 0D 0E 01 F7
quadrangle: Sent Programmer Mode to BOTH outputs
quadrangle: Event 1: Object/Blank (transport)  # <-- Only transport, no MIDI!
quadrangle: WARNING - No MIDI events received yet. Check MIDI routing!
```

## Test 4: MIDI Monitor

Use a MIDI monitor to see what's actually being sent:

### Using ALSA MIDI Monitor

```bash
# Install if needed
sudo apt install aseqdump

# Monitor Launchpad output
aseqdump -p "Launchpad Mini MK3 DAW"

# Press pads and watch for:
# Receiving Note On: channel 0, note 11, velocity 127
```

### Using QjackCtl MIDI Monitor

1. Open QjackCtl
2. Click **Patchbay** button
3. Select **ALSA** tab
4. Connect Launchpad DA to a MIDI monitor port

## Test 5: Check JACK/ALSA Connections

```bash
# List ALSA MIDI connections
aconnect -l

# List JACK MIDI connections (if using JACK)
jack_lsp -c | grep -i midi
```

## Debugging Checklist

### For Reaper

- [ ] MIDI track armed for recording?
- [ ] Routing Matrix shows connections?
- [ ] Console showing any MIDI at all?
- [ ] Try **View → Monitoring FX → MIDI Monitor** on track

### For jalv

- [ ] QjackCtl running?
- [ ] Connections tab shows Launchpad?
- [ ] Try `jalv -d` for debug output
- [ ] Check jalv console for errors

## Expected Differences

| Behavior | grid-seq | quadrangle |
|----------|----------|------------|
| Loads in DAW | ✅ Yes | ✅ Yes |
| Receives transport | ✅ Yes | ✅ Yes |
| Receives MIDI pads | ✅ Yes | ❌ **NO** |
| Sends SysEx | ✅ Yes | ✅ Yes (code) |
| LEDs update | ✅ Yes | ❌ **NO** |

## If grid-seq Works and quadrangle Doesn't

This would be **very strange** because the code is identical. Possible causes:

1. **Plugin state** - Try deleting Reaper project state and recreating
2. **Port naming** - Reaper might cache port connections by name
3. **Build issue** - Rebuild quadrangle from scratch
4. **LV2 cache** - Clear LV2 cache: `rm -rf ~/.lv2/quadrangle.lv2 && cmake --install build`

## If Neither Works

Then the issue is MIDI routing in the host:

1. Check **Reaper MIDI preferences**
2. Verify **Launchpad is in DAW mode** (not standalone)
3. Test with **plain MIDI monitor** first
4. Try **different host** (Ardour, Carla, etc.)

## Success Criteria

✅ Console shows: `Received Note On - note=XX`
✅ LEDs change color when pads pressed
✅ Grid state updates in UI (if visible)
