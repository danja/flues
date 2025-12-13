#!/bin/bash
# build-synth-pi.sh - Build flues-synth on Raspberry Pi
# Usage: ./scripts/build-synth-pi.sh [VERSION]

set -e

VERSION="${1:-0.1.0}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build-output/synth-v$VERSION"
ARCH=$(uname -m)
OS=$(uname -s | tr '[:upper:]' '[:lower:]')

echo "======================================"
echo "Flues-Synth Build (Raspberry Pi)"
echo "======================================"
echo "Version: $VERSION"
echo "Architecture: $ARCH (should be aarch64 or armv7l)"
echo "OS: $OS"
echo "Build directory: $BUILD_DIR"
echo ""

# Verify we're on ARM
if [[ ! "$ARCH" =~ ^(aarch64|armv7l|arm).*$ ]]; then
    echo "WARNING: This doesn't look like ARM architecture ($ARCH)"
    echo "Are you sure you're running on Raspberry Pi?"
    read -p "Continue anyway? [y/N] " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Create build directory
mkdir -p "$BUILD_DIR"

# Build flues-synth
echo "======================================"
echo "Building Flues-Synth"
echo "======================================"
echo ""

cd "$PROJECT_ROOT/flues-synth"

# Clean and build with Pi-optimized flags
rm -rf builddir
meson setup builddir \
    --buildtype=release \
    -Dc_args="-O3 -mcpu=cortex-a72 -mfpu=neon-fp-armv8 -DNDEBUG" \
    -Dcpp_args="-O3 -mcpu=cortex-a72 -mfpu=neon-fp-armv8 -DNDEBUG"

meson compile -C builddir

# Run tests to verify
echo ""
echo "======================================"
echo "Running Tests"
echo "======================================"
echo ""
meson test -C builddir --print-errorlogs

# Copy binary
cp builddir/flues-synth "$BUILD_DIR/"

# Strip symbols to reduce size
strip "$BUILD_DIR/flues-synth"

BINARY_SIZE=$(du -h "$BUILD_DIR/flues-synth" | cut -f1)

echo ""
echo "======================================"
echo "Copying Documentation"
echo "======================================"
echo ""

# Copy documentation
mkdir -p "$BUILD_DIR/docs"
cp README.md "$BUILD_DIR/docs/FLUES-SYNTH.md"
cp -r docs/* "$BUILD_DIR/docs/"

# Create tarball for transfer back to desktop
echo ""
echo "======================================"
echo "Creating Synth Tarball"
echo "======================================"
echo ""

cd "$PROJECT_ROOT/build-output"
TARBALL="flues-synth-v$VERSION-$OS-$ARCH.tar.gz"
tar czf "$TARBALL" "synth-v$VERSION/"

TARBALL_SIZE=$(du -h "$TARBALL" | cut -f1)

echo "✓ Created synth tarball: $TARBALL ($TARBALL_SIZE)"
echo ""

# Summary
echo "======================================"
echo "Raspberry Pi Build Complete"
echo "======================================"
echo ""
echo "Build directory: $BUILD_DIR"
echo "Tarball: build-output/$TARBALL"
echo "Binary size: $BINARY_SIZE (stripped)"
echo "Tarball size: $TARBALL_SIZE"
echo ""
echo "Flues-Synth:"
file "$BUILD_DIR/flues-synth" | cut -d: -f2-
echo ""
echo "Test Results:"
cd "$PROJECT_ROOT/flues-synth"
meson test -C builddir --print-errorlogs 2>&1 | grep "^Ok:" || echo "See above for test results"
echo ""
echo "Next steps:"
echo "  1. Transfer build back to desktop:"
echo "     # On desktop, run:"
echo "     scp pi@raspberrypi.local:~/flues/build-output/$TARBALL build-output/"
echo ""
echo "  2. Assemble final release on desktop:"
echo "     ./scripts/assemble-release.sh $VERSION"
echo ""
