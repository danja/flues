#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cmake -S "$root_dir/lv2/gremlin" -B "$root_dir/lv2/gremlin/build"
cmake --build "$root_dir/lv2/gremlin/build"
cmake --install "$root_dir/lv2/gremlin/build" --prefix "$HOME/.lv2"

echo "Installed Gremlin to $HOME/.lv2/gremlin.lv2"
