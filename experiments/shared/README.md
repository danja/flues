# Shared Modules

Reusable building blocks extracted for future browser synth projects. Existing apps (e.g. `pm-synth`) still reference their local implementations; migration can happen opportunistically once Disyn stabilises.

## Audio
- `audio/EnvelopeAR.js` – Attack/Release envelope (`EnvelopeAR`) with normalized/seconds setters.
- `audio/ReverbSchroeder.js` – Lightweight Schroeder reverb (`ReverbSchroeder`) with size & wet controls.

## UI
- `ui/KnobControl.js` – Pointer-friendly rotary knob (`KnobControl`) emitting normalized values.
- `ui/KeyboardInput.js` – Combined onscreen/computer keyboard handler (`KeyboardInput`) emitting MIDI-aware payloads.

## MIDI
- `midi/MidiInputManager.js` – Web MIDI input manager (`MidiInputManager`) with device tracking, channel filtering, and normalized velocity.

## Usage
Import modules directly or via `experiments/shared/index.js`:

```js
import { EnvelopeAR, KeyboardInput } from '../shared/index.js';
```

Each class includes minimal runtime logging and makes no assumptions about specific app structure. Future integration work:
- Update `pm-synth` to consume shared modules once Disyn implementation validates the API surface.
- Evaluate extracting visual styles alongside JS logic (current code expects host projects to supply CSS).

