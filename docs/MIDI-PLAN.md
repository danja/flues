# MIDI Input Support - Implementation Plan

**Status**: Phase 1 & 2 Complete ✓
**Last Updated**: 2025-10-15

## Viability Assessment: HIGH ✓

MIDI support is **highly viable** for both synthesizers. The Web MIDI API has excellent browser support and integrates cleanly with the existing architecture.

---

## Current Architecture Analysis

Both synths (clarinet-synth and pm-synth) have clean separation:

- **Input layer**: `KeyboardController` handles mouse/touch/keyboard → calls `handleNoteOn(note, frequency)` / `handleNoteOff(note)`
- **Processing layer**: `main.js` coordinates inputs and routes to audio processors
- **Audio layer**: `ClarinetProcessor`/`PMSynthProcessor` accept `noteOn(frequency)` / `noteOff()`

This architecture makes MIDI integration straightforward—we can add MIDI as another input source without modifying the audio engine.

---

## Implementation Phases

### Phase 1: Basic MIDI Note On/Off ✅

**Status**: Complete (pm-synth)

**New module**: `src/ui/MidiController.js`

**Features**:
- Request MIDI access via Web MIDI API
- Listen to note on/off messages (status bytes 0x90/0x80)
- Convert MIDI note numbers to frequencies using formula: `f = 440 * 2^((n-69)/12)` where A4=440Hz @ note 69
- Call existing `handleNoteOn(note, frequency)` / `handleNoteOff(note)` callbacks
- Track active notes for proper note-off handling
- Implement monophonic priority (last-note-priority for single-note synths)
- Velocity sensitivity (map velocity to breath/intensity parameter)

**Files Created**:
- ✅ `experiments/pm-synth/src/ui/MidiController.js` - Full MIDI input handling with device management
- ⏳ `experiments/clarinet-synth/src/ui/MidiController.js` - Not yet implemented

**Files Modified**:
- ✅ `experiments/pm-synth/src/main.js` - MIDI controller initialized, wired up with UI callbacks
- ⏳ `experiments/clarinet-synth/src/main.js` - Not yet modified

**Implementation Details**:
- Web MIDI API access with graceful fallback
- Automatic device detection and selection
- MIDI note to frequency conversion (A4=440Hz)
- Active note tracking for proper note-off handling
- Monophonic last-note-priority
- Channel filtering support (1-16 or "all")
- MIDI activity indicator callback
- All Notes Off (panic) functionality

---

### Phase 2: MIDI Settings UI ✅

**Status**: Complete (pm-synth)

**Location**: Collapsible panel below keyboard in HTML

**UI Components**:
- MIDI device selector dropdown (populated from available inputs)
- Connection status indicator (green=connected, red=disconnected, yellow=no devices)
- Enable/disable toggle switch
- MIDI channel selector (1-16, or "All channels")
- Visual MIDI activity indicator (blinks on note events)
- Collapse/expand button for settings panel

**CSS Styling**:
- Match existing synth aesthetic (dark theme, blue accents)
- Smooth collapse/expand animation
- Mobile-responsive design

**Files Modified**:
- ✅ `experiments/pm-synth/index.html` - Complete MIDI panel with HTML + CSS
- ⏳ `experiments/clarinet-synth/index.html` - Not yet modified

**Implemented Features**:
- Collapsible MIDI panel (click header to toggle)
- Device selector dropdown (auto-populated)
- Connection status indicator (green/yellow/red)
- MIDI activity indicator (flashes on note events)
- Channel selector (1-16 or "All channels")
- Enable/disable toggle checkbox
- "All Notes Off" panic button (red)
- Mobile-responsive grid layout
- Matches Stove dark theme aesthetic

---

### Phase 3: CC Mapping (Future) 📋

**Status**: Planned (Not Started)

**Control Change Support**:
- Map CC controllers (0-127) to synth parameters
- Store mappings in localStorage for persistence
- MIDI learn functionality (click parameter, move CC controller to map)
- Visual feedback during MIDI learn mode
- Default CC mappings for common controllers (CC1=Modulation, CC7=Volume, etc.)
- Clear/reset individual mappings

**Potential CC Mappings**:
- CC1 (Mod Wheel) → Vibrato/LFO depth
- CC7 (Volume) → Master gain
- CC11 (Expression) → Breath/Intensity
- CC74 (Brightness) → Filter cutoff

**Files to Create**:
- `src/ui/MidiCCMapper.js` - CC mapping and MIDI learn logic

**Files to Modify**:
- `src/ui/MidiController.js` - Add CC message handling
- HTML files - Add CC mapping UI section to MIDI panel

---

## Browser Compatibility

| Browser | Support | Notes |
|---------|---------|-------|
| Chrome/Edge | ✓ Full | Native Web MIDI API support |
| Firefox | ✓ Full | Enabled by default since v108 |
| Safari | ⚠️ Partial | Requires flag, improving in recent versions |
| Mobile Safari | ⚠️ Limited | No Web MIDI API support yet |
| Mobile Chrome | ⚠️ Limited | Experimental support |

