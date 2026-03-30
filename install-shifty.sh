#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cmake -S "$root_dir/lv2/shifty" -B "$root_dir/lv2/shifty/build"
cmake --build "$root_dir/lv2/shifty/build"
cmake --install "$root_dir/lv2/shifty/build" --prefix "$HOME/.lv2"

echo "Installed Shifty to $HOME/.lv2/shifty.lv2"
