#!/usr/bin/env bash
set -euo pipefail

cmake -S lv2/midi-flip -B lv2/midi-flip/build
cmake --build lv2/midi-flip/build
cmake --install lv2/midi-flip/build --prefix ~/.lv2

echo "Installed MIDI Flip to ~/.lv2/midi-flip.lv2"
