#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cmake -S "$root_dir/lv2/p-mix" -B "$root_dir/lv2/p-mix/build"
cmake --build "$root_dir/lv2/p-mix/build"
cmake --install "$root_dir/lv2/p-mix/build" --prefix "$HOME/.lv2"

echo "Installed P-Mix to $HOME/.lv2/p-mix.lv2"
