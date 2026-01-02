#!/usr/bin/env bash
set -euo pipefail

cmake -S lv2/euclidean-gate -B lv2/euclidean-gate/build
cmake --build lv2/euclidean-gate/build
cmake --install lv2/euclidean-gate/build --prefix ~/.lv2
