#!/bin/bash
# assemble-release.sh - Assemble final release from desktop plugins + Pi synth
# Usage: ./scripts/assemble-release.sh [VERSION]

set -e

VERSION="${1:-0.1.0}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build-output"
RELEASE_DIR="$PROJECT_ROOT/releases/v$VERSION"
DESKTOP_ARCH=$(uname -m)

echo "======================================"
echo "Flues Release Assembly"
echo "======================================"
echo "Version: $VERSION"
echo "Desktop arch: $DESKTOP_ARCH"
echo "Release directory: $RELEASE_DIR"
echo ""

# Check for required build artifacts
PLUGINS_DIR="$BUILD_DIR/plugins-v$VERSION"
SYNTH_TARBALL=$(find "$BUILD_DIR" -name "flues-synth-v$VERSION-*.tar.gz" | head -1)

if [ ! -d "$PLUGINS_DIR" ]; then
    echo "ERROR: Plugins not found: $PLUGINS_DIR"
    echo "Run: ./scripts/build-plugins.sh $VERSION"
    exit 1
fi

if [ -z "$SYNTH_TARBALL" ] || [ ! -f "$SYNTH_TARBALL" ]; then
    echo "ERROR: Flues-synth tarball not found in $BUILD_DIR"
    echo ""
    echo "Expected workflow:"
    echo "  1. Build plugins on desktop: ./scripts/build-plugins.sh $VERSION"
    echo "  2. Transfer repo to Pi: scp -r flues/ pi@raspberrypi.local:~/"
    echo "  3. Build synth on Pi: ssh pi@raspberrypi.local"
    echo "                        cd flues && ./scripts/build-synth-pi.sh $VERSION"
    echo "  4. Transfer synth build back: scp pi@raspberrypi.local:~/flues/build-output/flues-synth-*.tar.gz build-output/"
    echo "  5. Assemble release: ./scripts/assemble-release.sh $VERSION"
    exit 1
fi

# Extract synth architecture from tarball name
PI_ARCH=$(basename "$SYNTH_TARBALL" | grep -oP '(?<=-linux-)[^.]+')
echo "Raspberry Pi arch: $PI_ARCH"
echo ""

# Create release directory structure
echo "Creating release directory structure..."
rm -rf "$RELEASE_DIR"
mkdir -p "$RELEASE_DIR"/{lv2-plugins,flues-synth,docs}

# Copy LV2 plugins
echo "Copying LV2 plugins..."
cp -r "$PLUGINS_DIR/"*.lv2 "$RELEASE_DIR/lv2-plugins/"

# Extract and copy flues-synth
echo "Extracting flues-synth build..."
cd "$BUILD_DIR"
tar xzf "$(basename "$SYNTH_TARBALL")"
SYNTH_DIR=$(tar tzf "$(basename "$SYNTH_TARBALL")" | head -1 | cut -d/ -f1)
cp "$SYNTH_DIR/flues-synth" "$RELEASE_DIR/flues-synth/"
cp -r "$SYNTH_DIR/docs/"* "$RELEASE_DIR/docs/"

# Copy project documentation
echo "Copying project documentation..."
cp "$PROJECT_ROOT/README.md" "$RELEASE_DIR/docs/"
cp "$PROJECT_ROOT/CHANGELOG.md" "$RELEASE_DIR/docs/" 2>/dev/null || true

# Create installation script
echo "Creating install script..."
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
echo "Creating uninstall script..."
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
rm -rf /usr/local/lib/lv2/pm-synth.lv2
rm -rf /usr/local/lib/lv2/flues-control.lv2

echo "Removing flues-synth..."
rm -f /usr/local/bin/flues-synth

echo "✓ Uninstallation complete!"
EOF

chmod +x "$RELEASE_DIR/uninstall.sh"

# Create README for release
echo "Creating release README..."
cat > "$RELEASE_DIR/README.md" << EOF
# Flues Project - Release v$VERSION

Binary distribution for desktop ($DESKTOP_ARCH) + Raspberry Pi ($PI_ARCH).

## Contents

