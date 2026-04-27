# LucidVideo Release v0.4.1

## Release Information

| Item | Value |
|------|-------|
| **Version** | 0.4.1 |
| **Package Name** | lucidvideo |
| **GitLab Release URL** | https://gitlab.com/leestripp/lucidvideo/-/releases/0.4.1 |
| **GitLab Repository** | https://gitlab.com/leestripp/lucidvideo |

## What's New

### Submodules Converted to Regular Folders
- **Important**: The `third_party/` directory is now included directly in the repository
- Git submodules have been converted to regular folders for archive downloads
- This allows AUR packages to work with archive downloads instead of git clones

### Dependencies Included
The following third-party libraries are now included:
- `whisper.cpp` - Speech recognition
- `ggml` - Tensor library (from llama.cpp)
- `llama.cpp` - LLM inference
- `sqlite-vec` - Vector search for SQLite

### Breaking Change
- Whisper.cpp now uses ggml from llama.cpp instead of the separate ggml submodule due to version conflicts

### Executable Renamed
- Changed from `lucid` to `lucidvideo`

## Installation

### Arch Linux (AUR)

```bash
# Install from AUR
yay -S lucidvideo
```

### Building from Source

```bash
# Clone the repository
git clone https://gitlab.com/leestripp/lucidvideo.git
cd lucidvideo

# Checkout the release tag
git checkout v0.4.1

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# Run
./build/lucidvideo
```

## Links

- [GitLab Repository](https://gitlab.com/leestripp/lucidvideo)
- [Release Notes](https://gitlab.com/leestripp/lucidvideo/-/releases/0.4.1)
- [AUR Package](https://aur.archlinux.org/packages/lucidvideo)