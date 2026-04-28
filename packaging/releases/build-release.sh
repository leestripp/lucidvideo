#!/bin/bash
# Build binary release for GitLab upload
# Usage: ./build-release.sh [version]

set -e

VERSION="${1:-0.4.1}"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
RELEASE_DIR="$(dirname "${BASH_SOURCE[0]}")/${VERSION}"
OUTPUT_TARBALL="$(dirname "${BASH_SOURCE[0]}")/lucid-${VERSION}-linux-x86_64.tar.gz"

echo "=== Building LucidVideo Binary Release ==="
echo "Version: ${VERSION}"
echo "Project: ${PROJECT_ROOT}"
echo ""

# Clean and prepare release directory
echo "Preparing release directory..."
rm -rf "$RELEASE_DIR"
mkdir -p "$RELEASE_DIR"

# Check if binary exists
if [ ! -f "${BUILD_DIR}/lucidvideo" ]; then
    echo "Error: Binary not found at ${BUILD_DIR}/lucidvideo"
    echo "Build the project first:"
    echo "  mkdir build && cd build"
    echo "  cmake .. -DCMAKE_BUILD_TYPE=Release"
    echo "  cmake --build . -j$(nproc)"
    exit 1
fi

# Copy files
echo "Copying files..."
cp "${BUILD_DIR}/lucidvideo" "$RELEASE_DIR/"
cp "${PROJECT_ROOT}/data/lucid.desktop" "$RELEASE_DIR/"
cp "${PROJECT_ROOT}/resources/icons/lucid-app.svg" "$RELEASE_DIR/lucid.svg"

# Create tarball
echo "Creating tarball..."
rm -f "$OUTPUT_TARBALL"
tar -czvf "$OUTPUT_TARBALL" -C "$RELEASE_DIR" .

echo ""
echo "=== Build Complete ==="
echo "Release files: $RELEASE_DIR"
echo "Tarball: $OUTPUT_TARBALL"
echo "Size: $(du -h "$OUTPUT_TARBALL" | cut -f1)"
echo ""
echo "To upload to GitLab:"
echo "  cd $(dirname "$OUTPUT_TARBALL")"
echo "  ./upload.sh v${VERSION}"