#!/usr/bin/env bash
set -euo pipefail

cmake -S lv2/slimmer -B lv2/slimmer/build
cmake --build lv2/slimmer/build
cmake --install lv2/slimmer/build --prefix ~/.lv2

echo "Installed Slimmer to ~/.lv2/slimmer.lv2"
