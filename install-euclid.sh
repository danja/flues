#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cmake -S "$root_dir/lv2/euclid" -B "$root_dir/lv2/euclid/build"
cmake --build "$root_dir/lv2/euclid/build"
cmake --install "$root_dir/lv2/euclid/build" --prefix "$HOME/.lv2"

echo "Installed Euclid to $HOME/.lv2/euclid.lv2"
