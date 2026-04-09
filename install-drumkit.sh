#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cmake -S "$root_dir/lv2/drumkit" -B "$root_dir/lv2/drumkit/build"
cmake --build "$root_dir/lv2/drumkit/build"
cmake --install "$root_dir/lv2/drumkit/build" --prefix "$HOME/.lv2"

echo "Installed Drumkit to $HOME/.lv2/drumkit.lv2"
