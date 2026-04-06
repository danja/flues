#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cmake -S "$root_dir/lv2/gremlin-driver" -B "$root_dir/lv2/gremlin-driver/build"
cmake --build "$root_dir/lv2/gremlin-driver/build"
cmake --install "$root_dir/lv2/gremlin-driver/build" --prefix "$HOME/.lv2"

echo "Installed GremlinDriver to $HOME/.lv2/gremlin-driver.lv2"
