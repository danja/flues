#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cmake -S "$root_dir/lv2/chordant" -B "$root_dir/lv2/chordant/build"
cmake --build "$root_dir/lv2/chordant/build"
cmake --install "$root_dir/lv2/chordant/build" --prefix "$HOME/.lv2"

echo "Installed Chordant to $HOME/.lv2/chordant.lv2"
