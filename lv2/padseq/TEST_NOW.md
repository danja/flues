# Quick Test Commands

## Grid-seq is Now Installed ✅

```bash
lv2ls | grep grid-seq
# http://github.com/danny/grid-seq
```

## Test grid-seq in Reaper

1. **Insert Plugin:**
   - Track → Insert virtual instrument on new track
   - Search for "Grid Sequencer (Danny)"
   - Insert it

2. **Connect MIDI:**
   - View → Routing Matrix
   - Find "Launchpad Mini MK3 DAW"
   - Connect: Launchpad DA → grid-seq MIDI In
   - Connect: grid-seq Launchpad Control → Launchpad DA

3. **Watch Console:**
   ```bash
   # Open terminal, watch for:
   journalctl -f | grep grid-seq
   # Or if console shows in terminal:
   # Look for "grid-seq: Sending SysEx"
   ```

4. **Press Pads:**
   - Should see: "grid-seq: Launchpad pad pressed"
   - LEDs should light up green
   - Current step should be yellow

## Test quadrangle in Reaper (Compare)

1. **On another track, insert quadrangle**
2. **Same MIDI routing:**
   - Launchpad DA → quadrangle Control In
   - quadrangle Launchpad Control → Launchpad DA
3. **Watch console - compare output**

## Quick MIDI Monitor

```bash
# See what Launchpad is sending
aseqdump -p "Launchpad Mini MK3 DAW"

# Press pads, should see:
# Note on 0, note 11, velocity 127
# Note off 0, note 11, velocity 0
```

## If grid-seq Works

→ MIDI routing is fine
→ Quadrangle has a bug (very unlikely, code is identical)
→ Try rebuilding quadrangle

## If grid-seq Doesn't Work

→ MIDI routing issue
→ Check Launchpad is in DAW mode
→ Verify connections in routing matrix

## Next Command

Start testing with:
```bash
# Open Reaper, insert grid-seq plugin, connect MIDI, press pads
```
