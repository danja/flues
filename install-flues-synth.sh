#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/flues-synth"
BUILD_DIR="${PROJECT_DIR}/builddir"

RUN_AFTER_BUILD=true
SKIP_TEST=false

print_usage() {
    cat <<'EOF'
Usage: ./flues-all [options] [audio_device]

Builds flues-synth with Meson, runs the smoke test, and then launches the
binary. If no ALSA device is provided, defaults to Raspberry Pi headphones
("hw:2,0"). You can override by passing any device name (e.g. hw:Headphones).

Options:
  --no-run       Build/test only; do not start the synth.
  --skip-test    Skip engine-smoke after building.
  --help         Show this help.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --no-run)
            RUN_AFTER_BUILD=false
            shift
            ;;
        --skip-test)
            SKIP_TEST=true
            shift
            ;;
        --help|-h)
            print_usage
            exit 0
            ;;
        *)
            # First non-flag argument is the audio device; leave it + rest intact
            break
            ;;
    esac
done

DEVICE_ARGS=("$@")
if [[ ${#DEVICE_ARGS[@]} -eq 0 ]]; then
    DEVICE_ARGS=("hw:3,0") # danny was 2,0
fi

cd "${PROJECT_DIR}"

echo "info: configuring Meson (reconfigure to pick up changes)..."
meson setup "${BUILD_DIR}" --reconfigure

echo "info: compiling flues-synth..."
meson compile -C "${BUILD_DIR}"

if [[ "${SKIP_TEST}" == false ]]; then
    echo "info: running smoke test..."
    meson test -C "${BUILD_DIR}" engine-smoke
fi

if [[ "${RUN_AFTER_BUILD}" == true ]]; then
    echo "info: launching ${BUILD_DIR}/flues-synth ${DEVICE_ARGS[*]}"
    exec "${BUILD_DIR}/flues-synth" "${DEVICE_ARGS[@]}"
fi
