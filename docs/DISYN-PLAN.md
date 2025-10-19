# Disyn Implementation Plan

## Objectives
- Deliver a browser-based distortion synthesizer (`experiments/disyn`) that follows the Flues ES module / Vite / Vitest conventions.
- Provide a focused engine built on `experiments/pm-synth` patterns with a new oscillator section implementing distortion synthesis algorithms from `docs/reference/distortion-synthesis.html`.
- Support monophonic keyboard play, Web MIDI input, attack/release envelope, Schroeder reverb, and deployable build artifacts under `www/disyn`.

## Target Deliverables
- Vite project scaffolding with `src/audio`, `src/ui`, `src/main.js`, AudioWorklet build path, Vitest config, and package scripts (`dev`, `build`, `test`).
- Audio engine: `DisynEngine`, `DisynProcessor`, `disyn-worklet.js`, oscillator module, envelope module, reverb module, MIDI/keyboard adapters.
- UI layer: knobs/switches reused or refactored from PM synth, algorithm selector, parameter controls, keyboard, MIDI status indicator, install prompt/button, audio unlock button.
- Documentation: README, algorithm reference table, parameter mapping, MIDI instructions.
- Automated tests covering oscillator math helpers, parameter mapping, envelope gating behaviour, MIDI event translation.

## Project Layout
```
experiments/disyn/
├── package.json          # type: module, vite/vitest scripts
├── vite.config.js
├── vitest.config.js
├── src/
│   ├── audio/
│   │   ├── DisynEngine.js
│   │   ├── DisynProcessor.js
│   │   ├── modules/
│   │   │   ├── OscillatorModule.js
│   │   │   ├── EnvelopeModule.js    # lightweight AR VCA (reuse from pm-synth with refactor if needed)
│   │   │   └── ReverbModule.js      # Schroeder implementation
│   │   ├── midi/
│   │   │   └── MIDIRouter.js
│   │   └── disyn-worklet.js         # AudioWorkletProcessor entry point
│   ├── ui/
│   │   ├── controls/                # knobs, selectors, keyboard
│   │   ├── AlgorithmPanel.js
│   │   ├── EnvelopePanel.js
│   │   ├── ReverbPanel.js
│   │   ├── KeyboardController.js
│   │   └── AppView.js
│   ├── main.js
│   ├── styles.css
│   └── service-worker.js (if reused)
├── index.html
└── tests/
    ├── oscillator.spec.js
    ├── envelope.spec.js
    └── midi.spec.js
```

If reuse of PM synth controls is desirable, perform a preliminary refactor to hoist generic UI/audio helpers into `experiments/shared/` to avoid duplication.

### Shared Utilities (new)
- `experiments/shared/audio/EnvelopeAR.js` – normalized AR envelope helper.
- `experiments/shared/audio/ReverbSchroeder.js` – Schroeder-style reverb core.
- `experiments/shared/ui/KnobControl.js` – generic knob interactions emitting normalized values plus absolute readouts.
- `experiments/shared/ui/KeyboardInput.js` – onscreen/computer keyboard handler returning MIDI metadata.
- `experiments/shared/midi/MidiInputManager.js` – Web MIDI wrapper with device management.
- Import barrel: `experiments/shared/index.js`.

## Audio Architecture
- **Engine orchestration (`DisynEngine`)**
  - Manages AudioContext lifecycle, instantiates AudioWorkletNode (`DisynProcessor`) with `new URL('./disyn-worklet.js', import.meta.url)`.
  - Maintains current algorithm selection, parameter state, envelope settings, reverb settings, and global volume.
  - Handles note-on/note-off events from keyboard and MIDI, translating pitch to frequency, maintaining monophonic behaviour with legato option.
- **AudioWorklet (`DisynProcessor` & `disyn-worklet.js`)**
  - Processor runs oscillator, envelope, and reverb DSP sample-by-sample.
  - Receives parameter updates via `port.postMessage` and AudioParam automation (for attack/release, mix).
  - Provides ScriptProcessor fallback mirroring pm-synth unlock helper for Safari/iOS.
