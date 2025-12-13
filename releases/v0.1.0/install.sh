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
