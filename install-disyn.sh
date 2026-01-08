#!/usr/bin/env bash
set -euo pipefail

cmake -S lv2/disyn -B lv2/disyn/build
cmake --build lv2/disyn/build
cmake --install lv2/disyn/build --prefix "$HOME/.lv2"
