# ChatGen LV2 Plugin

Text-to-speech MIDI generator for controlling the Chatterbox formant speech synthesizer.

## Overview

ChatGen is an LV2 utility plugin that converts English text into MIDI events (Note On/Off + CC messages) for driving the Chatterbox speech synthesis plugin. It parses text using rule-based phoneme estimation and generates sample-accurate MIDI synchronized to a musical beat grid.

**Status**: Phase 2 Complete - Full DSP engine with X11/Cairo UI

## Features

- **Full phoneme parsing**: Converts English text to 40+ phonemes (vowels, fricatives, plosives, nasals, liquids, affricates)
- **MIDI Note generation**: Sends Note On/Off to trigger Chatterbox's envelope
- **MIDI CC output**: 7 CCs for complete speech control (formants, noise, voiced/unvoiced)
- **X11/Cairo UI**: Text input field with real-time phoneme preview
- **DAW tempo sync**: Reads BPM from host via LV2 Time extension
- **Transport sync**: Follows DAW play/stop automatically
- **Loop mode**: Repeat phoneme sequence continuously
- **Text persistence**: Text survives UI focus changes and DAW restarts
- **Sample-accurate events**: MIDI messages sent at exact beat boundaries
- **Voiced/Unvoiced articulation**: Proper larynx control for consonants vs vowels
- **Noise control**: Dynamic aspirator control for fricatives and plosives

## Signal Flow

```
Text Input (UI) → TextParser → Phoneme Sequence
                       ↓
                PhonemeMapper → Formant CCs + Voiced/Noise flags
                       ↓
                  ClockSync → Half-note beat detection (every 2 beats)
                       ↓
               MidiGenerator → Note On/Off + 7 MIDI CCs → Output
```

## Phoneme Mapping

ChatGen supports 40+ phonemes with complete voiced/unvoiced and noise control:

### Vowels (Voiced, No Noise)

| Phoneme | Example | Text | F1 | F2 | F3 | F4 | Voiced | Noise |
|---------|---------|------|----|----|----|----|--------|-------|
| /i/ | "see" | "ee", "ea" | 11 | 90 | 63 | 63 | ON | 0 |
| /ɪ/ | "sit" | "i" | 36 | 76 | 63 | 63 | ON | 0 |
| /e/ | "bed" | "e" | 52 | 67 | 63 | 63 | ON | 0 |
| /æ/ | "cat" | "a" | 71 | 67 | 63 | 63 | ON | 0 |
| /ɑ/ | "father" | "ah" | 84 | 30 | 63 | 63 | ON | 0 |
| /ɔ/ | "thought" | "aw" | 63 | 20 | 63 | 63 | ON | 0 |
| /o/ | "home" | "oh" | 58 | 18 | 63 | 63 | ON | 0 |
| /u/ | "boot" | "oo", "ou" | 17 | 19 | 63 | 63 | ON | 0 |
| /ʊ/ | "put" | "u" (in some contexts) | 36 | 25 | 63 | 63 | ON | 0 |
| /ʌ/ | "cup" | "u", "uh" | 63 | 30 | 63 | 63 | ON | 0 |
| /ɜ/ | "bird" | "er", "ur", "ir" | 52 | 46 | 45 | 63 | ON | 5 |

### Plosives (Burst Noise)

| Phoneme | Example | Text | Voiced | Noise | Notes |
|---------|---------|------|--------|-------|-------|
| /p/ | "pat" | "p" | OFF | 70 | Voiceless bilabial |
| /b/ | "bat" | "b" | ON | 70 | Voiced bilabial |
| /t/ | "top" | "t" | OFF | 70 | Voiceless alveolar |
| /d/ | "dog" | "d" | ON | 70 | Voiced alveolar |
| /k/ | "cat" | "k", "c" | OFF | 70 | Voiceless velar |
| /g/ | "go" | "g" | ON | 70 | Voiced velar |

### Fricatives (High Noise)

