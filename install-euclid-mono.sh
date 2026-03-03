#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cmake -S "$root_dir/lv2/euclid-mono" -B "$root_dir/lv2/euclid-mono/build"
cmake --build "$root_dir/lv2/euclid-mono/build"
cmake --install "$root_dir/lv2/euclid-mono/build" --prefix "$HOME/.lv2"

echo "Installed EuclidMono to $HOME/.lv2/euclid-mono.lv2"
