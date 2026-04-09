#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cmake -S "$root_dir/lv2/drumgen" -B "$root_dir/lv2/drumgen/build"
cmake --build "$root_dir/lv2/drumgen/build"
cmake --install "$root_dir/lv2/drumgen/build" --prefix "$HOME/.lv2"

echo "Installed DrumGen to $HOME/.lv2/drumgen.lv2"
