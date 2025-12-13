# Release Testing Checklist

Use this checklist to verify a release build before publishing to GitHub.

## Build Process

- [ ] Clean build completed without errors
- [ ] All tests passed (6/6 for flues-synth)
- [ ] All LV2 plugins built successfully (7 plugins)
- [ ] Tarball created with correct version number
- [ ] Checksums generated (SHA256 + MD5)
- [ ] Release directory structure is correct

## Installation Tests

### System Install
- [ ] `sudo ./install.sh` completes without errors
- [ ] LV2 plugins installed to `/usr/local/lib/lv2/`
- [ ] flues-synth installed to `/usr/local/bin/`
- [ ] `lv2ls | grep flues` shows all 7 plugins
- [ ] `which flues-synth` returns path
- [ ] `sudo ./uninstall.sh` removes everything cleanly

### User Install
- [ ] Copy to `~/.lv2/` works
- [ ] Copy to `~/bin/` works
- [ ] Plugins appear in LV2 host

## LV2 Plugin Tests

### Disyn
- [ ] Loads in jalv: `jalv.gtk https://danja.github.io/flues/plugins/disyn`
- [ ] UI renders correctly (X11/Cairo)
- [ ] All 7 primitive algorithms produce sound (0-6)
- [ ] MIDI note input works
- [ ] Envelope controls affect sound
- [ ] Reverb works
- [ ] No crashes during normal use
- [ ] Clean shutdown

### Floozy
- [ ] Loads in jalv
- [ ] UI renders correctly
- [ ] All 17 algorithms work (0-16)
- [ ] Interface type switching works (0-11)
- [ ] No crashes when switching algorithms/interfaces
- [ ] param1, param2, param3 all respond
- [ ] Filter and modulation work
- [ ] Reverb works

### Chatterbox
- [ ] Loads in jalv
- [ ] UI renders correctly (20 knobs + vowel joystick)
- [ ] Formants F1-F4 respond to MIDI CC
- [ ] Vowel joystick works (F1/F2 control)
- [ ] Vocal modes work: Nasal, Sing, Shout, Fry
- [ ] Stress control works
- [ ] MIDI velocity affects amplitude
- [ ] Reverb works
- [ ] No distortion at default settings

### ChatGen
- [ ] Loads in jalv
- [ ] UI renders correctly (text input + phoneme preview)
- [ ] Text input accepts keyboard input
- [ ] Phoneme preview shows parsed phonemes
- [ ] Play button triggers MIDI output
- [ ] Serial routing with Chatterbox works (text → speech)
- [ ] Text persists across UI close/reopen
- [ ] Common words parse correctly ("hello world", "cat dog fish")

### Drumkit
- [ ] Loads in jalv
- [ ] UI renders correctly (18 knobs, 4 rows)
- [ ] All 8 drum voices respond to MIDI notes (36-50)
- [ ] Velocity sensitivity works (kick, snare, toms)
- [ ] Hi-hat choke group works (closed chokes open)
- [ ] Bit crusher works
- [ ] Master distortion works
- [ ] Reverb works
- [ ] No clipping at default settings

### PM-Synth
- [ ] Loads in jalv
- [ ] UI renders correctly
- [ ] All 12 interface types selectable
- [ ] Interface switching doesn't crash (race fix verified)
- [ ] Sources (DC, Noise, Tone) work
- [ ] Delay lines work
- [ ] Filter morphing works (LP→BP→HP)
- [ ] Modulation works
- [ ] Reverb works

### Flues-Control
- [ ] Loads in DAW (Ardour/Reaper)
- [ ] Program selector shows 0-28
- [ ] 9 sliders respond to input
- [ ] MIDI Program Change messages sent correctly
- [ ] MIDI CC messages sent correctly (73, 72, 28, 30, 74, 71, 1, 27, 7)
- [ ] Routing to flues-synth works

## Flues-Synth Tests

### Basic Functionality
- [ ] Starts without errors: `./flues-synth`
- [ ] Auto-detects audio device
- [ ] Connects to MIDI controller automatically
- [ ] Responds to MIDI note input (60-84)
- [ ] Responds to MIDI Program Change (0-28)
- [ ] Responds to MIDI CC messages (9 sliders)
- [ ] Clean shutdown with Ctrl+C

