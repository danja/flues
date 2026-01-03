#!/usr/bin/env bash
set -euo pipefail

build_dir="lv2/grid-seq/builddir"
src_dir="lv2/grid-seq"
prefix="${HOME}/.lv2"

if [[ -d "${build_dir}" ]]; then
  meson setup --reconfigure "${build_dir}" "${src_dir}" --prefix "${prefix}"
else
  meson setup "${build_dir}" "${src_dir}" --prefix "${prefix}"
fi

meson compile -C "${build_dir}"
meson install -C "${build_dir}"
