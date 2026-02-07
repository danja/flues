#!/usr/bin/env bash
set -euo pipefail

cmake -S lv2/metalv -B lv2/metalv/build
cmake --build lv2/metalv/build
cmake --install lv2/metalv/build --prefix ~/.lv2

echo "Installed MetaLV to ~/.lv2/metalv.lv2"
