#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

build_and_install() {
  local name="$1"
  local dir="$ROOT_DIR/lv2/$name"

  echo "==> Building $name"
  cmake -S "$dir" -B "$dir/build"
  cmake --build "$dir/build"
  cmake --install "$dir/build" --prefix "$HOME/.lv2"
}

build_and_install "drumkit"
build_and_install "euclid"

echo "==> Installed drumkit and euclid to $HOME/.lv2"
