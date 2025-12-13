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
OS=$(uname -s | tr '[:upper:]' '[:lower:]')
TARBALL="flues-v$VERSION-$OS-$ARCH.tar.gz"

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

# Check if tarball exists
if [ ! -f "$PROJECT_ROOT/releases/$TARBALL" ]; then
    echo "ERROR: Release tarball not found: $TARBALL"
    echo "Run: ./scripts/build-release.sh $VERSION"
    exit 1
fi

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

# Create release
gh release create "v$VERSION" \
    --title "Flues v$VERSION - Experimental Synthesis Plugins" \
    --notes-file "$RELEASE_NOTES" \
    "$PROJECT_ROOT/releases/$TARBALL" \
    "$PROJECT_ROOT/releases/$TARBALL.sha256" \
    "$PROJECT_ROOT/releases/$TARBALL.md5"

echo ""
echo "======================================"
echo "✓ GitHub Release Created!"
echo "======================================"
echo ""
echo "Release URL:"
gh release view "v$VERSION" --web

echo ""
echo "Uploaded assets:"
echo "  - $TARBALL"
echo "  - $TARBALL.sha256"
echo "  - $TARBALL.md5"
echo ""
echo "To edit release:"
echo "  gh release edit v$VERSION"
echo ""
echo "To delete release (if needed):"
echo "  gh release delete v$VERSION"
echo ""
