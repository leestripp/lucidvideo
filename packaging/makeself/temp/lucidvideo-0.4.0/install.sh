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
