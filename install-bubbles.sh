#!/bin/bash
set -euo pipefail

cmake -S lv2/bubbles -B lv2/bubbles/build
cmake --build lv2/bubbles/build
cmake --install lv2/bubbles/build --prefix ~/.lv2

echo "Installed bubbles LV2 plugin to ~/.lv2/bubbles.lv2"
