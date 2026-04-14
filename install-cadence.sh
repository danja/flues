#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cmake -S "$root_dir/lv2/cadence" -B "$root_dir/lv2/cadence/build"
cmake --build "$root_dir/lv2/cadence/build"
cmake --install "$root_dir/lv2/cadence/build" --prefix "$HOME/.lv2"

echo "Installed Cadence to $HOME/.lv2/cadence.lv2"
