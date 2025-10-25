# Floozy Dev Polyphony Plan

Target: upgrade `lv2/floozy-dev` from the current single-voice implementation to an 8-note polyphonic instrument that stays real-time safe inside LV2 hosts.

## Goals & Constraints
- Maximum of 8 concurrent notes; graceful voice stealing when all voices are engaged.
- Maintain the existing signal chain character (Disyn source → PM modules → feedback → filter → modulation → reverb → master).
- No heap allocations, locks, or dynamic `new`/`delete` on the audio thread; pre-allocate everything during `instantiate`.
- Keep latency identical to the mono version and avoid denormals/popping when voices start/stop.
- Leave the public LV2 port layout unchanged so existing presets/session files continue to load.

## Step-by-step Plan

1. **Baseline Audit**
   - Review `lv2/floozy-dev/src/floozy_plugin.cpp`, `src/FloozyEngine.hpp`, and `src/modules/FloozySourceModule.hpp` to document which modules keep per-voice state (envelope, interface, delay lines, feedback, filter, modulation, reverb, random generator, DC blocker).
   - Capture current CPU cost per block to use as a regression target once polyphony lands.

2. **Refactor Engine Into Voice + Manager**
   - Split `FloozyEngine` (now mono) into two pieces:
     - `FloozyVoice` (new file) that contains the existing module instances and per-note state (`frequency`, `isPlaying`, DC blocker history, previous delay/filter samples, etc.).
     - `FloozyPolyEngine` (new file) that owns a fixed-size `std::array<FloozyVoice, kMaxVoices>` where `kMaxVoices = 8`, plus shared modules (global reverb send if desired).
   - Move all `setX()` parameter functions onto `FloozyPolyEngine`, forwarding them to each voice or to shared modules as appropriate.

3. **Introduce Global Parameter Cache**
   - Create a `struct FloozyParams` holding the 24 control values plus derived scalars (e.g., frequency coefficient). All setters update this struct and mark dirty flags.
   - During each audio block, the poly engine checks the dirty flags and pushes updates into voices that require them (e.g., modulation rate).
   - This avoids touching every voice when a knob hasn’t moved and keeps the UI responsive.

4. **Per-Voice DSP Flow**
   - Port the existing `process()` body from `FloozyEngine` into `FloozyVoice::process(const FloozyParams& params, float noteFrequency)`.
   - Ensure each voice owns its own envelope, interface module, delay lines, feedback, filter, modulation, reverb, RNG, and DC blocker state so independent notes don’t share state.
   - Add helper methods `start(note, frequency)` and `release(note)` that reset module state and gate the envelope without allocating memory.

5. **Global Mixing & Reverb Strategy**
   - Decide whether to keep a per-voice reverb (current behaviour) or move to a shared global reverb bus:
     - Preferred: voices output “dry” audio; `FloozyPolyEngine` sums all voices to `dryMix`, feeds a single `ReverbModule` (already heavy) with `dryMix * params.reverbLevel`, then blends wet/dry at the end. This keeps CPU bounded.
     - If keeping per-voice reverbs, ensure the sum is scaled by `1.0f / activeVoices` to avoid clipping.
   - Clamp the accumulated output using a soft limiter or scaling factor before writing to the LV2 audio buffer.

6. **Voice Allocation & MIDI Handling**
   - Replace `FloozyLV2::currentNote` with a `VoiceState[8]` table storing `note`, `isActive`, `isReleasing`, `ageCounter`, and a pointer/index into the voice array.
   - On NOTE ON:
     - Reuse an idle voice if available.
     - Otherwise choose a voice to steal (oldest `isReleasing`, then lowest amplitude, finally oldest active).
     - Call `voice.start(note, frequency)` and update the state table.
   - On NOTE OFF:
     - Find the matching voice, call `voice.release()`, mark it releasing, and keep rendering until `voice.isSilent()` returns true.
   - On ALL NOTES OFF/ALL SOUND OFF: iterate all voices and call `forceStop()` (hard reset).

7. **Render Loop Changes**
   - In `run()`, when iterating frames:
     - Call `polyEngine.processBlock(n_samples, outputBuffer)` which internally loops voices per sample and writes directly to the buffer.
     - Use interleaved loops that keep hot data in cache: iterate samples outermost, voices innermost to keep voice state in registers.
   - Ensure `apply_parameters()` is now O(number of changed controls) and invoked once per block before processing.

8. **Metadata & UI**
   - Update `lv2/floozy-dev/floozy-dev.lv2/floozy.ttl` description to mention 8-voice polyphony.
   - Optionally expose a “Poly Voices” control port (default 8) if runtime scaling is desired; if added, document the change and bump the LV2 minor version.
   - UI (`src/ui/floozy_ui_x11.c`) can remain unchanged unless adding a voice meter; note in README if behaviour differs.

9. **Performance Validation**
   - Add a new `ctest`/benchmark (or a simple standalone harness) that renders worst-case scenarios (8 sustained notes, high feedback) and measures CPU usage at 44.1/48 kHz.
   - Use `npx vitest run --pool vmThreads --maxWorkers=1` for JS code when touching shared modules, but prioritize a native benchmark for the LV2 path.
   - Test in a DAW with rapid chords to ensure no stuck notes or zippering when voice stealing occurs.

10. **Documentation & Release Prep**
    - Update `lv2/floozy-dev/README.md` with the new polyphony description, build/testing notes, and any new controls.
    - Note the change in `docs/CHANGELOG.md` (if present) and prepare new sound demos for GitHub Pages.

## Real-time Safety Checklist
- Pre-allocate voices and parameter buffers during `instantiate`.
- Avoid `std::vector::push_back` inside audio code; use fixed-size arrays.
- Keep RNG per voice to prevent contention.
- Flush denormals by zeroing envelopes below `1e-6f` and using `std::fmaf` where helpful.
- Profile with `perf`/`valgrind --tool=callgrind` to ensure the 8-voice version fits within the original CPU budget on a mid-tier CPU.

## Deliverables
1. Updated C++ sources implementing `FloozyPolyEngine`, `FloozyVoice`, and the modified LV2 glue.
2. Revised LV2 bundle metadata + README text noting 8-note polyphony.
3. Benchmark notes (numbers and methodology) captured in `docs/POLY-PLAN.md` or a sibling report once implemented.
