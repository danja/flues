# Flues Disyn LV2 Plugin

This LV2 plugin is a direct port of the `experiments/disyn` distortion synthesizer. It provides 7 distortion synthesis algorithms in a monophonic LV2 instrument plugin.

## Algorithms

The plugin features seven distortion-based synthesis algorithms, each with two parameters:

1. **Dirichlet Pulse** - Band-limited pulse via Dirichlet kernel
   - Param 1: Harmonics (1-64)
   - Param 2: Tilt (-3 to +15 dB/oct)

2. **DSF Single-Sided** - Moorer discrete summation formula
   - Param 1: Decay (0-0.98)
   - Param 2: Ratio (0.5-4)

3. **DSF Double-Sided** - Symmetric sideband generation
   - Param 1: Decay (0-0.96)
   - Param 2: Ratio (0.5-4.5)

4. **Tanh Square** - Hyperbolic tangent waveshaping (default)
   - Param 1: Drive (0.05-5)
   - Param 2: Trim (0.2-1.2)

5. **Tanh Saw** - Square-to-saw transformation
   - Param 1: Drive (0.05-4.5)
   - Param 2: Blend (0-1)

6. **PAF** - Phase-Aligned Formant oscillator
   - Param 1: Formant (0.5-6 ×f0)
   - Param 2: Bandwidth (50-3000 Hz)

7. **Modified FM** - Modified FM synthesis with exponential modulator
   - Param 1: Index (0.01-8)
   - Param 2: Ratio (0.25-6)

## Signal Chain

```
MIDI Input → Voice Management → Algorithm Oscillator
  → Attack/Release Envelope → Schroeder Reverb → Master Gain → Audio Output
```

## Building

### Dependencies

- CMake (≥ 3.16)
- C++17 compiler
- LV2 development files

On Ubuntu/Debian:
```bash
sudo apt install build-essential cmake lv2-dev
```

### Build Steps

```bash
cd lv2/disyn
cmake -S . -B build
cmake --build build
```

The DSP plugin (`disyn.so`) and metadata (`manifest.ttl`, `disyn.ttl`) are generated in `build/`.

## Installing

Copy the bundle to your LV2 directory (commonly `~/.lv2` on Linux):

```bash
cmake --install build --prefix ~/.lv2
```

After installation, your LV2 path will contain:

```
~/.lv2/disyn.lv2/
├── manifest.ttl
├── disyn.ttl
└── disyn.so
```

## Testing

Verify the plugin is recognized:

```bash
lv2ls | grep disyn
# Should output: https://danja.github.io/flues/plugins/disyn

lv2info https://danja.github.io/flues/plugins/disyn
```

Load in an LV2 host:
- **Jalv**: `jalv.gtk https://danja.github.io/flues/plugins/disyn`
- **Ardour**: Plugins → Instrument → Flues Disyn
- **Carla**: Add Plugin → Instrument → Flues Disyn

## Parameters

| Port | Name | Range | Default | Description |
|------|------|-------|---------|-------------|
| Algorithm | Algorithm | 0-6 | 3 (Tanh Square) | Algorithm selector (enumeration) |
| Param 1 | Parameter 1 | 0-1 | 0.55 | Algorithm-specific parameter 1 (normalized) |
| Param 2 | Parameter 2 | 0-1 | 0.5 | Algorithm-specific parameter 2 (normalized) |
| Attack | Envelope Attack | 0-1 | 0.5 | Envelope attack time (0.001-1.0s) |
| Release | Envelope Release | 0-1 | 0.5 | Envelope release time (0.01-3.0s) |
| Size | Reverb Size | 0-1 | 0.5 | Reverb room size |
| Level | Reverb Level | 0-1 | 0.3 | Reverb wet/dry mix |
| Master | Master Gain | 0-1 | 0.8 | Output level |

**Note:** Parameters 1 and 2 are normalized (0-1) and internally mapped to algorithm-specific ranges. The meaning changes based on the selected algorithm.

## Usage

1. Load the plugin in your LV2 host
2. Connect MIDI input and audio output
3. Select an algorithm from the dropdown (default: Tanh Square)
4. Play MIDI notes (monophonic)
5. Adjust Param 1 and Param 2 to shape the sound
6. Tune envelope attack/release for desired articulation
7. Add reverb to taste

## Architecture

The plugin follows the established pattern from `lv2/pm-synth`:

- **Header-only modules** in `src/modules/`
  - `OscillatorModule.hpp` - Seven algorithm implementations
  - `EnvelopeModule.hpp` - AR envelope generator
  - `ReverbModule.hpp` - Schroeder reverb (4 comb + 2 allpass)
- **Main engine**: `DisynEngine.hpp` - Coordinates modules, voice management
- **Plugin glue**: `disyn_plugin.cpp` - LV2 interface, MIDI handling
- **Metadata**: `disyn.lv2/*.ttl` - Port definitions, plugin metadata

## Differences from JavaScript Version

The C++ port is a line-by-line translation of the JavaScript AudioWorklet code:

- **Identical algorithms** - Same math, same constants (TWO_PI, EPSILON)
- **Identical parameter mapping** - Same exponential/linear curves
- **Identical signal flow** - oscillator → envelope → reverb
- **Sample-accurate MIDI** - Events processed at exact sample positions
- **Voice tail detection** - Automatically stops processing when envelope finishes

## Performance

- **Real-time safe** - No allocations in audio callback
- **Low latency** - Direct synthesis, no look-ahead
- **CPU efficient** - Optimized C++ with minimal overhead
- **Monophonic** - Single voice for low CPU usage

## Known Limitations

- Monophonic only (no polyphony)
- No preset system (yet)
- No GUI (uses generic host controls)
- Parameter 1/2 labels don't change per algorithm (host limitation)

## Future Enhancements

- [ ] Polyphony (4-8 voices with voice stealing)
- [ ] GTK3 UI with algorithm-specific parameter labels
- [ ] LV2 state extension for preset save/load
- [ ] Pitch bend support
- [ ] Modulation wheel → parameter mapping
- [ ] Per-algorithm parameter value displays

## License

MIT License - See repository root for details.

## See Also

- Source: `experiments/disyn/` - JavaScript browser version
- Related: `lv2/pm-synth/` - Physical modeling synth LV2 plugin
- Documentation: `experiments/disyn/docs/DISYN-LV2.md` - Implementation plan
