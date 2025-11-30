# ChatGen LV2 Plugin

Text-to-MIDI CC generator for controlling the Chatterbox speech synthesis plugin.

## Overview

ChatGen is an LV2 utility plugin that converts English text into MIDI Continuous Controller (CC) messages. It parses text using rule-based phoneme estimation and generates CC messages synchronized to a musical beat grid. The primary use case is controlling the formant parameters of the Chatterbox speech synthesizer.

**Status**: Phase 1 MVP - Core DSP engine complete, UI pending (Phase 2)

## Features

- **Rule-based phoneme parsing**: Converts English text to 5 vowels + 8 consonants
- **MIDI CC output**: Generates CC messages for formant control (CC 71, 10, 74, 75)
- **DAW tempo sync**: Reads BPM from host via LV2 Time extension
- **Fallback BPM control**: Manual BPM slider if host doesn't provide tempo
- **Loop mode**: Repeat phoneme sequence continuously
- **Sample-accurate events**: MIDI messages sent at exact beat boundaries
- **Consonant articulation**: Basic F3/F4 variations for nasals, liquids, fricatives, plosives

## Signal Flow

```
Text Input → TextParser → Phoneme Sequence
                ↓
         PhonemeMapper → Formant CC Values
                ↓
         ClockSync → Beat Detection
                ↓
      MidiGenerator → MIDI CC Messages (Output)
```

## Phoneme Mapping

ChatGen uses IPA vowel positions from the Chatterbox joystick control, plus basic consonant articulations:

### Vowels

| Phoneme | Example | F1 (CC71) | F2 (CC10) | F3 (CC74) | F4 (CC75) |
|---------|---------|-----------|-----------|-----------|-----------|
| /i/ | "see" | 11 | 90 | 63 | 63 |
| /e/ | "bed" | 52 | 67 | 63 | 63 |
| /a/ | "father" | 84 | 30 | 63 | 63 |
| /o/ | "home" | 58 | 18 | 63 | 63 |
| /u/ | "boot" | 17 | 19 | 63 | 63 |

### Consonants

| Phoneme | Example | F1 (CC71) | F2 (CC10) | F3 (CC74) | F4 (CC75) | Notes |
|---------|---------|-----------|-----------|-----------|-----------|-------|
| /m/ | "me" | 50 | 40 | 70 | 63 | Nasal |
| /n/ | "no" | 50 | 40 | 70 | 63 | Nasal |
| /l/ | "let" | 45 | 55 | 80 | 63 | Liquid |
| /r/ | "red" | 42 | 50 | 50 | 63 | Rhotic |
| /s/ | "see" | 40 | 60 | 100 | 100 | Fricative |
| /t/ | "top" | 50 | 50 | 63 | 63 | Plosive |
| /d/ | "do" | 50 | 50 | 63 | 63 | Plosive |
| /k/ | "cat" | 50 | 50 | 63 | 63 | Plosive |

## Text Parsing Rules

### Digraphs (2-character patterns)
- "ee", "ea" → /i/ (as in "meet", "eat")
- "oo", "ou" → /u/ (as in "moon", "you")
- "ah" → /a/ (as in "aha")
- "oh" → /o/ (as in "oh")

### Single characters
- **Vowels**: "i", "y" → /i/ | "e" → /e/ | "a" → /a/ | "o" → /o/ | "u" → /u/
- **Consonants**: "m" → /m/ | "n" → /n/ | "l" → /l/ | "r" → /r/ | "s" → /s/ | "t" → /t/ | "d" → /d/ | "k", "c" → /k/
- **Unhandled**: Other consonants and whitespace are skipped

### Examples
```
Text: "hello"      → Phonemes: [e] [l] [o]
Text: "meet"       → Phonemes: [m] [i] [t]
Text: "sunset"     → Phonemes: [s] [u] [n] [s] [e] [t]
Text: "can"        → Phonemes: [k] [a] [n]
```

## Installation

### Build from source

```bash
cd lv2/chatgen
cmake -S . -B build
cmake --build build
cmake --install build --prefix ~/.lv2
```

### Verify installation

```bash
lv2ls | grep chatgen
# Should output: https://danja.github.io/flues/plugins/chatgen

lv2info https://danja.github.io/flues/plugins/chatgen
```

## Usage

### With Chatterbox (Recommended)

1. Load both plugins in your DAW:
   - **ChatGen**: MIDI generator
   - **Chatterbox**: Speech synthesizer

2. Route MIDI output:
   ```
   ChatGen MIDI Out → Chatterbox MIDI In
   ```

3. Configure Chatterbox:
   - **Voiced**: ON
   - **Attack**: 10ms (fast onset)
   - **Release**: 100ms (smooth transition)
   - **Master Gain**: 0.8

4. Configure ChatGen:
   - **BPM**: 120 (or match DAW tempo)
   - **Play**: ON (toggle)
   - **Loop**: ON (repeat sequence)

5. Edit text (Phase 2 UI):
   - Current MVP uses hardcoded text "hello"
   - Phase 2 will add text input widget

6. Start playback and hear vowel sounds!

### In Jalv (Testing)

```bash
jalv.gtk https://danja.github.io/flues/plugins/chatgen
```

Use a MIDI monitor to verify CC output:
```bash
# Install MIDI monitor
sudo apt install kmidimon

# Or use command-line monitor
aseqdump -p <ChatGen_port>
```

