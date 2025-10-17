# Flues PM Synth LV2 Plugin

This LV2 plugin is a direct port of the `experiments/pm-synth` physical modelling synthesizer engine. It recreates the full Stove signal path – sources, interface strategies, dual delay lines, feedback routing, state-variable filter, LFO, and Schroeder reverb – inside a monophonic LV2 instrument.

## Building

```bash
cmake -S . -B build
cmake --build build
```

The DSP plugin (`pm_synth.so`) and metadata (`manifest.ttl`, `pm-synth.ttl`) are emitted to `build/pm-synth.lv2/`.

### One-step helper

From the repository root you can run the helper script, which checks for required dependencies, configures, and builds the plugin (optionally cleaning or installing):

```bash
./build_pm_synth.sh --clean
./build_pm_synth.sh --install-default   # copies bundle to ~/.lv2 automatically
./build_pm_synth.sh --install ~/custom  # or choose a custom LV2 prefix
```

### Optional GTK UI

The tactile panel (`pm_synth_ui.so`) mirrors the Steampipe aesthetics with illuminated knobs. It depends on GTK+3 development headers:

```bash
sudo apt install libgtk-3-dev   # or equivalent on your platform
cmake -S . -B build -DGTK3_DIR=/path/if/needed
cmake --build build
```

When GTK is present the UI bundle is built alongside the DSP binary and installed into the same LV2 directory.

> ⚠️ If the configure step prints `GTK+3 development files not found; pm_synth_ui target will not be built.`, install the development package for your platform (for example `sudo apt install libgtk-3-dev` on Debian/Ubuntu, `sudo dnf install gtk3-devel` on Fedora, or `brew install gtk+3` on macOS with Homebrew), delete the previous CMake build directory, and rerun `cmake -S . -B build && cmake --build build`.

## Installing

Copy the bundle to your LV2 directory (commonly `~/.lv2` on Linux):

```bash
cmake --install build --prefix ~/.lv2
```

After installation, your LV2 path will contain:

```
~/.lv2/pm-synth.lv2/
├── manifest.ttl
├── pm-synth.ttl
├── pm_synth.so
└── pm_synth_ui.so   # optional, present when GTK UI is built
```

Load “Flues PM Synth” in any LV2 host that provides MIDI input and sample-accurate event delivery.
