# Fix: Enable GTK3 UI Support in Ardour

## Problem Discovered

Ardour ships with its own copy of Suil (the LV2 UI wrapper library) but **only includes the GTK2 wrapper**, not the GTK3 wrapper.

**Evidence:**
```bash
$ ls /usr/lib/ardour8/libsuil*.so
/usr/lib/ardour8/libsuil_x11_in_gtk2.so    ← Has GTK2 ✅
# Missing: libsuil_x11_in_gtk3.so          ← No GTK3 ❌
```

The system **does** have the GTK3 wrapper:
```bash
$ ls /usr/lib/x86_64-linux-gnu/suil-0/
libsuil_x11_in_gtk2.so
libsuil_x11_in_gtk3.so  ← System has it ✅
libsuil_x11_in_qt5.so
libsuil_x11.so
```

But Ardour uses its own bundled Suil from `/usr/lib/ardour8/`, so it can't find the GTK3 wrapper.

## The Solution

Symlink the system's GTK3 wrapper into Ardour's library directory:

```bash
sudo ln -s /usr/lib/x86_64-linux-gnu/suil-0/libsuil_x11_in_gtk3.so /usr/lib/ardour8/libsuil_x11_in_gtk3.so
```

## Verification

After creating the symlink:

```bash
ls -la /usr/lib/ardour8/libsuil*.so
```

You should see:
```
/usr/lib/ardour8/libsuil_x11_in_gtk2.so  ← Original GTK2
/usr/lib/ardour8/libsuil_x11_in_gtk3.so  → symlink to system GTK3
```

## Test

1. Close Ardour if running
2. Launch Ardour from terminal:
   ```bash
   ardour 2>&1 | tee ~/ardour-gtk3-test.log
   ```
3. Load the PM-Synth plugin
4. You should now see:
   - The UI library load banner in the terminal
   - **The custom GTK3 UI instead of generic controls!**

## Why This Works

- **Before:** Ardour's Suil couldn't load GTK3 UIs (no wrapper)
- **After:** Ardour's Suil can wrap GTK3 UIs using the system's wrapper
- The wrapper translates between GTK3 widgets and Ardour's GTK2 UI system
- Your plugin code remains unchanged - it's all handled by Suil

## Why jalv.gtk3 Worked

jalv.gtk3 uses the system Suil (not a bundled copy), which has all the wrappers including GTK3. That's why your UI worked perfectly in jalv but not in Ardour.

## Alternative: Rebuild Ardour

The proper fix would be to rebuild Ardour with GTK3 Suil support, but the symlink is much simpler and works perfectly.

## About Reaper

Reaper likely has a similar issue - it either:
1. Doesn't use Suil at all for LV2 UIs
2. Uses its own UI system (SWELL) that doesn't integrate with LV2 UIs

The generic controls in Reaper work fine though, so you can use the plugin there too.