- **lv2-plugins/** - Seven LV2 instrument plugins (built on $DESKTOP_ARCH)
  - disyn.lv2 - Distortion synthesis (7 algorithms)
  - floozy.lv2 - Hybrid distortion + physical modeling
  - chatterbox.lv2 - Formant speech synthesizer
  - chatgen.lv2 - Text-to-speech MIDI generator
  - drumkit.lv2 - Industrial drum synthesizer (8 voices)
  - pm-synth.lv2 - Physical modeling synthesizer
  - flues-control.lv2 - MIDI CC controller with 29 programs

- **flues-synth/** - Headless Raspberry Pi synthesizer binary (built on $PI_ARCH)
  - Optimized for Raspberry Pi 4 (Cortex-A72)
  - 29 MIDI programs with dynamic slider remapping
  - ALSA MIDI/audio backends

- **docs/** - Complete documentation

## Installation

### Desktop (LV2 Plugins Only)

\`\`\`bash
# Install plugins
sudo cp -r lv2-plugins/* /usr/local/lib/lv2/

# Or user install
cp -r lv2-plugins/* ~/.lv2/
\`\`\`

### Raspberry Pi (Flues-Synth Only)

\`\`\`bash
# Install synth
sudo cp flues-synth/flues-synth /usr/local/bin/
sudo chmod +x /usr/local/bin/flues-synth

# Run
flues-synth
\`\`\`

### Full Install (Both)

\`\`\`bash
sudo ./install.sh
\`\`\`

## Usage

### LV2 Plugins (Desktop)

Load in any LV2 host (Ardour, Reaper, Carla):

\`\`\`bash
lv2ls | grep flues
jalv.gtk https://danja.github.io/flues/plugins/disyn
\`\`\`

### Flues-Synth (Raspberry Pi)

\`\`\`bash
flues-synth              # Auto-detect audio device
flues-synth hw:2,0       # Specify device
FLUES_MIDI_DEBUG=1 flues-synth  # Enable debug
\`\`\`

## Build Information

- Version: $VERSION
- Desktop arch: $DESKTOP_ARCH
- Raspberry Pi arch: $PI_ARCH
- Build type: Release (O3 optimization)
- Built: $(date -u +"%Y-%m-%d %H:%M:%S UTC")

## Documentation

See \`docs/\` directory for complete reference.

## Uninstall

\`\`\`bash
sudo ./uninstall.sh
\`\`\`
EOF

# Create tarballs
echo ""
echo "======================================"
echo "Creating Release Tarballs"
echo "======================================"
echo ""

cd "$PROJECT_ROOT/releases"

# Multi-architecture tarball (plugins + synth)
FULL_TARBALL="flues-v$VERSION-multi-arch.tar.gz"
tar czf "$FULL_TARBALL" "v$VERSION/"
sha256sum "$FULL_TARBALL" > "$FULL_TARBALL.sha256"
md5sum "$FULL_TARBALL" > "$FULL_TARBALL.md5"
FULL_SIZE=$(du -h "$FULL_TARBALL" | cut -f1)
echo "✓ Full release: $FULL_TARBALL ($FULL_SIZE)"

# Plugins-only tarball (desktop)
PLUGINS_TARBALL="flues-plugins-v$VERSION-$DESKTOP_ARCH.tar.gz"
cd "v$VERSION"
tar czf "../$PLUGINS_TARBALL" lv2-plugins/ docs/ README.md
cd ..
sha256sum "$PLUGINS_TARBALL" > "$PLUGINS_TARBALL.sha256"
md5sum "$PLUGINS_TARBALL" > "$PLUGINS_TARBALL.md5"
PLUGINS_SIZE=$(du -h "$PLUGINS_TARBALL" | cut -f1)
echo "✓ Plugins only: $PLUGINS_TARBALL ($PLUGINS_SIZE)"

# Synth-only tarball (Pi)
SYNTH_TARBALL_NAME="flues-synth-v$VERSION-$PI_ARCH.tar.gz"
cd "v$VERSION"
tar czf "../$SYNTH_TARBALL_NAME" flues-synth/ docs/ README.md install.sh uninstall.sh
cd ..
sha256sum "$SYNTH_TARBALL_NAME" > "$SYNTH_TARBALL_NAME.sha256"
md5sum "$SYNTH_TARBALL_NAME" > "$SYNTH_TARBALL_NAME.md5"
SYNTH_SIZE=$(du -h "$SYNTH_TARBALL_NAME" | cut -f1)
echo "✓ Synth only: $SYNTH_TARBALL_NAME ($SYNTH_SIZE)"

echo ""

# Summary
echo "======================================"
echo "Release Assembly Complete"
echo "======================================"
echo ""
echo "Release directory: $RELEASE_DIR"
echo ""
echo "Tarballs created:"
echo "  - $FULL_TARBALL ($FULL_SIZE) - Full multi-arch release"
echo "  - $PLUGINS_TARBALL ($PLUGINS_SIZE) - Desktop plugins only"
echo "  - $SYNTH_TARBALL_NAME ($SYNTH_SIZE) - Raspberry Pi synth only"
echo ""
echo "LV2 Plugins ($DESKTOP_ARCH):"
ls -1 "$RELEASE_DIR/lv2-plugins/"*.lv2 2>/dev/null | sed 's|.*/||' | sed 's/^/  - /'
echo ""
echo "Flues-Synth ($PI_ARCH):"
file "$RELEASE_DIR/flues-synth/flues-synth" | cut -d: -f2- | sed 's/^/  /'
echo ""
echo "Documentation:"
ls -1 "$RELEASE_DIR/docs/" | sed 's/^/  - /'
echo ""
echo "Checksums:"
echo "  Full: $(cat "$FULL_TARBALL.sha256" | cut -d' ' -f1)"
echo "  Plugins: $(cat "$PLUGINS_TARBALL.sha256" | cut -d' ' -f1)"
echo "  Synth: $(cat "$SYNTH_TARBALL_NAME.sha256" | cut -d' ' -f1)"
echo ""
echo "======================================"
echo "Next Steps"
echo "======================================"
echo ""
echo "1. Test installation:"
echo "   cd releases/v$VERSION && sudo ./install.sh"
echo ""
echo "2. Create GitHub release:"
echo "   git tag v$VERSION && git push origin v$VERSION"
echo "   ./scripts/create-github-release.sh $VERSION"
echo ""
echo "3. Upload all tarballs to GitHub release"
echo ""
