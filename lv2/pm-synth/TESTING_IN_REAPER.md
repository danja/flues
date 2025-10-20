# Testing PM-Synth in Reaper - Debug Version

Both the DSP plugin (`pm_synth.so`) and UI (`pm_synth_ui.so`) now have comprehensive debug logging.

## What's New

### Version: v1.0.2-debug-2024-10-20

**Updated Files:**
- `pm_synth.so` - Built Oct 20 10:43 (379K) - DSP plugin with debug logging
- `pm_synth_ui.so` - Built Oct 20 10:36 (47K) - UI with debug logging

Both binaries now print banner messages when loaded, making it instantly clear which components Reaper is loading.

## Expected Debug Output (from jalv.gtk3)

```
========================================
[PM-Synth Plugin] DSP PLUGIN LOADED! v1.0.2-debug-2024-10-20
[PM-Synth Plugin] Time: Mon Oct 20 10:43:XX 2025
[PM-Synth Plugin] Binary: pm_synth.so
========================================

[PM-Synth Plugin] instantiate() called
[PM-Synth Plugin]   Sample rate: 48000.0 Hz
[PM-Synth Plugin]   Bundle path: /home/danny/.lv2/pm-synth.lv2/
[PM-Synth Plugin]   Engine created successfully
[PM-Synth Plugin] instantiate() complete! Instance: 0x...

========================================
[PM-Synth UI] LIBRARY LOADED! v1.0.2-debug-2024-10-20
[PM-Synth UI] Time: Mon Oct 20 10:37:06 2025
[PM-Synth UI] Binary: pm_synth_ui.so
[PM-Synth UI] GTK version: 3.24.41
========================================

[PM-Synth UI] lv2ui_descriptor called with index 0
[PM-Synth UI]   Returning descriptor for URI: https://danja.github.io/flues/plugins/pm-synth#ui
[PM-Synth UI] ui_instantiate called
[PM-Synth UI]   plugin_uri: https://danja.github.io/flues/plugins/pm-synth
[PM-Synth UI]   bundle_path: /home/danny/.lv2/pm-synth.lv2/
[PM-Synth UI]   descriptor URI: https://danja.github.io/flues/plugins/pm-synth#ui
[PM-Synth UI] Creating UI instance...
[PM-Synth UI] Available features:
[PM-Synth UI]   [0] http://lv2plug.in/ns/ext/urid#map
[PM-Synth UI]   [1] http://lv2plug.in/ns/ext/urid#unmap
[PM-Synth UI]   [2] http://lv2plug.in/ns/ext/instance-access
[PM-Synth UI]   [3] http://lv2plug.in/ns/ext/data-access
[PM-Synth UI]   [4] http://lv2plug.in/ns/ext/log#log
[PM-Synth UI]   [5] http://lv2plug.in/ns/extensions/ui#parent -> parent widget: 0x...
[PM-Synth UI]   [6] http://lv2plug.in/ns/ext/options#options
[PM-Synth UI]   [7] http://lv2plug.in/ns/extensions/ui#idleInterface
[PM-Synth UI]   [8] http://lv2plug.in/ns/extensions/ui#requestValue
[PM-Synth UI]   [9] http://lv2plug.in/ns/extensions/ui#portMap
[PM-Synth UI] Creating root widget...
[PM-Synth UI] Adding controls to groups...
[PM-Synth UI] Showing all widgets...
[PM-Synth UI] Widget pointer set to: 0x...
[PM-Synth UI] Widget visible: 1
[PM-Synth UI] Widget realized: 0
[PM-Synth UI] UI instantiation complete!
```

## Testing in Reaper

### Step 1: Launch Reaper from Terminal

```bash
# Kill any running Reaper instances
killall reaper

# Launch Reaper with debug output
cd ~
reaper 2>&1 | tee reaper-pm-synth-debug.log
```

### Step 2: Load the Plugin

1. Create a new track (Ctrl+T)
2. Click **FX** button
3. Search for "Stove" or "PM-Synth"
4. Add the plugin

