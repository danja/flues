# Debugging PM-Synth UI in Reaper

The UI is working correctly in jalv.gtk3, but Reaper may be showing default controls. Here's how to debug and fix this issue.

## Changes Made

1. **Added comprehensive debug logging** to track UI initialization
2. **Added size request** (`gtk_widget_set_size_request(root, 800, 400)`) to ensure widget visibility
3. **Fixed widget parenting** - LV2 hosts handle parenting automatically

## Testing in Reaper

### Step 1: Launch Reaper from Terminal

To see the debug output, you MUST launch Reaper from a terminal:

```bash
# Close Reaper if it's running
killall reaper

# Launch from terminal to see stderr output
reaper 2>&1 | tee reaper-debug.log
```

This will display all debug messages in the terminal and save them to `reaper-debug.log`.

### Step 2: Load the Plugin

1. In Reaper, create a new track (Ctrl+T)
2. Click **FX** button on the track
3. Search for "Stove" or "PM-Synth"
4. Add the plugin

### Step 3: Check Debug Output

Look for these debug messages in the terminal:

```
[PM-Synth UI] lv2ui_descriptor called with index 0
[PM-Synth UI]   Returning descriptor for URI: https://danja.github.io/flues/plugins/pm-synth#ui
[PM-Synth UI] ui_instantiate called
[PM-Synth UI]   plugin_uri: https://danja.github.io/flues/plugins/pm-synth
[PM-Synth UI]   bundle_path: /home/danny/.lv2/pm-synth.lv2/
[PM-Synth UI] Available features:
[PM-Synth UI]   [0] http://lv2plug.in/ns/ext/urid#map
[PM-Synth UI]   [1] http://lv2plug.in/ns/extensions/ui#parent -> parent widget: 0x...
...
[PM-Synth UI] UI instantiation complete!
```

### What to Look For

**If you see the debug messages:**
- The UI is being loaded by Reaper
- Check if `ui#parent` feature is present and what the parent widget pointer is
- The UI should appear

**If you DON'T see any debug messages:**
- Reaper is not loading the custom UI at all
- This means Reaper doesn't support GTK3 UIs or has them disabled

## Possible Issues and Solutions

### Issue 1: Reaper Doesn't Support GTK3 UIs

**Check:** Look for Reaper preferences about LV2 UI handling

**Solution:** Reaper on Linux may only support X11 UIs or may have LV2 UI support disabled. Options:
1. Check Reaper preferences → Plug-ins → LV2 for UI-related settings
2. Try the `carla` host instead (known to support GTK UIs well)
3. Create an X11 native UI instead (more complex)

### Issue 2: UI Is Loaded But Not Visible

**Symptoms:** You see debug messages but no UI window

**Solutions:**
- Check if Reaper has a "Show plugin UI" or "UI visible" option
- Try right-clicking on the plugin slot for context menu options
- The size request (800x400) should make the UI visible

### Issue 3: Reaper Shows Generic Controls Only

**This is normal IF:** Reaper doesn't find or can't load the custom UI

**Check:**
```bash
reaper --help | grep -i lv2
reaper --version
```

## Testing with Carla (Alternative Host)

Carla is known to have excellent LV2 UI support:

```bash
# Launch carla
carla

# Or use carla-rack for a simpler interface
carla-rack
```

Then:
1. Click **Add Plugin**
2. Select **LV2** category
3. Find "Stove Synth"
4. Double-click to add
5. The custom UI should appear automatically

## Verify UI Binary

Check that the UI is properly installed:

```bash
ls -lh ~/.lv2/pm-synth.lv2/
# Should show: pm_synth_ui.so (around 46K)

lv2info https://danja.github.io/flues/plugins/pm-synth | grep -A 5 "UIs:"
# Should show the Gtk3UI declaration

ldd ~/.lv2/pm-synth.lv2/pm_synth_ui.so | grep gtk
# Should show GTK3 libraries are linked
```

## Next Steps Based on Results

### If Debug Messages Appear in Reaper
- Copy the debug output here and we can analyze what Reaper is doing
- Check if there are errors after "UI instantiation complete!"

### If NO Debug Messages Appear
- Reaper doesn't support GTK3 UIs (or has them disabled)
- Options:
  1. Use Carla or another LV2 host
  2. Create a generic X11 UI (more portable but more work)
  3. Use Reaper's generic parameter controls

### If Carla Works But Reaper Doesn't
- This confirms the UI itself is correct
- The issue is Reaper-specific
- May need to contact Reaper support about Linux LV2 UI support

## Reference: Working Test in jalv.gtk3

The UI works perfectly in jalv.gtk3:

```bash
jalv.gtk3 https://danja.github.io/flues/plugins/pm-synth 2>&1
```

Output shows successful initialization:
```
[PM-Synth UI] ui_instantiate called
[PM-Synth UI] Widget visible: 1
[PM-Synth UI] UI instantiation complete!
```

This proves the UI code is correct and functional.
