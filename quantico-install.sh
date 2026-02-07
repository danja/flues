#!/usr/bin/env bash
set -euo pipefail

cmake -S lv2/quantico -B lv2/quantico/build
cmake --build lv2/quantico/build
cmake --install lv2/quantico/build --prefix ~/.lv2

echo "Installed Quantico to ~/.lv2/quantico.lv2"
