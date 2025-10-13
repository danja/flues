# Clarinet Synthesizer

A digital waveguide physical modeling synthesizer that simulates clarinet acoustics.

## Features

- **Physical Modeling**: Karplus-Strong style delay line synthesis
- **Nonlinear Reed Model**: Realistic reed behavior simulation
- **Real-time Control**: 8 synthesis parameters with rotary knobs
- **Cross-platform**: Works on desktop and mobile browsers
- **Modern Audio API**: Uses AudioWorklet for better performance (with ScriptProcessor fallback)

## Architecture

- `src/audio/ClarinetEngine.js` - Core DSP synthesis engine
- `src/audio/clarinet-worklet.js` - AudioWorklet processor for efficient audio processing
- `src/audio/ClarinetProcessor.js` - Web Audio API interface with worklet/fallback support
- `src/ui/KnobController.js` - Rotary knob controls
- `src/ui/KeyboardController.js` - Musical keyboard interface
- `src/ui/Visualizer.js` - Waveform visualization
- `src/main.js` - Application coordination

## Development

```bash
npm install
npm run dev
```

Visit `http://localhost:5173/`

## Build

```bash
npm run build
```

Output will be in `dist/` directory.

## Controls

### Reed & Excitation
- **Breath**: Air pressure intensity (70-100% recommended)
- **Reed Stiffness**: Reed mechanical stiffness
- **Noise**: Breath turbulence noise level
- **Attack**: Note onset speed

### Bore & Resonance
- **Damping**: High-frequency damping in the bore
- **Brightness**: Harmonic content
- **Vibrato**: Pitch modulation depth
- **Release**: Note decay time

### Keyboard
- Click/touch the visual keys
- Or use computer keyboard: A W S E D F T G Y H U J K
- Maps to notes C4 through C5

## Technical Notes

- Uses ES modules throughout
- AudioWorklet provides low-latency audio on modern browsers
- Falls back to ScriptProcessorNode on older browsers
- Relative paths ensure compatibility with GitHub Pages deployment
- Power button shows "PWR" for maximum cross-browser/device compatibility

## Browser Compatibility

- Chrome/Edge: Full AudioWorklet support
- Firefox: Full AudioWorklet support
- Safari: Full AudioWorklet support (iOS 14.5+)
- Older browsers: ScriptProcessor fallback

Note: Audio requires user interaction before playback (Web Audio API requirement).
