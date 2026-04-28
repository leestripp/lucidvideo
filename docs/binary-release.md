# Binary Release Process

This document explains how to create and publish a binary release of LucidVideo on GitLab for Arch Linux installation.

## Overview

The binary release contains a pre-built executable and supporting files that can be downloaded and installed on Arch Linux without compiling from source.

## Release Structure

```
lucid-{version}-linux-x86_64.tar.gz
├── lucidvideo          # Main executable
├── lucid.desktop       # Desktop entry file
└── lucid.svg           # Application icon
```

## Creating the Release

### Step 1: Build the Binary

On Arch Linux:

```bash
rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

### Step 2: Package the Files

Create a temporary release directory:

```bash
mkdir release-build
cp build/lucidvideo release-build/
cp data/lucid.desktop release-build/
cp resources/icons/lucid-app.svg release-build/lucid.svg
```

### Step 3: Create the Tarball

```bash
tar -czvf lucid-{version}-linux-x86_64.tar.gz -C release-build .
```

### Step 4: Upload to GitLab

1. Go to: https://gitlab.com/leestripp/lucidvideo/-/releases/new
2. Create tag `v{version}` and release
3. Attach the tarball as a release asset

After upload, the download URL will be:
```
https://gitlab.com/leestripp/lucidvideo/-/releases/v{version}/downloads/lucid-{version}-linux-x86_64.tar.gz
```

## Installation

### Using PKGBUILD

The AUR package `lucidvideo-bin` uses this format:

```bash
source=("https://gitlab.com/leestripp/lucidvideo/-/releases/v${pkgver}/downloads/lucid-${pkgver}-linux-x86_64.tar.gz")
```

### Manual Installation

```bash
tar -xzf lucid-{version}-linux-x86_64.tar.gz
sudo cp lucidvideo /usr/bin/
sudo cp lucid.desktop /usr/share/applications/
sudo cp lucid.svg /usr/share/icons/hicolor/scalable/apps/
```

## Requirements

- gtk4
- libadwaita
- sqlite3
- curl
- json-glib
- gst-libav
- gst-plugins-base
- gst-plugins-good

## Related Documentation

- [AUR Binary Package Guide](aur-bin.md) - PKGBUILD details and AUR submission
- [Release Notes](release.md) - Version changelog