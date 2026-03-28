#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cmake -S "$root_dir/lv2/bassgen" -B "$root_dir/lv2/bassgen/build"
cmake --build "$root_dir/lv2/bassgen/build"
cmake --install "$root_dir/lv2/bassgen/build" --prefix "$HOME/.lv2"

echo "Installed BassGen to $HOME/.lv2/bassgen.lv2"
