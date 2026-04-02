#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVER_DIR="$ROOT_DIR/outsider"
CLIENT_DIR="$ROOT_DIR/lv2/outsider-client"

SERVER_BUILD_DIR="${OUTSIDER_SERVER_BUILD_DIR:-$SERVER_DIR/build}"
CLIENT_BUILD_DIR="${OUTSIDER_CLIENT_BUILD_DIR:-$CLIENT_DIR/build}"
INSTALL_PREFIX="${1:-${LV2_INSTALL_PREFIX:-$HOME/.lv2}}"

if [[ ! -d "$SERVER_DIR" ]]; then
  echo "Outsider server source directory not found: $SERVER_DIR" >&2
  exit 1
fi

if [[ ! -d "$CLIENT_DIR" ]]; then
  echo "Outsider LV2 client source directory not found: $CLIENT_DIR" >&2
  exit 1
fi

echo "==> Configuring Outsider server"
cmake -S "$SERVER_DIR" -B "$SERVER_BUILD_DIR"

echo "==> Building Outsider server"
cmake --build "$SERVER_BUILD_DIR" --config Release

echo "==> Configuring Outsider LV2 client"
cmake -S "$CLIENT_DIR" -B "$CLIENT_BUILD_DIR"

echo "==> Building Outsider LV2 client"
cmake --build "$CLIENT_BUILD_DIR" --config Release

echo "==> Installing Outsider LV2 client to: $INSTALL_PREFIX"
cmake --install "$CLIENT_BUILD_DIR" --prefix "$INSTALL_PREFIX"

SERVER_BINARY="$SERVER_BUILD_DIR/outsider"
CLIENT_BUNDLE="$INSTALL_PREFIX/outsider-client.lv2"

if [[ ! -x "$SERVER_BINARY" ]]; then
  echo "Expected server binary not found: $SERVER_BINARY" >&2
  exit 1
fi

echo "==> Done"
echo "Built server: $SERVER_BINARY"
echo "Installed bundle: $CLIENT_BUNDLE"