- **Sample rate & oversampling**
  - Validate algorithms against aliasing; apply optional bandlimited guards (pre-warping, drive limiting) and consider simple 2× oversampling switch for high-drive algorithms.
- **Signal chain**
  - Oscillator output ➔ envelope VCA ➔ reverb send/return ➔ master gain ➔ context destination.
  - Envelope exposes Attack/Release (ms) mapped to exponential coefficients for the worklet.
  - Reverb module can reuse pm-synth Schroeder, parameterized by Room Size & Wet/Dry mix.

## Oscillator Algorithms
Implement algorithm catalogue with metadata to drive UI labels, parameter ranges, defaults, and DSP dispatch. Suggested initial set:

| Algorithm ID | Source | Param 1 | Param 2 | Implementation Notes |
|--------------|--------|---------|---------|----------------------|
| `bandLimitedPulse` | Dirichlet kernel (Doc §I) | Harmonic count (map 0‒1 → N=1‒64) | Low-pass tilt (map 0‒1 → exponent shaping) | Use guarded division, compute numerator/denominator per sample; optionally cache sine table or use direct `sin` for clarity. |
| `dsfSingleSided` | Moorer DSF (Doc §I) | Decay `a` (0.0‒0.98) | Inharmonic ratio `θ/ω` (0.5‒4× fundamental) | Implement normalisation Eq. (6); maintain stable `a` to avoid blow-up, clamp near 1. |
| `tanhSquare` | Hyperbolic waveshaping (Doc §II) | Drive/index (map to sinus amplitude) | Output trim (auto-scaling vs manual) | Evaluate tanh via built-in `Math.tanh`; pre-scale input sine, add anti-alias guard for high drive. |
| `tanhSaw` | Waveshaping + heterodyning (Doc §II) | Drive/index | Even-harmonic blend (0‒1 crossfade with cosine heterodyne) | Derive saw using square heterodyning Eq. (9); expose second parameter as blend between square & saw. |
| `paf` | Phase-Aligned Formant (Doc §IV) | Formant ratio (0.5‒6×) | Bandwidth (Hz mapped 50‒3000) | Precompute integer `n`, compute exponential decay `kg`; port article pseudo-code to JS. |
| `modFm` | Modified FM (Doc §V) | Modulation index `k` (0‒8) | Modulator ratio (0.25‒6×) | Implement Eq. (14) using lookup for exponential; reuse table to keep CPU low. |

Future extensions: add polynomial waveshaping, Chebyshev partial masks, user-drawn transfer functions.

### Parameter Handling
- Maintain algorithm definitions as data objects { id, name, params: [{ id, label, default, range, scaling, unit, mapToDSP(value) }], renderHints }.
- UI knobs produce normalized 0‒1 values; engine converts using algorithm-specific mapping before messaging worklet.
- Provide `AlgorithmRegistry` for lookups and to centralize documentation strings/tooltips.
- Persist last-used algorithm & params in `localStorage`.

## Envelope & Reverb Modules
- Envelope: adapt `experiments/pm-synth/src/audio/modules/EnvelopeModule.js` to a distilled Attack/Release version; support gating (trigger on note-on, release on note-off) and optional legato skip for attack.
- Reverb: port Schroeder implementation from PM synth with simplified controls (Room Size (comb delay scaling) + Wet Level). Expose UI as two knobs; keep CPU low for iOS.
- Handle wet/dry mix in engine node so reverb path can be bypassed when wet=0.

## UI & Interaction
- Reuse knob, switch, keyboard components from PM synth. If duplication occurs, extract to `experiments/shared/ui` with tree-shakable exports.
- Layout: top row algorithm selector & parameter knobs; middle row envelope/reverb; bottom keyboard & transport (power/unlock, install prompt, MIDI indicator).
- Implement responsive CSS using existing Vite CSS pipeline (postcss optional). Ensure controls remain finger-friendly on mobile.
- Include visual feedback (LED or text) for current MIDI device and note/velocity.
- Provide optional output scope (reuse visualizer) only if performance permits; otherwise leave for later milestone.