| Phoneme | Example | Text | Voiced | Noise | Notes |
|---------|---------|------|--------|-------|-------|
| /f/ | "fish" | "f" | OFF | 110 | Voiceless labiodental |
| /v/ | "very" | "v" | ON | 110 | Voiced labiodental |
| /s/ | "see" | "s" | OFF | 120 | Voiceless alveolar |
| /z/ | "zoo" | "z" | ON | 120 | Voiced alveolar |
| /θ/ | "think" | "th" (voiceless) | OFF | 100 | Voiceless dental |
| /ð/ | "this" | "dh" | ON | 100 | Voiced dental |
| /ʃ/ | "ship" | "sh" | OFF | 115 | Voiceless postalveolar |
| /ʒ/ | "measure" | "zh" | ON | 115 | Voiced postalveolar |
| /h/ | "hat" | "h" | OFF | 90 | Voiceless glottal |

### Affricates (Plosive + Fricative)

| Phoneme | Example | Text | Voiced | Noise |
|---------|---------|------|--------|-------|
| /tʃ/ | "church" | "ch" | OFF | 95 |
| /dʒ/ | "judge" | "j" | ON | 95 |

### Nasals (Voiced + Slight Noise)

| Phoneme | Example | Text | Voiced | Noise |
|---------|---------|------|--------|-------|
| /m/ | "mat" | "m" | ON | 10 |
| /n/ | "net" | "n" | ON | 10 |
| /ŋ/ | "sing" | "ng" | ON | 10 |

### Liquids & Approximants (Mostly Voiced)

| Phoneme | Example | Text | Voiced | Noise |
|---------|---------|------|--------|-------|
| /l/ | "let" | "l" | ON | 5 |
| /r/ | "red" | "r" | ON | 5 |
| /w/ | "wet" | "w" | ON | 0 |
| /j/ | "yes" | "y" | ON | 0 |

## Text Parsing Rules

### Digraphs (2-character patterns, checked first)
- "ee", "ea" → /i/ (as in "meet", "eat")
- "oo", "ou" → /u/ (as in "moon", "you")
- "ah" → /ɑ/ (as in "aha")
- "oh" → /o/ (as in "oh")
- "aw" → /ɔ/ (as in "awful")
- "uh" → /ʌ/ (as in "uh-oh")
- "er", "ur", "ir" → /ɜ/ (as in "her", "fur", "bird")
- "sh" → /ʃ/ (as in "ship")
- "ch" → /tʃ/ (as in "church")
- "th" → /θ/ (as in "think") - voiceless by default
- "dh" → /ð/ (as in "this") - voiced version
- "zh" → /ʒ/ (as in "measure")
- "ng" → /ŋ/ (as in "sing")

### Single characters
- **Vowels**: "a" → /æ/ | "e" → /e/ | "i" → /ɪ/ | "o" → /o/ | "u" → /ʌ/
- **Consonants**: All standard consonants mapped to nearest IPA equivalent
- **Whitespace**: Skipped (allows multi-word phrases)

