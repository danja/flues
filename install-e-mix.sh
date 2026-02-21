#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_DIR="$ROOT_DIR/lv2/e-mix"
BUILD_DIR="$PLUGIN_DIR/build"
PREFIX="${1:-$HOME/.lv2}"

cmake -S "$PLUGIN_DIR" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR"
cmake --install "$BUILD_DIR" --prefix "$PREFIX"

echo "Installed E-Mix to: $PREFIX/e-mix.lv2"