## Input & Power Management
- Keyboard: clone `KeyboardController` pattern from clarinet/pm synth (mouse & `AWSEDFTGYHUHJK`). Support transpose via arrow keys might be optional.
- MIDI: `MIDIRouter` to request `navigator.requestMIDIAccess`, map `noteon/noteoff` to engine. Provide fallback message if feature unavailable. Add UI toggle to arm/disarm MIDI.
- Audio unlock: call shared helper during first user gesture. Provide power button to suspend/resume AudioContext.

## App Shell & Platform Requirements
- Service worker registration deferred until `window.load`; paths relative to `/disyn/` for deployment in `www`.
- Use existing PWA prompt pattern (`beforeinstallprompt` capture, install button).
- Ensure AudioWorklet bundle path uses `new URL('./disyn-worklet.js', import.meta.url)` and copies worklet to `www/disyn/assets`.
- ScriptProcessor fallback for browsers without AudioWorklet (mirrors pm-synth fallback).
- iOS: ensure first-touch unlock also primes oscillator tables to prevent denormals (precompute sin/exponential tables on main thread).

## Testing Strategy
- **Unit tests (Vitest)**
  - Verify parameter mapping conversion functions for each algorithm.
  - Envelope coefficient calculations (attack/release to exponential multiplier).
  - Reverb delay tap generation deterministic tests (size parameter results).
  - MIDI router: note-on velocity scaling, channel filtering, sustain pedal ignore.
- **Integration tests**
  - Use `@vitest/browser` or happy-dom to simulate UI interactions (algorithm change updates state).
  - Snapshot test for algorithm registry metadata.
- **Audio sanity checks**
  - OfflineAudioContext renders for each algorithm at representative settings; assert RMS/headroom within bounds to catch explosions.
  - Frequency analysis (using FFT) to verify fundamental alignment for selected algorithms (optional but valuable).
- **Manual QA**
  - Desktop Chrome/Firefox, Safari desktop & iOS, Android Chrome. Validate MIDI on Chrome desktop.
  - Latency & CPU profiling for high-drive settings.

## Build & Deployment
- Add root `package.json` scripts to include `npm run build --workspace experiments/disyn` in `npm run ghp`.
- Update `www` build pipeline to copy `experiments/disyn/dist` into `www/disyn`.
- Include smoke test checklist before merging to `main`.

## Risks & Open Questions
- **CPU load**: Worklet must remain lightweight; consider caching sin/cos values or using vectorized approximations. Monitor mobile Safari performance.
- **Aliasing**: Distortion algorithms can alias heavily; may need optional oversampling or dynamic low-pass filtering. Evaluate per algorithm.
- **Parameter semantics**: Param1/Param2 meanings differ per algorithm; ensure UI communicates context (tooltips, inline labels). Consider dynamic label text.
- **Code sharing**: Decide whether to refactor pm-synth controls/modules into shared libs before or after Disyn MVP to avoid duplicated maintenance.
- **MIDI timing**: Web MIDI lacks sample-accurate scheduling; plan for simple event queue to smooth jitter if necessary.

## Implementation Order
1. Extract/shared groundwork: confirm reuse strategy for UI/audio helpers, create `experiments/shared` if needed.
2. Scaffold Vite project (package.json, Vite/Vitest configs, lint config, base index.html with root div).
3. Implement oscillator module + algorithm registry with unit tests (main-thread math first).
4. Build AudioWorklet pipeline (worklet processor, engine wiring, message protocol).
5. Port envelope and reverb modules, integrate into worklet signal flow.
6. Create UI (algorithm selector, knobs, envelope/reverb controls, keyboard) and connect to engine.
7. Add MIDI support, status UI, and manual power/unlock flow.
8. Implement PWA/service worker integration and deployment plumbing.
9. Finalize automated tests, run `npx vitest run --pool vmThreads --maxWorkers=1`.
10. Smoke-test across browsers, update docs, hook into `npm run ghp` deployment.