### Examples
```
Text: "cat dog fish"
Phonemes: [k] [ae] [t] [d] [o] [g] [f] [ih] [sh]
Sound: "k-æt d-ɔg f-ɪ-ʃ" with proper voiced/unvoiced transitions

Text: "hello world"
Phonemes: [h] [e] [l] [o] [w] [er] [l] [d]

Text: "the quick brown fox"
Phonemes: [th] [e] [k] [w] [ih] [k] [b] [r] [aw] [n] [f] [o] [k] [s]
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

### With Chatterbox (Recommended Setup)

**Best configuration**: Put both plugins on the **same track** in series:

1. Create an instrument track in your DAW
2. Add plugins in this order:
   ```
   Track: [ChatGen] → [Chatterbox] → Audio Out
   ```

3. Configure ChatGen:
   - Type text in the text input box (e.g., "cat dog fish")
   - Press **Enter** to send text to DSP
   - Click **Play** button (or use DAW transport)
   - Enable **Loop** to repeat

4. Configure Chatterbox:
   - **Voiced**: Will be controlled by ChatGen (CC 104)
   - **Aspirated**: Will be controlled by ChatGen (CC 103)
   - **Attack**: 10-20ms (fast onset)
   - **Release**: 50-100ms (smooth transition)
   - **Master Gain**: 0.6-0.8

5. Start DAW playback and hear speech!

**How it works**:
- ChatGen generates MIDI Note On (C4, velocity 96) when Play starts
- Every 2 beats (1 second at 120 BPM), ChatGen sends:
  - CC 71, 10, 74, 75 (formants F1-F4)
  - CC 102 (noise level)
  - CC 103 (aspirator on/off)
  - CC 104 (voiced on/off)
- Chatterbox receives all messages on same track and synthesizes speech
- ChatGen sends Note Off when playback stops

### Timing Control

**Phoneme duration**: Each phoneme lasts **2 beats (half notes)**

| DAW Tempo | Phoneme Duration | Use Case |
|-----------|------------------|----------|
| 120 BPM | 1.0 second | Normal speech |
| 60 BPM | 2.0 seconds | Slow/clear articulation |
| 240 BPM | 0.5 seconds | Fast/robotic speech |

Tip: Start at **60-90 BPM** for clearer consonant articulation.

### In Jalv (Testing)

```bash
jalv.gtk https://danja.github.io/flues/plugins/chatgen
```

Use a MIDI monitor to verify output:
```bash
# Install MIDI monitor
sudo apt install kmidimon