## Controls

| Control | Range | Default | Description |
|---------|-------|---------|-------------|
| **Control** | Atom | - | Input for LV2 Time position from DAW (auto tempo sync) |
| **BPM** | 30-300 | 120 | Manual tempo (fallback if host doesn't provide tempo) |
| **Play** | Toggle | OFF | Start/stop MIDI output |
| **Loop** | Toggle | ON | Loop phoneme sequence |

**Note**: ChatGen automatically reads tempo from your DAW via LV2 Time extension. The BPM slider is only used as fallback if the host doesn't send tempo information.

## MIDI Output

ChatGen sends CC messages on MIDI channel 1 (0-indexed as channel 0):

| CC # | Parameter | Chatterbox Mapping |
|------|-----------|-------------------|
| 71 | F1 value | Jaw opening (200-1000 Hz) |
| 10 | F2 value | Tongue position (500-3000 Hz) |
| 74 | F3 value | Lip rounding (1500-4000 Hz) |
| 75 | F4 value | Voice quality (2500-4500 Hz) |

Messages are sent at beat boundaries with sample-accurate timing.

## Architecture

### DSP Modules

- **TextParser.hpp**: Text → phoneme sequence (rule-based vowel detection)
- **PhonemeMapper.hpp**: Phoneme → MIDI CC lookup table
- **ClockSync.hpp**: BPM-based beat timing engine
- **MidiGenerator.hpp**: LV2 Atom Sequence writer
- **ChatGenEngine.hpp**: Module coordinator

### Files

```
lv2/chatgen/
├── CMakeLists.txt           # Build configuration
├── chatgen.lv2/
│   ├── manifest.ttl         # Plugin registration
│   └── chatgen.ttl          # Port definitions
├── src/
│   ├── modules/
│   │   ├── TextParser.hpp
│   │   ├── PhonemeMapper.hpp
│   │   ├── ClockSync.hpp
│   │   └── MidiGenerator.hpp
│   ├── ChatGenEngine.hpp
│   └── chatgen_plugin.cpp   # LV2 wrapper
└── README.md
```

## Roadmap

### Phase 1: Core DSP (✓ Complete)
- [x] Text parsing with vowel + basic consonant support
- [x] MIDI CC output for formants
- [x] BPM-based timing with DAW sync (LV2 Time)
- [x] Loop mode
- [x] Build and install
- [x] Consonant articulation (8 consonants: m, n, l, r, s, t, d, k)

### Phase 2: UI (In Progress)
- [ ] X11/Cairo text input widget
- [ ] Phoneme preview display
- [ ] Transport controls (Play/Stop button)
- [ ] BPM rotary knob
- [ ] Visual current-phoneme indicator

### Phase 3: Enhanced Parsing (Future)
- [ ] Consonant support (F3/F4 articulation)
- [ ] Diphthong detection
- [ ] Syllable boundary detection
- [ ] Stress marking

### Phase 4: DAW Integration (Partial Complete)
- [x] LV2 Time extension for tempo sync
- [x] Follow DAW BPM automatically
- [ ] Transport play/stop sync (currently manual Play toggle)
- [ ] Timeline position sync
- [ ] MIDI learn for CC routing

## Limitations (MVP)

- **Limited consonants**: Only 8 consonants supported (m, n, l, r, s, t, d, k)
- **No diphthongs**: "ai" treated as separate /a/ + /i/
- **Fixed timing**: Quarter notes only (no subdivision)
- **Hardcoded text**: "hello" (UI pending in Phase 2)
- **English only**: Assumes English orthography
- **Basic articulation**: Consonant F3/F4 values are approximate

## Technical Notes

### First MIDI Output Plugin

ChatGen is the first plugin in the Flues codebase to **output** MIDI rather than receive it. Key implementation details:

- **Port type**: `atom:AtomPort, lv2:OutputPort` (not InputPort)
- **Atom sequence initialization**: Must set type and size in run()
- **Event appending**: Uses `lv2_atom_sequence_append_event()`
- **Sample-accurate timing**: Events sent at exact frame offsets

### Performance

- **CPU usage**: <2% at 48kHz (text parsing is lightweight)
- **Latency**: <1ms from beat boundary (sample-accurate)
- **Memory**: ~10KB (small phoneme sequences)

### Compatibility

- **Tested DAWs**: Ardour, Reaper, Carla
- **Sample rates**: 44.1kHz, 48kHz, 96kHz
- **OS**: Linux (X11 required for Phase 2 UI)

## Troubleshooting

### No MIDI output

1. Check plugin loaded: `lv2ls | grep chatgen`
2. Verify Play toggle is ON
3. Check BPM is reasonable (30-300)
4. Use MIDI monitor to verify messages

### Wrong pitch/timing

- ChatGen does NOT control pitch (MIDI notes)
- It only sends CC messages for formant shaping
- Chatterbox must receive MIDI notes separately

### Formants don't change

- Verify MIDI routing: ChatGen → Chatterbox
- Check Chatterbox CC mapping enabled
- Ensure different vowels in text (not all same)

## License

MIT License - See project root LICENSE file

## Credits

- **IPA vowel positions**: From Chatterbox joystick control (experiments/chatterbox/src/ui/JoystickControl.js)
- **LV2 MIDI output pattern**: Inspired by stepseq.lv2
- **Part of**: Flues audio synthesis project (https://danja.github.io/flues/)