**Graceful Degradation**: If Web MIDI API is unavailable, show message and hide MIDI settings panel.

---

## Integration Points

1. **main.js**: Initialize `MidiController` alongside existing `KeyboardController`
2. **index.html**: Add collapsible MIDI settings panel below keyboard section
3. **No changes needed** to audio processors (already accept frequency input)
4. **Optional**: Add MIDI status to existing status displays

---

## Technical Details

### MIDI Note to Frequency Conversion

```javascript
function midiNoteToFrequency(noteNumber) {
    return 440 * Math.pow(2, (noteNumber - 69) / 12);
}
```

### MIDI Note to Name Conversion

```javascript
function midiNoteToName(noteNumber) {
    const noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
    const octave = Math.floor(noteNumber / 12) - 1;
    const noteName = noteNames[noteNumber % 12];
    return `${noteName}${octave}`;
}
```

### MIDI Message Structure

- **Note On**: Status byte `0x90-0x9F` (0x90 + channel), note number (0-127), velocity (1-127)
- **Note Off**: Status byte `0x80-0x8F` (0x80 + channel), note number (0-127), velocity (0-127)
- **Note On with velocity 0** is equivalent to Note Off
- **CC**: Status byte `0xB0-0xBF` (0xB0 + channel), controller number (0-127), value (0-127)

---

## Benefits

- **Zero breaking changes** - adds alongside existing input methods
- **Reuses existing note handling** - no audio engine changes required
- **Progressive enhancement** - gracefully degrades if MIDI unavailable
- **Small footprint** - ~200-300 lines for basic support (Phase 1+2)
- **Professional workflow** - enables hardware MIDI controllers and DAW integration
- **Future expandable** - foundation for advanced features (MPE, polyphony, etc.)

---

## Testing Strategy

1. **Virtual MIDI devices**: Test with software MIDI keyboards (VMPK, etc.)
2. **Hardware MIDI controllers**: Test with USB MIDI keyboards
3. **MIDI channels**: Verify channel filtering works correctly
4. **Multiple devices**: Test device switching in dropdown
5. **Note-off handling**: Verify all notes release properly
6. **Browser compatibility**: Test across Chrome, Firefox, Safari
7. **Mobile fallback**: Verify graceful degradation on mobile

---

## Implementation Progress

### Phase 1: Basic MIDI Note On/Off (pm-synth)
- [x] Create MidiController.js for pm-synth ✅
- [x] Wire up in pm-synth/main.js ✅
- [ ] Create MidiController.js for clarinet-synth (future)
- [ ] Wire up in clarinet-synth/main.js (future)
- [ ] Test with virtual MIDI device 🔄 (ready for testing)
- [ ] Test with hardware MIDI device 🔄 (ready for testing)

### Phase 2: MIDI Settings UI (pm-synth)
- [x] Design MIDI panel HTML structure ✅
- [x] Add CSS styling for MIDI panel ✅
- [x] Implement collapse/expand functionality ✅
- [x] Add device selector dropdown ✅
- [x] Add connection status indicator ✅
- [x] Add channel selector ✅
- [x] Add MIDI activity indicator ✅
- [ ] Test on mobile (responsive design) 🔄 (ready for testing)

### Phase 3: CC Mapping (Future)
- [ ] Design CC mapping data structure
- [ ] Implement MIDI learn mode
- [ ] Add CC mapping UI
- [ ] Implement localStorage persistence
- [ ] Add default CC mappings
- [ ] Test with various controllers

---

## Notes

- Both synthesizers are currently **monophonic**, so MIDI implementation uses last-note-priority
- Velocity parameter is passed to callbacks but not yet mapped to synth parameters (ready for future enhancement)
- Future polyphonic support would require significant engine changes (voice allocation, mixing)
- ✅ MIDI panic button implemented (All Notes Off)
- MIDI activity indicator provides visual feedback for incoming messages

## Testing Instructions

To test MIDI support in pm-synth:

1. **Build and run**: `cd experiments/pm-synth && npm run dev`
2. **Connect MIDI device**: Plug in USB MIDI controller or use virtual MIDI device
3. **Power on synth**: Click PWR button
4. **Check MIDI panel**: Should show green indicator if device detected
5. **Select device**: Choose your MIDI device from dropdown
6. **Play notes**: MIDI keyboard should trigger synth notes
7. **Test panic button**: Click "All Notes Off" to release stuck notes
8. **Test channel filter**: Try different channel settings
9. **Test collapse**: Click MIDI panel header to collapse/expand

## Known Limitations

- Velocity is captured but not yet mapped to synth parameters (Phase 3 feature)
- No CC mapping yet (Phase 3 feature)
- Mobile browsers have limited/no Web MIDI API support
- Only pm-synth has MIDI support currently (clarinet-synth pending)

---

## Related Files

- `experiments/clarinet-synth/src/ui/KeyboardController.js` - Reference for input handling pattern
- `experiments/pm-synth/src/ui/KeyboardController.js` - Reference for input handling pattern
- `experiments/clarinet-synth/src/main.js` - Main app coordination
- `experiments/pm-synth/src/main.js` - Main app coordination
