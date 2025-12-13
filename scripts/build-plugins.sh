#!/bin/bash
# build-plugins.sh - Build LV2 plugins on Ubuntu desktop
# Usage: ./scripts/build-plugins.sh [VERSION]

set -e

VERSION="${1:-0.1.0}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build-output/plugins-v$VERSION"
ARCH=$(uname -m)
OS=$(uname -s | tr '[:upper:]' '[:lower:]')

echo "======================================"
echo "Flues LV2 Plugins Build (Desktop)"
echo "======================================"
echo "Version: $VERSION"
echo "Architecture: $ARCH"
echo "OS: $OS"
echo "Build directory: $BUILD_DIR"
echo ""

# Create build directory
mkdir -p "$BUILD_DIR"

# Function to build LV2 plugin
build_lv2_plugin() {
    local plugin_name=$1
    local plugin_dir="$PROJECT_ROOT/lv2/$plugin_name"

    echo "Building LV2 plugin: $plugin_name"

    if [ ! -d "$plugin_dir" ]; then
        echo "  WARNING: Plugin directory not found: $plugin_dir"
        return 1
    fi

    cd "$plugin_dir"

    # Clean and build with release optimization
    rm -rf build
    cmake -S . -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_FLAGS="-O3 -march=native -DNDEBUG" \
        -DCMAKE_CXX_FLAGS="-O3 -march=native -DNDEBUG"

    cmake --build build --config Release

    # Install to temporary staging directory to create .lv2 bundle
    local staging_dir="$plugin_dir/build/staging"
    rm -rf "$staging_dir"
    cmake --install build --prefix "$staging_dir"

    # Find the .lv2 bundle in staging directory
    local bundle=$(find "$staging_dir" -name "*.lv2" -type d | head -1)

    if [ -z "$bundle" ]; then
        echo "  ERROR: No .lv2 bundle found after install"
        return 1
    fi

    # Copy bundle to build directory
    cp -r "$bundle" "$BUILD_DIR/"

    echo "  ✓ Built and packaged $plugin_name"
    echo ""
}

# Build all LV2 plugins
echo "======================================"
echo "Building LV2 Plugins"
echo "======================================"
echo ""

build_lv2_plugin "disyn"
build_lv2_plugin "floozy"
build_lv2_plugin "chatterbox"
build_lv2_plugin "chatgen"
build_lv2_plugin "drumkit"
build_lv2_plugin "pm-synth"
build_lv2_plugin "flues-control"

# Create tarball for transfer to Pi
echo "======================================"
echo "Creating Plugin Tarball"
echo "======================================"
echo ""

cd "$PROJECT_ROOT/build-output"
TARBALL="flues-plugins-v$VERSION-$OS-$ARCH.tar.gz"
tar czf "$TARBALL" "plugins-v$VERSION/"

TARBALL_SIZE=$(du -h "$TARBALL" | cut -f1)

echo "✓ Created plugin tarball: $TARBALL ($TARBALL_SIZE)"
echo ""

# Summary
echo "======================================"
echo "Desktop Plugin Build Complete"
echo "======================================"
echo ""
echo "Build directory: $BUILD_DIR"
echo "Tarball: build-output/$TARBALL"
echo "Size: $TARBALL_SIZE"
echo ""
echo "LV2 Plugins:"
ls -1 "$BUILD_DIR/"*.lv2 2>/dev/null | sed 's|.*/||' | sed 's/^/  - /'
echo ""
echo "Next steps:"
echo "  1. Transfer to Raspberry Pi:"
echo "     scp build-output/$TARBALL pi@raspberrypi.local:~/"
echo ""
echo "  2. On Raspberry Pi, run:"
echo "     ./scripts/build-synth-pi.sh $VERSION"
echo ""
echo "  3. Transfer flues-synth build back to desktop"
echo ""
echo "  4. Assemble final release:"
echo "     ./scripts/assemble-release.sh $VERSION"
echo ""
