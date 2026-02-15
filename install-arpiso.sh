#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_DIR="$ROOT_DIR/lv2/arpiso"
BUILD_DIR="${BUILD_DIR:-$PLUGIN_DIR/build}"
INSTALL_PREFIX="${1:-$HOME/.lv2}"

if [[ ! -d "$PLUGIN_DIR" ]]; then
  echo "ArpIso source directory not found: $PLUGIN_DIR" >&2
  exit 1
fi

echo "==> Configuring ArpIso"
cmake -S "$PLUGIN_DIR" -B "$BUILD_DIR"

echo "==> Building ArpIso"
cmake --build "$BUILD_DIR" --config Release

echo "==> Installing ArpIso to: $INSTALL_PREFIX"
cmake --install "$BUILD_DIR" --prefix "$INSTALL_PREFIX"

echo "==> Done"
echo "Installed bundle: $INSTALL_PREFIX/arpiso.lv2"
