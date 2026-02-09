#!/usr/bin/env bash
set -euo pipefail

cmake -S lv2/ants -B lv2/ants/build
cmake --build lv2/ants/build
cmake --install lv2/ants/build --prefix ~/.lv2

echo "Installed Ants to ~/.lv2/ants.lv2"