### Program Testing (Sample)
- [ ] Program 0 (Disyn Echo) works
- [ ] Program 6 (Full Hybrid) works - **NO SEGFAULT** ✓
- [ ] Program 7 (Disyn Direct) works
- [ ] Program 18 (Hybrid Formant) works
- [ ] Program 18 responds to param3 slider (CC 28)
- [ ] Program 27 (Cross-Mod) works
- [ ] All programs produce sound when triggered

### Stress Tests
- [ ] Rapid program switching doesn't crash
- [ ] Rapid interface type changes don't crash (Program 6)
- [ ] Continuous note playing (2+ minutes) stable
- [ ] All 17 algorithms tested (programs 16/18-27)
- [ ] No memory leaks (valgrind optional)
- [ ] No audio dropouts or glitches

### New Algorithm Tests (param3)
- [ ] Algorithm 7 (Hybrid Formant) - param3 controls formant spacing
- [ ] Algorithm 8 (Cascaded) - param3 controls tanh drive
- [ ] Algorithm 9 (Parallel Bank) - param3 controls mix balance
- [ ] Algorithm 10 (Feedback) - param3 controls feedback lowpass
- [ ] Algorithm 11 (Morphing) - param3 controls crossfade curve
- [ ] Algorithm 12 (Inharmonic) - param3 controls PAF formant freq
- [ ] Algorithm 13 (Adaptive Filter) - param3 controls filter character
- [ ] Algorithm 14 (Multi-Stage) - param3 controls ring mod carrier
- [ ] Algorithm 15 (Freq Asymmetry) - param3 controls crossover freq
- [ ] Algorithm 16 (Cross-Mod) - param3 controls ModFM→DSF depth

## Documentation Tests

- [ ] README.md is accurate
- [ ] CHANGELOG.md updated for this version
- [ ] All plugin README files included
- [ ] algorithms.md reflects 17 algorithms
- [ ] midi.md shows correct CC mappings
- [ ] PROGRAM_CHANGE.md lists all 29 programs
- [ ] PROGRAM6_FIX.md documents race fix
- [ ] QUICKSTART.md instructions work
- [ ] Install/uninstall instructions are correct

## Platform-Specific Tests

### x86_64
- [ ] All plugins work
- [ ] flues-synth works
- [ ] No architecture-specific issues

### ARM (Raspberry Pi 4)
- [ ] All plugins work
- [ ] flues-synth works at 48kHz
- [ ] Audio latency acceptable (~10ms)
- [ ] No ARM-specific crashes
- [ ] CPU usage acceptable (<50% with typical use)

## Integration Tests

### Ardour
- [ ] All plugins load
- [ ] UI windows open correctly
- [ ] MIDI routing works
- [ ] Audio output clean
- [ ] No crashes during session save/load

### Reaper
- [ ] All plugins load
- [ ] UI windows work (including X11 raw UIs)
- [ ] MIDI routing works
- [ ] Audio output clean
- [ ] No crashes

### Carla (Standalone)
- [ ] All plugins load
- [ ] MIDI routing works
- [ ] Audio patchbay works
- [ ] No crashes

## Pre-Release Final Checks

- [ ] Version number correct in all files
- [ ] Git tag created: `git tag v0.1.0`
- [ ] Git tag pushed: `git push origin v0.1.0`
- [ ] Release notes reviewed and edited
- [ ] All checksums verified
- [ ] Tarball tested on clean system
- [ ] No debug/test code left in binaries
- [ ] File permissions correct (executables +x)

## Post-Release Checks

- [ ] GitHub release created successfully
- [ ] Assets uploaded correctly
- [ ] Release notes visible on GitHub
- [ ] Download links work
- [ ] Checksums match uploaded files
- [ ] Installation instructions in release notes correct

## Known Issues to Document

List any known issues that should be in release notes:

- [ ] Flues-synth is monophonic only
- [ ] Plugin UIs require X11 (no Wayland)
- [ ] ChatGen phoneme parsing is basic
- [ ] (Others as discovered)

---

## Sign-off

- [ ] All critical tests passed
- [ ] All major features verified
- [ ] No release-blocking bugs found
- [ ] Ready for publication

**Tester:** _______________
**Date:** _______________
**Version:** _______________
