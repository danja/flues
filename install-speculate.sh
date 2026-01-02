#!/usr/bin/env bash
set -euo pipefail

cmake -S lv2/speculate -B lv2/speculate/build
cmake --build lv2/speculate/build
cmake --install lv2/speculate/build --prefix ~/.lv2
