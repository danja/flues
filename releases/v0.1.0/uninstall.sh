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
