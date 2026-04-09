#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PLUGINS=(
  disyn
  floozy
  chatterbox
  chatgen
  drumkit
  drumgen
  gremlin
  gremlin-driver
  euclid
  pm-synth
  flues-control
)

build_and_install() {
  local name="$1"
  local dir="$ROOT_DIR/lv2/$name"

  echo "==> Building $name"
  cmake -S "$dir" -B "$dir/build"
  cmake --build "$dir/build"
  cmake --install "$dir/build" --prefix "$HOME/.lv2"
}

for plugin in "${PLUGINS[@]}"; do
  build_and_install "$plugin"
done

echo "==> Installed LV2 plugins to $HOME/.lv2"
