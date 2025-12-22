#!/bin/bash
# build-release.sh - Build optimized releases for GitHub publication
# Usage: ./scripts/build-release.sh [VERSION]

set -e  # Exit on error

VERSION="${1:-0.1.0}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
RELEASE_DIR="$PROJECT_ROOT/releases/v$VERSION"
ARCH=$(uname -m)
OS=$(uname -s | tr '[:upper:]' '[:lower:]')

echo "======================================"
echo "Flues Project Release Builder"
echo "======================================"
echo "Version: $VERSION"
echo "Architecture: $ARCH"
echo "OS: $OS"
echo "Release directory: $RELEASE_DIR"
echo ""

# Create release directory structure
mkdir -p "$RELEASE_DIR"/{lv2-plugins,flues-synth,docs}

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

    # Copy bundle to release directory
    cp -r "$bundle" "$RELEASE_DIR/lv2-plugins/"

    echo "  ✓ Built and packaged $plugin_name"
    echo ""
}

# Function to build flues-synth
build_flues_synth() {
    echo "Building flues-synth"

    local synth_dir="$PROJECT_ROOT/flues-synth"
    cd "$synth_dir"

    # Clean and build with release optimization
    rm -rf builddir
    meson setup builddir \
        --buildtype=release \
        -Dc_args="-O3 -march=native -DNDEBUG" \
        -Dcpp_args="-O3 -march=native -DNDEBUG"

    meson compile -C builddir

    # Run tests to verify
    echo "  Running tests..."
    meson test -C builddir --print-errorlogs

    # Copy binary
    cp builddir/flues-synth "$RELEASE_DIR/flues-synth/"

    # Strip symbols to reduce size
    strip "$RELEASE_DIR/flues-synth/flues-synth"

    echo "  ✓ Built and packaged flues-synth"
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
build_lv2_plugin "euclid"
build_lv2_plugin "pm-synth"
build_lv2_plugin "flues-control"

# Build flues-synth
echo "======================================"
echo "Building Flues-Synth"
echo "======================================"
echo ""

build_flues_synth

# Copy documentation
echo "======================================"
echo "Copying Documentation"
echo "======================================"
echo ""

cp "$PROJECT_ROOT/README.md" "$RELEASE_DIR/docs/"
cp "$PROJECT_ROOT/flues-synth/README.md" "$RELEASE_DIR/docs/FLUES-SYNTH.md"
cp -r "$PROJECT_ROOT/flues-synth/docs/"* "$RELEASE_DIR/docs/"

# Create installation script
cat > "$RELEASE_DIR/install.sh" << 'EOF'
#!/bin/bash
# install.sh - Install Flues LV2 plugins and flues-synth
# Usage: sudo ./install.sh

set -e

if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (use sudo)"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "Installing Flues LV2 plugins to /usr/local/lib/lv2..."
mkdir -p /usr/local/lib/lv2
cp -r "$SCRIPT_DIR/lv2-plugins/"* /usr/local/lib/lv2/

echo "Installing flues-synth to /usr/local/bin..."
mkdir -p /usr/local/bin
cp "$SCRIPT_DIR/flues-synth/flues-synth" /usr/local/bin/
chmod +x /usr/local/bin/flues-synth

echo ""
echo "✓ Installation complete!"
echo ""
echo "LV2 plugins installed:"
ls -1 /usr/local/lib/lv2/*.lv2 2>/dev/null | sed 's|/usr/local/lib/lv2/||' | sed 's/^/  - /'
echo ""
echo "To verify plugin installation:"
echo "  lv2ls | grep flues"
echo ""
echo "To run flues-synth:"
echo "  flues-synth [hw:X,Y]"
echo ""
EOF

chmod +x "$RELEASE_DIR/install.sh"

# Create uninstall script
cat > "$RELEASE_DIR/uninstall.sh" << 'EOF'
#!/bin/bash
# uninstall.sh - Remove Flues LV2 plugins and flues-synth

set -e

if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (use sudo)"
    exit 1
fi

echo "Removing Flues LV2 plugins..."
rm -rf /usr/local/lib/lv2/disyn.lv2
rm -rf /usr/local/lib/lv2/floozy.lv2
rm -rf /usr/local/lib/lv2/chatterbox.lv2
rm -rf /usr/local/lib/lv2/chatgen.lv2
rm -rf /usr/local/lib/lv2/drumkit.lv2
rm -rf /usr/local/lib/lv2/euclid.lv2
rm -rf /usr/local/lib/lv2/pm-synth.lv2
rm -rf /usr/local/lib/lv2/flues-control.lv2

echo "Removing flues-synth..."
rm -f /usr/local/bin/flues-synth

echo "✓ Uninstallation complete!"
EOF

chmod +x "$RELEASE_DIR/uninstall.sh"

# Create README for release
cat > "$RELEASE_DIR/README.md" << EOF
# Flues Project - Release v$VERSION

Binary distribution for $OS-$ARCH.

## Contents

- **lv2-plugins/** - Eight LV2 instrument plugins
  - disyn.lv2 - Distortion synthesis (7 algorithms)
  - floozy.lv2 - Hybrid distortion + physical modeling
  - chatterbox.lv2 - Formant speech synthesizer
  - chatgen.lv2 - Text-to-speech MIDI generator
  - drumkit.lv2 - Industrial drum synthesizer (11 voices)
  - euclid.lv2 - Euclidean rhythm generator
  - pm-synth.lv2 - Physical modeling synthesizer
  - flues-control.lv2 - MIDI CC controller with 29 programs

- **flues-synth/** - Headless Raspberry Pi synthesizer binary
  - Monophonic synthesis engine
  - 29 MIDI programs with dynamic slider remapping
  - ALSA MIDI/audio backends
  - Designed for Raspberry Pi 4 deployment

- **docs/** - Complete documentation

## Installation

### Quick Install (requires root)

\`\`\`bash
sudo ./install.sh
\`\`\`

This installs:
- LV2 plugins to \`/usr/local/lib/lv2/\`
- flues-synth to \`/usr/local/bin/\`

### Manual Install

**LV2 Plugins:**
\`\`\`bash
cp -r lv2-plugins/* ~/.lv2/
\`\`\`

**Flues-Synth:**
\`\`\`bash
sudo cp flues-synth/flues-synth /usr/local/bin/
sudo chmod +x /usr/local/bin/flues-synth
\`\`\`

## Usage

### LV2 Plugins

Load in any LV2 host (Ardour, Reaper, Carla, Jalv):

\`\`\`bash
# Verify installation
lv2ls | grep flues

# Test individual plugin
jalv.gtk https://danja.github.io/flues/plugins/disyn
\`\`\`

### Flues-Synth

\`\`\`bash
# Auto-detect audio device
flues-synth

# Specify device
flues-synth hw:2,0

# Enable MIDI debug
FLUES_MIDI_DEBUG=1 flues-synth
\`\`\`

**MIDI Control:**
- Program Change 0-28: Select synthesis program
- CC 73, 72, 28, 30, 74, 71, 1, 27, 7: Hardware sliders (dynamically remapped)
- Notes 60-84: Chromatic keyboard (C4-C6)

## Documentation

See \`docs/\` directory for:
- \`FLUES-SYNTH.md\` - Synthesis engine overview
- \`algorithms.md\` - 17 distortion synthesis algorithms
- \`midi.md\` - MIDI CC mapping reference
- \`PROGRAM_CHANGE.md\` - 29 program descriptions
- \`PROGRAM6_FIX.md\` - Race condition fix notes

## Build Information

- Version: $VERSION
- Architecture: $ARCH
- OS: $OS
- Build type: Release (-O3 -march=native)
- Built: $(date -u +"%Y-%m-%d %H:%M:%S UTC")

## Requirements

- ALSA library (libasound2)
- LV2 host application (for plugins)
- X11 and Cairo (for plugin UIs)

## Uninstall

\`\`\`bash
sudo ./uninstall.sh
\`\`\`

## License

See individual plugin documentation for license information.

## More Information

- Project repository: https://github.com/danja/flues
- Documentation: https://danja.github.io/flues/
EOF

# Create tarball
echo "======================================"
echo "Creating Release Tarball"
echo "======================================"
echo ""

cd "$PROJECT_ROOT/releases"
TARBALL="flues-v$VERSION-$OS-$ARCH.tar.gz"

tar czf "$TARBALL" "v$VERSION/"

# Generate checksums
sha256sum "$TARBALL" > "$TARBALL.sha256"
md5sum "$TARBALL" > "$TARBALL.md5"

TARBALL_SIZE=$(du -h "$TARBALL" | cut -f1)

echo "✓ Created release tarball: $TARBALL ($TARBALL_SIZE)"
echo ""

# Summary
echo "======================================"
echo "Release Build Summary"
echo "======================================"
echo ""
echo "Release directory: $RELEASE_DIR"
echo "Tarball: releases/$TARBALL"
echo "Size: $TARBALL_SIZE"
echo ""
echo "LV2 Plugins:"
ls -1 "$RELEASE_DIR/lv2-plugins/"*.lv2 2>/dev/null | sed 's|.*/||' | sed 's/^/  - /'
echo ""
echo "Flues-Synth:"
echo "  - $(file "$RELEASE_DIR/flues-synth/flues-synth" | cut -d: -f2-)"
echo ""
echo "Documentation:"
ls -1 "$RELEASE_DIR/docs/" | sed 's/^/  - /'
echo ""
echo "Checksums:"
echo "  SHA256: $(cat "$TARBALL.sha256" | cut -d' ' -f1)"
echo "  MD5: $(cat "$TARBALL.md5" | cut -d' ' -f1)"
echo ""
echo "======================================"
echo "✓ Release build complete!"
echo "======================================"
echo ""
echo "Next steps:"
echo "  1. Test installation: cd releases/v$VERSION && sudo ./install.sh"
echo "  2. Create GitHub release: gh release create v$VERSION"
echo "  3. Upload tarball: gh release upload v$VERSION releases/$TARBALL"
echo ""
