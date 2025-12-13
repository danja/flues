#!/bin/bash
# create-github-release.sh - Create GitHub release with artifacts
# Usage: ./scripts/create-github-release.sh VERSION

set -e

VERSION="${1}"

if [ -z "$VERSION" ]; then
    echo "Usage: $0 VERSION"
    echo "Example: $0 0.1.0"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
RELEASE_DIR="$PROJECT_ROOT/releases/v$VERSION"
ARCH=$(uname -m)
DESKTOP_ARCH=$ARCH  # Desktop architecture for tarball names
OS=$(uname -s | tr '[:upper:]' '[:lower:]')
TARBALL="flues-v$VERSION-$OS-$ARCH.tar.gz"  # Legacy single-platform tarball name

echo "======================================"
echo "GitHub Release Creator"
echo "======================================"
echo "Version: $VERSION"
echo ""

# Check if gh CLI is installed
if ! command -v gh &> /dev/null; then
    echo "ERROR: GitHub CLI (gh) is not installed"
    echo "Install with: sudo apt install gh"
    exit 1
fi

# Check if authenticated
if ! gh auth status &> /dev/null; then
    echo "ERROR: Not authenticated with GitHub"
    echo "Run: gh auth login"
    exit 1
fi

# Check for multi-arch release (new build system)
MULTI_ARCH_TARBALL="flues-v$VERSION-multi-arch.tar.gz"
PLUGINS_TARBALL="flues-plugins-v$VERSION-$DESKTOP_ARCH.tar.gz"
SYNTH_TARBALL=$(find "$PROJECT_ROOT/releases" -name "flues-synth-v$VERSION-*.tar.gz" 2>/dev/null | head -1)

# Check if release exists
if [ ! -f "$PROJECT_ROOT/releases/$MULTI_ARCH_TARBALL" ]; then
    echo "ERROR: Release tarball not found: $MULTI_ARCH_TARBALL"
    echo ""
    echo "Expected workflow:"
    echo "  1. Build plugins on desktop: ./scripts/build-plugins.sh $VERSION"
    echo "  2. Build synth on Pi: ./scripts/build-synth-pi.sh $VERSION"
    echo "  3. Assemble release: ./scripts/assemble-release.sh $VERSION"
    echo "  4. Create GitHub release: ./scripts/create-github-release.sh $VERSION"
    echo ""
    echo "Or use single-platform build:"
    echo "  ./scripts/build-release.sh $VERSION"
    exit 1
fi

# Determine what tarballs to upload
TARBALLS_TO_UPLOAD=()
TARBALLS_TO_UPLOAD+=("$PROJECT_ROOT/releases/$MULTI_ARCH_TARBALL")
TARBALLS_TO_UPLOAD+=("$PROJECT_ROOT/releases/$MULTI_ARCH_TARBALL.sha256")
TARBALLS_TO_UPLOAD+=("$PROJECT_ROOT/releases/$MULTI_ARCH_TARBALL.md5")

# Add plugins-only tarball if exists
if [ -f "$PROJECT_ROOT/releases/$PLUGINS_TARBALL" ]; then
    TARBALLS_TO_UPLOAD+=("$PROJECT_ROOT/releases/$PLUGINS_TARBALL")
    TARBALLS_TO_UPLOAD+=("$PROJECT_ROOT/releases/$PLUGINS_TARBALL.sha256")
    TARBALLS_TO_UPLOAD+=("$PROJECT_ROOT/releases/$PLUGINS_TARBALL.md5")
fi

# Add synth-only tarball if exists
if [ -n "$SYNTH_TARBALL" ] && [ -f "$SYNTH_TARBALL" ]; then
    TARBALLS_TO_UPLOAD+=("$SYNTH_TARBALL")
    TARBALLS_TO_UPLOAD+=("$SYNTH_TARBALL.sha256")
    TARBALLS_TO_UPLOAD+=("$SYNTH_TARBALL.md5")
fi

TARBALL="$MULTI_ARCH_TARBALL"  # Primary tarball for compatibility

# Generate release notes from template
RELEASE_NOTES="$PROJECT_ROOT/releases/v$VERSION/RELEASE_NOTES.md"
cp "$SCRIPT_DIR/release-notes-template.md" "$RELEASE_NOTES"

# Substitute version placeholders
sed -i "s/{VERSION}/$VERSION/g" "$RELEASE_NOTES"
sed -i "s/{ARCH}/$ARCH/g" "$RELEASE_NOTES"

echo "Release notes generated: $RELEASE_NOTES"
echo ""
echo "Please review and edit the release notes before continuing."
echo "Press Enter to open in editor, or Ctrl+C to abort."
read

# Open in editor
${EDITOR:-nano} "$RELEASE_NOTES"

echo ""
echo "Creating GitHub release v$VERSION..."
echo ""

# Create release with all tarballs
gh release create "v$VERSION" \
    --title "Flues v$VERSION - Experimental Synthesis Plugins" \
    --notes-file "$RELEASE_NOTES" \
    "${TARBALLS_TO_UPLOAD[@]}"

echo ""
echo "======================================"
echo "✓ GitHub Release Created!"
echo "======================================"
echo ""
echo "Release URL:"
gh release view "v$VERSION" --web

echo ""
echo "Uploaded assets:"
for tarball in "${TARBALLS_TO_UPLOAD[@]}"; do
    echo "  - $(basename "$tarball")"
done
echo ""
echo "To edit release:"
echo "  gh release edit v$VERSION"
echo ""
echo "To delete release (if needed):"
echo "  gh release delete v$VERSION"
echo ""
