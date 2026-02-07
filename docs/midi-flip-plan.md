# MIDI Flip Implementation Checklist

## 1) LV2 Interface + Metadata
- [x] Create `lv2/midi-flip/midi-flip.lv2/manifest.ttl`
- [x] Create `lv2/midi-flip/midi-flip.lv2/midi-flip.ttl`
- [x] Define ports: `midi_in`, `midi_out`, `pivot`
- [x] Set pivot defaults: 60 (C4), range 0–127, integer
- [x] Declare `urid:map` required feature
- [x] Add state key for pivot persistence

## 2) DSP Plugin
- [x] Add `lv2/midi-flip/src/midi_flip_plugin.cpp`
- [x] Implement MIDI pass-through for non-note events
- [x] Flip Note On/Off around pivot: `flipped = clamp(2*pivot - note, 0, 127)`
- [x] Preserve channel + velocity
- [x] Implement state save/restore for pivot

## 3) X11/Cairo UI
- [x] Add `lv2/midi-flip/src/ui/midi_flip_ui_x11.c`
- [x] Single slider for Pivot (0–127)
- [x] Display numeric pivot value
- [x] Use `LV2_UI__parent` and an event thread

## 4) Build + Install
- [x] Add `lv2/midi-flip/CMakeLists.txt` (plugin + UI)
- [x] Add `flip-install.sh` at repo root

## 5) Documentation
- [x] Add `lv2/midi-flip/README.md` (user-first)
- [x] Update root `README.md` LV2 list + build list
- [x] Update `AGENTS.md` LV2 section
