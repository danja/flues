# Disyn Distortion Synth

Browser-based distortion synthesizer experiment building on the Flues shared UI/audio toolkit.

## Development

```bash
cd experiments/disyn
npm install
npm run dev
```

This starts a Vite dev server and opens the app at `http://localhost:5173/`.

## Build

```bash
npm run build
```

The production bundle is emitted to `experiments/disyn/dist`. Running `npm run ghp` from the repository root will include Disyn in the `www/disyn` GitHub Pages output.

## Tests

```bash
npm run test
```

Vitest runs algorithm registry checks with deterministic assertions. Expand coverage with audio math helpers and UI state logic as the engine matures.

## Architecture

- `src/audio` – on-main-thread engine coordination (`DisynEngine`) and AudioWorklet DSP (`disyn-worklet.js`).
- `src/audio/modules` – modular oscillator, envelope, and reverb helpers.
- `src/ui` – DOM-driven interface wiring reused across Flues experiments via shared controls.
- `src/styles.css` – dark theme visual styling; mobile-first layout.

The AudioWorklet processes a single monophonic voice combining:
- Distortion-driven oscillator algorithms (Dirichlet pulse, DSF, hyperbolic waveshaping, PAF, Modified FM).
- Shared attack/release envelope and Schroeder reverb helpers.
- Monophonic keyboard and MIDI input routing.

## TODO

- Flesh out oscillator algorithms with rigorous scaling vs. the reference formulas.
- Implement multi-voice polyphony and voice stealing.
- Add offline tests leveraging `OfflineAudioContext` for spectral assertions.
- Integrate PWA/service worker plumbing once the UI stabilises.

