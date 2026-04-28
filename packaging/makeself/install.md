# LucidVideo Installation Dependencies

LucidVideo requires the following runtime dependencies. Since the application is built with **statically linked third-party libraries** (whisper.cpp, ggml, llama.cpp), only the system libraries need to be installed.

## Ubuntu / Debian

```bash
sudo apt install \
    libgtk-4-1 \
    libgtkmm-4.0-1 \
    libadwaita-1-0 \
    libsqlite3-0 \
    libcurl4 \
    libgstreamer1.0-0 \
    libgstreamer-plugins-base1.0-0 \
    libgstreamer-plugins-good1.0 \
    libavformat60 \
    libavcodec60 \
    libavutil58 \
    libswscale7 \
    libglib2.0-0 \
    gstreamer1.0-libav \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good
```

## Fedora / RHEL / CentOS / AlmaLinux

```bash
sudo dnf install \
    gtk4 \
    gtkmm4.0 \
    libadwaita \
    sqlite \
    libcurl \
    gstreamer1 \
    gstreamer1-plugins-base \
    gstreamer1-plugins-good \
    ffmpeg \
    glib2
```

## Arch Linux / Manjaro

```bash
sudo pacman -S \
    gtk4 \
    gtkmm4 \
    libadwaita \
    sqlite \
    curl \
    gst-libav \
    gst-plugins-base \
    gst-plugins-good \
    ffmpeg \
    glib2
```

## openSUSE

```bash
sudo zypper install \
    gtk4 \
    gtkmm4 \
    libadwaita-1-0 \
    libsqlite3-0 \
    libcurl4 \
    gstreamer \
    gstreamer-plugins-base \
    gstreamer-plugins-good \
    ffmpeg \
    libavcodec60 \
    libavformat60 \
    libavutil58 \
    libswscale7 \
    glib2-devel
```

## Verified Distributions

| Distribution | Version | Status |
|--------------|---------|--------|
| Arch Linux | Latest | Verified |
| Ubuntu | 24.04 | Should work |
| Fedora | 40+ | Should work |
| Debian | 12+ | Should work |
| openSUSE | Leap 15.5+ | Should work |

## Optional: Vulkan GPU Acceleration

For GPU-accelerated inference (recommended):

```bash
# Arch
sudo pacman -S vulkan-headers vulkan-validation-layers

# Ubuntu
sudo apt install vulkan-tools libvulkan1 vulkan-validation-layers-dev

# Fedora
sudo dnf install vulkan-tools vulkan-loader vulkan-validation-layers
```

LucidVideo will automatically use Vulkan if available and `--gpu` is enabled in settings.

## Notes

- All third-party ML libraries (whisper.cpp, ggml, llama.cpp) are **statically linked**
- No need to install separate packages for Whisper, llama.cpp, or GGML
- GStreamer plugins are required for video playback support
- FFmpeg libraries are required for video decoding/processing
