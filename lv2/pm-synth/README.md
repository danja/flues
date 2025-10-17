# Flues PM Synth LV2 Plugin

This LV2 plugin is a direct port of the `experiments/pm-synth` physical modelling synthesizer engine. It recreates the full Stove signal path – sources, interface strategies, dual delay lines, feedback routing, state-variable filter, LFO, and Schroeder reverb – inside a monophonic LV2 instrument.

## Building

```bash
cmake -S . -B build
cmake --build build
```

The build places `pm_synth.so`, `manifest.ttl`, and `pm-synth.ttl` inside `build/pm-synth.lv2/`.

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
└── pm_synth.so
```

Load “Flues PM Synth” in any LV2 host that provides MIDI input and sample-accurate event delivery.
