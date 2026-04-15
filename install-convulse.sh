#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cmake -S "$root_dir/lv2/convulse" -B "$root_dir/lv2/convulse/build"
cmake --build "$root_dir/lv2/convulse/build"
cmake --install "$root_dir/lv2/convulse/build" --prefix "$HOME/.lv2"

echo "Installed Convulse to $HOME/.lv2/convulse.lv2"
