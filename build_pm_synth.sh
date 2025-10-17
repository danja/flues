#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/lv2/pm-synth"
BUILD_DIR="${PROJECT_DIR}/build"

CUSTOM_INSTALL_PREFIX=""
AUTO_INSTALL=false
CLEAN=false
SHOW_HELP=false

function usage() {
    cat <<'EOF'
Usage: ./build_pm_synth.sh [options]

Options:
  --clean               Remove the existing CMake build directory before configuring.
  --install <prefix>    Run `cmake --install` with the given prefix (e.g. ~/.lv2).
  --install-default     Install into the user's default LV2 directory (usually ~/.lv2).
  --help                Show this help message.

The script checks for required build tools, configures the LV2 project with CMake,
and builds the Flues PM Synth plugin. If GTK+3 headers are available, the Steampipe-style
UI is built automatically.
EOF
}

while (( "$#" )); do
    case "$1" in
        --clean)
            CLEAN=true
            shift
            ;;
        --install)
            if [[ $# -lt 2 ]]; then
                echo "error: --install requires a prefix argument" >&2
                exit 1
            fi
            CUSTOM_INSTALL_PREFIX="$2"
            shift 2
            ;;
        --install-default)
            AUTO_INSTALL=true
            shift
            ;;
        --help|-h)
            SHOW_HELP=true
            shift
            ;;
        *)
            echo "error: unknown option '$1'" >&2
            usage
            exit 1
            ;;
    esac
done

if "${SHOW_HELP}"; then
    usage
    exit 0
fi

REQUIRED_CMDS=(cmake pkg-config)
for cmd in "${REQUIRED_CMDS[@]}"; do
    if ! command -v "${cmd}" >/dev/null 2>&1; then
        echo "error: '${cmd}' is not installed or not in PATH." >&2
        echo "       Install it (e.g. sudo apt install ${cmd}) and rerun." >&2
        exit 1
    fi
done

if ! pkg-config --exists lv2; then
    echo "error: LV2 development files were not found." >&2
    echo "       On Debian/Ubuntu install them with: sudo apt install lv2-dev" >&2
    exit 1
fi

if ! pkg-config --exists gtk+-3.0; then
    echo "note: GTK+3 development files are missing, the graphical UI will be skipped."
    echo "      Install them to get the Steampipe-style panel (e.g. sudo apt install libgtk-3-dev)."
else
    echo "info: GTK+3 detected; the graphical UI will be built."
fi

if "${CLEAN}" && [[ -d "${BUILD_DIR}" ]]; then
    echo "info: removing existing build directory '${BUILD_DIR}'."
    rm -rf "${BUILD_DIR}"
fi

echo "info: configuring project..."
cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}"

echo "info: building..."
cmake --build "${BUILD_DIR}"

DEFAULT_LV2_DIR="${HOME}/.lv2"
if [[ "${AUTO_INSTALL}" == true ]]; then
    echo "info: installing into default LV2 directory '${DEFAULT_LV2_DIR}'."
    mkdir -p "${DEFAULT_LV2_DIR}"
    cmake --install "${BUILD_DIR}" --prefix "${DEFAULT_LV2_DIR}"
elif [[ -n "${CUSTOM_INSTALL_PREFIX}" ]]; then
    echo "info: installing into '${CUSTOM_INSTALL_PREFIX}'..."
    cmake --install "${BUILD_DIR}" --prefix "${CUSTOM_INSTALL_PREFIX}"
fi

echo "info: build complete."
echo "      Artifacts are in '${BUILD_DIR}/pm-synth.lv2/'."
if pkg-config --exists gtk+-3.0; then
    echo "      Bundle contains both 'pm_synth.so' and 'pm_synth_ui.so'."
else
    echo "      Bundle contains 'pm_synth.so' (UI skipped due to missing GTK+3 headers)."
fi