# Or use command-line monitor
aseqdump -p <ChatGen_port>
```

## Controls

### UI Controls

| Control | Type | Description |
|---------|------|-------------|
| **Text Input** | Text field | Type English text, press Enter to send to DSP |
| **Phoneme Preview** | Label | Shows parsed phonemes in real-time (e.g., "[k] [ae] [t]") |
| **Play Button** | Toggle | Start/stop MIDI output (green when playing) |
| **Loop Button** | Toggle | Loop phoneme sequence (blue when enabled) |

### Port Controls (Automatable)

| Port | Range | Default | Description |
|------|-------|---------|-------------|
| **Control** | Atom | - | Input for LV2 Time position + text from UI |
| **MIDI Out** | Atom | - | Note On/Off + 7 CCs |
| **Play** | Toggle | ON | Start/stop playback (syncs with transport) |
| **Loop** | Toggle | ON | Loop phoneme sequence |
| **Text Out** | Atom | - | Current text (for UI persistence) |

**Note**: ChatGen automatically reads tempo from your DAW via LV2 Time extension.

## MIDI Output

ChatGen sends on **MIDI channel 1** (0-indexed as channel 0):

### Note Events

| Event | Note | Velocity | When |
|-------|------|----------|------|
| Note On | 60 (C4) | 96 | When Play starts |
| Note Off | 60 (C4) | 0 | When Play stops |

### CC Messages (sent every 2 beats with phoneme changes)

| CC # | Parameter | Range | Chatterbox Mapping |
|------|-----------|-------|--------------------|
| 71 | F1 (Jaw) | 0-127 | 200-1000 Hz (exponential) |
| 10 | F2 (Tongue) | 0-127 | 500-3000 Hz (exponential) |
| 74 | F3 (Lips) | 0-127 | 1500-4000 Hz (exponential) |
| 75 | F4 (Quality) | 0-127 | 2500-4500 Hz (exponential) |
| 102 | Noise Level | 0-127 | 0.0-1.0 (linear) |
| 103 | Aspirated | 0 or 127 | OFF when noise=0, ON when noise>0 |
| 104 | Voiced | 0 or 127 | OFF for unvoiced consonants (f,s,t,k,p,etc), ON for vowels |

Messages are sent at beat boundaries with sample-accurate timing.

## Architecture

### DSP Modules

- **TextParser.hpp**: Text → phoneme sequence (40+ phonemes with digraph detection)
- **PhonemeMapper.hpp**: Phoneme → MIDI CC values (formants, noise, voiced)
- **ClockSync.hpp**: BPM-based half-note timing engine
- **MidiGenerator.hpp**: Note On/Off + CC sequence writer
- **ChatGenEngine.hpp**: Module coordinator

### UI

- **chatgen_ui_x11.c**: X11/Cairo text input widget with phoneme preview
- Real-time phoneme parsing for preview
- Text persistence across focus changes
- Play/Loop toggle buttons

### Files

```
lv2/chatgen/
├── CMakeLists.txt           # Build configuration
├── chatgen.lv2/
│   ├── manifest.ttl         # Plugin registration
│   └── chatgen.ttl          # Port definitions
├── src/
│   ├── modules/
│   │   ├── TextParser.hpp       # Text → phonemes (40+ phonemes)
│   │   ├── PhonemeMapper.hpp    # Phoneme → CCs (formants + noise + voiced)
│   │   ├── ClockSync.hpp        # Half-note timing (2 beats/phoneme)
│   │   └── MidiGenerator.hpp    # Note On/Off + CCs
│   ├── ui/
│   │   └── chatgen_ui_x11.c     # X11/Cairo UI
│   ├── ChatGenEngine.hpp        # DSP coordinator
│   └── chatgen_plugin.cpp       # LV2 wrapper
└── README.md
```

## Roadmap

### Phase 1: Core DSP (✓ Complete)
- [x] Text parsing with 40+ phonemes
- [x] MIDI CC output for formants
- [x] BPM-based timing with DAW sync (LV2 Time)
- [x] Loop mode
- [x] Note On/Off generation
- [x] Voiced/Unvoiced control (CC 104)
- [x] Noise control (CC 102, CC 103)
- [x] Half-note phoneme duration (2 beats)

### Phase 2: UI (✓ Complete)
- [x] X11/Cairo text input widget
- [x] Phoneme preview display
- [x] Transport controls (Play/Loop buttons)
- [x] Visual feedback (button states)
- [x] Text persistence across focus changes

### Phase 3: Enhanced Features (Future)
- [ ] Variable phoneme duration (rhythm control)
- [ ] Stress marking (amplitude/pitch variation)
- [ ] Syllable boundary detection
- [ ] Diphthong animation (smooth formant transitions)
- [ ] Preset system (save/load text phrases)
- [ ] MIDI learn for CC routing

### Phase 4: Advanced Parsing (Future)
- [ ] Dictionary-based pronunciation lookup
- [ ] Contextual pronunciation rules
- [ ] Support for other languages (IPA-based)
- [ ] Phonetic alphabet input mode

## Limitations

- **English only**: Assumes English orthography (but extensible to other languages)
- **Rule-based parsing**: No dictionary lookup (may mispronounce some words)
- **Fixed rhythm**: All phonemes last 2 beats (no variation yet)
- **Monophonic**: One phoneme at a time (no blending/coarticulation)
- **No pitch control**: Uses fixed MIDI note (C4/60)

## Technical Notes

### MIDI Generation Pattern

ChatGen is the first plugin in the Flues codebase to **output** MIDI. Key implementation:

- **Port type**: `atom:AtomPort, lv2:OutputPort`
- **Atom sequence initialization**: Set type/size in run() before appending
- **Event appending**: `lv2_atom_sequence_append_event()`
- **Sample-accurate timing**: Events at exact frame offsets within beat boundaries
- **Capacity management**: Track buffer capacity separately from current size

### Text Persistence Pattern

Text survives UI focus changes via bidirectional communication:

- **UI → DSP**: Bare atom:String sent with atomEventTransferUrid (host wraps in sequence)
- **DSP → UI**: Text broadcast on every audio callback in sequence
- **UI protection**: `user_is_editing` flag prevents overwrites while typing
- **Port subscription**: UI subscribes to TEXT_OUT port for updates

### Performance

- **CPU usage**: <1% at 48kHz (text parsing is one-time, CC output is sparse)
- **Latency**: <1ms from beat boundary (sample-accurate MIDI generation)
- **Memory**: ~20KB (small phoneme sequences + UI state)

### Compatibility

- **Tested DAWs**: Ardour, Reaper, Carla
- **Sample rates**: 44.1kHz, 48kHz, 96kHz
- **OS**: Linux (X11 required for UI)
- **Chatterbox version**: Requires CC 103 and CC 104 support (2025-02 or later)

## Troubleshooting

### No sound output

1. **Check plugin order**: ChatGen must come BEFORE Chatterbox on the same track
2. **Verify Play is ON**: Green Play button in ChatGen UI
3. **Check DAW transport**: Start DAW playback
4. **Verify text**: Type text and press Enter
5. **Check Chatterbox**: Master gain should be 0.6-0.8

### Only tonal sound (no fricatives)

1. **Verify CC 103/104 support**: Chatterbox must be 2025-02 or later
2. **Check MIDI routing**: CCs must reach Chatterbox
3. **Test with "fish"**: Should hear "f" and "sh" as breathy noise

### Phonemes too short

1. **Lower BPM**: Set DAW tempo to 60-90 BPM (2 beats = 1.3-2 seconds)
2. **Check timing**: Each phoneme lasts 2 beats (half notes)

### Text reverts to default

1. **Press Enter**: Text only sends to DSP when you press Enter
2. **Check console**: Should see "ChatGen DSP: Received text 'XXX' → N phonemes"
3. **Reload plugin**: Some hosts cache plugin state incorrectly

### MIDI monitoring shows no CCs

1. **Verify Play is ON**: Toggle must be enabled
2. **Check BPM**: Reasonable value (30-300)
3. **Use MIDI monitor**: `aseqdump -p <port>` to verify raw output
4. **Check beat timing**: CCs only sent every 2 beats (1 second at 120 BPM)

## Sound Design Tips

### Clear articulation
- **Tempo**: 60-75 BPM (2 seconds per phoneme)
- **Attack**: 20ms (sharp onset for consonants)
- **Release**: 100ms (smooth transitions)

### Fast robotic speech
- **Tempo**: 180-240 BPM (0.5-0.66 seconds per phoneme)
- **Attack**: 5ms (very sharp)
- **Release**: 20ms (abrupt)

### Singing/melodic
- **Tempo**: 40-60 BPM (3-4 seconds per phoneme)
- **Sing mode**: Enable in Chatterbox (CC 81)
- **Stress**: Increase for louder vowels (CC 1)

### Whispered speech
- **Manually adjust Chatterbox**:
  - Voiced: OFF (or use only noise)
  - Aspirated: ON
  - Noise Level: 0.3-0.5
- Text with many fricatives works best ("fish", "she", "ssh")

## Examples

### "Hello world"
```
Phonemes: [h] [e] [l] [o] [w] [er] [l] [d]
Duration: 8 phonemes × 2 beats = 16 beats = 8 seconds at 120 BPM
```

### "The quick brown fox"
```
Phonemes: [th] [e] [k] [w] [ih] [k] [b] [r] [aw] [n] [f] [o] [k] [s]
Duration: 14 phonemes × 2 beats = 28 beats = 14 seconds at 120 BPM
Features: Voiceless fricatives (th, f, s), voiced plosives (b), mixed articulation
```

### "She sells seashells"
```
Phonemes: [sh] [e] [s] [e] [l] [s] [s] [e] [ae] [sh] [e] [l] [s]
Duration: 13 phonemes × 2 beats = 26 beats = 13 seconds at 120 BPM
Features: Many sibilants (sh, s) - great for testing noise/fricative articulation
```

## License

MIT License - See project root LICENSE file

## Credits

- **IPA vowel positions**: From Chatterbox joystick control (experiments/chatterbox/src/ui/JoystickControl.js)
- **Phoneme mapping**: Based on standard IPA vowel quadrilateral and formant research
- **LV2 MIDI output pattern**: Inspired by stepseq.lv2 and midifilter.lv2
- **UI pattern**: Based on Disyn/Floozy X11/Cairo UI architecture
- **Part of**: Flues audio synthesis project (https://danja.github.io/flues/)

## See Also

- **Chatterbox**: The speech synthesis target for ChatGen (lv2/chatterbox/)
- **Flues Project**: Parent project with multiple synthesis experiments (https://github.com/danja/flues)