### Step 3: Analyze the Output

Watch the terminal for debug messages. You'll see one of these scenarios:

## Scenario Analysis

### ✅ Scenario 1: Both Plugin and UI Load

**You see:**
```
[PM-Synth Plugin] DSP PLUGIN LOADED!
[PM-Synth Plugin] instantiate() called
[PM-Synth UI] LIBRARY LOADED!
[PM-Synth UI] ui_instantiate called
[PM-Synth UI] UI instantiation complete!
```

**Meaning:** Reaper loaded both the DSP and UI successfully. If you still see generic controls:
- The UI widget might not be displaying correctly
- Check for any GTK/SWELL compatibility issues in the log
- The parent widget integration might need adjustment

### ⚠️ Scenario 2: Plugin Loads, UI Doesn't

**You see:**
```
[PM-Synth Plugin] DSP PLUGIN LOADED!
[PM-Synth Plugin] instantiate() called
```

**But NO UI messages**

**Meaning:** Reaper is loading the DSP plugin but NOT attempting to load the custom UI at all. This means:
- Reaper doesn't support GTK3 UIs (or has them disabled)
- Reaper is choosing to use generic controls instead
- The UI declaration in the TTL might not be recognized

**Solutions:**
1. Check Reaper preferences for LV2 UI settings
2. Use Carla or another LV2 host that supports GTK UIs
3. Consider creating a generic X11 UI (more portable)

### ❌ Scenario 3: Nothing Loads

**You see:** No PM-Synth messages at all

**Meaning:** Reaper isn't loading the plugin at all
- Plugin might not be in Reaper's LV2 scan path
- Plugin might have failed validation
- Check Reaper's plugin list/rescan

### 🔍 Scenario 4: UI Loads But Errors

**You see:**
```
[PM-Synth UI] LIBRARY LOADED!
[PM-Synth UI] ui_instantiate called
ERROR: <some GTK or widget error>
```

**Meaning:** Reaper is trying to load the UI but encountering errors
- Look for the specific error message
- May be SWELL/GTK compatibility issues
- Parent widget handling might need adjustment

## What Each Debug Message Means

| Message | Meaning |
|---------|---------|
| `DSP PLUGIN LOADED!` | The .so was loaded into memory (instant) |
| `instantiate() called` | Host is creating a plugin instance |
| `instantiate() complete!` | Plugin initialized successfully |
| `LIBRARY LOADED!` (UI) | The UI .so was loaded into memory (instant) |
| `lv2ui_descriptor called` | Host is querying for UI descriptor |
| `ui_instantiate called` | Host is creating the UI |
| `Available features:` | Shows what the host provides to the UI |
| `UI instantiation complete!` | UI widget created successfully |

## Key Information to Share

If you share the debug log, these are the most important parts:

1. **Both LOADED banners** - Confirms Reaper loaded the libraries
2. **Available features list** - Shows what Reaper provides to the UI
3. **Any ERROR messages** - Critical for debugging issues
4. **Parent widget pointer** - Shows if Reaper provides a parent widget

## Reaper and SWELL Notes

You mentioned Reaper supports GTK3 via libSWELL. SWELL is Reaper's cross-platform UI abstraction layer:

- On Linux, SWELL uses GTK3 internally
- SWELL translates between LV2 UI extensions and its own system
- Some GTK features might not work identically through SWELL
- The parent widget handling might be SWELL-specific

If Reaper loads the UI but doesn't display it, we may need to adjust the widget handling for SWELL compatibility.

## Next Steps Based on Results

After running Reaper from the terminal and loading the plugin:

1. **Copy all PM-Synth debug output** from the terminal
2. **Note whether you see generic controls or custom UI**
3. **Share the debug output** and we can analyze exactly what's happening

The debug logging will tell us:
- ✅ What Reaper IS doing
- ❌ What Reaper is NOT doing
- 🔍 Where things might be going wrong

Ready to test!
