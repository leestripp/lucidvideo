#!/bin/bash
# Upload binary release to GitLab
# Usage: ./upload.sh v0.4.1

set -e

VERSION="${1:-0.4.1}"
REPO="leestripp/lucidvideo"
TARBALL="lucid-${VERSION}-linux-x86_64.tar.gz"

echo "=== LucidVideo Binary Release Upload ==="
echo "Version: ${VERSION}"
echo ""

# Check if tarball exists
if [ ! -f "$TARBALL" ]; then
    echo "Error: $TARBALL not found"
    echo "Run build-release.sh first to create the tarball"
    exit 1
fi

# Check for glab CLI
if ! command -v glab &> /dev/null; then
    echo "Error: glab (GitLab CLI) not found"
    echo "Install with: yay -S glab"
    exit 1
fi

# Check authentication
if ! glab auth status &> /dev/null; then
    echo "Error: Not authenticated with GitLab"
    echo "Run: glab auth login"
    exit 1
fi

echo "Creating release v${VERSION}..."
glab release create "v${VERSION}" \
    --title "LucidVideo v${VERSION}" \
    --description "Binary release for Arch Linux. See docs/binary-release.md for installation." \
    --target main \
    "$TARBALL"

echo ""
echo "=== Release Created ==="
echo "View at: https://gitlab.com/${REPO}/-/releases/v${VERSION}"
echo ""
echo "Get download URL with:"
echo "  glab release view v${VERSION}"