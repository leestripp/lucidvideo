#!/bin/bash
# LucidVideo Makeself Installer Build Script
# Creates a self-contained .run installer

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
OUTPUT_DIR="$SCRIPT_DIR/output"
TEMP_DIR="$SCRIPT_DIR/temp"

VERSION=$(grep -E "^project.*VERSION" "$PROJECT_ROOT/CMakeLists.txt" | sed 's/.*VERSION \([0-9.]*\).*/\1/')
ARCH=$(uname -m)

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
echo_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
echo_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Clean up
echo_info "Cleaning up..."
rm -rf "$OUTPUT_DIR" "$TEMP_DIR"
mkdir -p "$OUTPUT_DIR" "$TEMP_DIR"

# Step 1: Build the application
echo_info "Step 1: Building LucidVideo v${VERSION}..."
cd "$PROJECT_ROOT"
if [ ! -f "$BUILD_DIR/lucidvideo" ]; then
    echo_info "Running CMake..."
    cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1
fi
cmake --build "$BUILD_DIR" -j$(nproc) > /dev/null 2>&1

if [ ! -f "$BUILD_DIR/lucidvideo" ]; then
    echo_error "Build failed - lucidvideo binary not found"
    exit 1
fi
echo_info "Build complete!"

# Step 2: Create package directory structure
echo_info "Step 2: Creating package structure..."
PKG_DIR="$TEMP_DIR/lucidvideo-$VERSION"
mkdir -p "$PKG_DIR/usr/bin"
mkdir -p "$PKG_DIR/usr/share/applications"
mkdir -p "$PKG_DIR/usr/share/icons/hicolor/scalable/apps"

# Copy binary
cp "$BUILD_DIR/lucidvideo" "$PKG_DIR/usr/bin/"
chmod +x "$PKG_DIR/usr/bin/lucidvideo"

# Copy desktop file
cp "$PROJECT_ROOT/data/lucid.desktop" "$PKG_DIR/usr/share/applications/"
cp "$PROJECT_ROOT/resources/icons/lucid-app.svg" "$PKG_DIR/usr/share/icons/hicolor/scalable/apps/lucid.svg"

# Create install script
cat > "$PKG_DIR/install.sh" << 'INSTALLSCRIPT'
#!/bin/bash
# LucidVideo Installation Script

VERSION="0.4.1"
INSTALL_DIR="$HOME/.local/lucidvideo"

echo "Installing LucidVideo v$VERSION to $INSTALL_DIR..."

# Create installation directory
mkdir -p "$INSTALL_DIR/bin"
mkdir -p "$INSTALL_DIR/share/applications"
mkdir -p "$INSTALL_DIR/share/icons"

# Copy files
cp -r usr/bin/lucidvideo "$INSTALL_DIR/bin/"
cp usr/share/applications/lucid.desktop "$INSTALL_DIR/share/applications/"
cp usr/share/icons/hicolor/scalable/apps/lucid.svg "$INSTALL_DIR/share/icons/"

# Make executable
chmod +x "$INSTALL_DIR/bin/lucidvideo"

# Create launcher script
cat > "$INSTALL_DIR/lucidvideo.sh" << 'LAUNCHER'
#!/bin/bash
export LD_LIBRARY_PATH="$HOME/.local/lucidvideo/lib:$LD_LIBRARY_PATH"
"$HOME/.local/lucidvideo/bin/lucidvideo" "$@"
LAUNCHER
chmod +x "$INSTALL_DIR/lucidvideo.sh"

# Create desktop integration
mkdir -p "$HOME/.local/share/applications"
cat > "$HOME/.local/share/applications/lucid.desktop" << 'DESKTOP'
[Desktop Entry]
Name=LucidVideo
Comment=Intelligent video library with AI-powered search
Exec=$HOME/.local/lucidvideo/lucidvideo.sh %U
Icon=$HOME/.local/lucidvideo/share/icons/lucid.svg
Type=Application
Categories=Video;AudioVideo;Audio;VideoEditor;
MimeType=video/x-matroska;video/x-msvideo;video/x-flv;video/mp4;video/mpeg;video/ogg;video/quicktime;video/x-ms-wmv;video/vnd.rn-realvideo;video/x-sgi-movie;video/3gpp;video/3gpp2;
StartupNotify=true
StartupWMClass=lucidvideo
DESKTOP

# Update icon cache
if command -v gtk-update-icon-cache &> /dev/null; then
    gtk-update-icon-cache -f "$HOME/.local/lucidvideo/share/icons/" 2>/dev/null || true
fi

echo ""
echo "✓ LucidVideo installed successfully!"
echo ""
echo "To run: $INSTALL_DIR/lucidvideo.sh"
echo ""
echo "Tip: Add $INSTALL_DIR to your PATH for easier access."
INSTALLSCRIPT
chmod +x "$PKG_DIR/install.sh"

# Step 3: Create makeself archive
echo_info "Step 3: Creating .run installer..."
cd "$TEMP_DIR"

/usr/bin/makeself \
    --gzip \
    --notemp \
    --chown \
    --sha256 \
    --header "/usr/share/makeself/makeself-header.sh" \
    "$PKG_DIR" \
    "$OUTPUT_DIR/LucidVideo-${VERSION}-${ARCH}.run" \
    "LucidVideo v${VERSION} - Intelligent video library with AI search" \
    "./install.sh"

chmod +x "$OUTPUT_DIR/LucidVideo-${VERSION}-${ARCH}.run"

echo ""
echo_info "============================================"
echo_info "Build complete!"
echo_info "============================================"
echo ""
echo "Output: $OUTPUT_DIR/LucidVideo-${VERSION}-${ARCH}.run"
ls -lh "$OUTPUT_DIR/LucidVideo-${VERSION}-${ARCH}.run"
echo ""
echo "Usage: ./LucidVideo-${VERSION}-${ARCH}.run"
echo ""