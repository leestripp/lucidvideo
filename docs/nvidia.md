# NVIDIA Vulkan Setup

Required packages for GPU acceleration via Vulkan on NVIDIA graphics cards.

## Required Packages

```bash
sudo pacman -S nvidia-utils vulkan-icd-loader
```

### Package Details

| Package | Description |
|---------|-------------|
| `nvidia-utils` | NVIDIA drivers and Vulkan support (includes `libvulkan.so`) |
| `vulkan-icd-loader` | Vulkan ICD loader - discovers Vulkan drivers at runtime |

## Optional Packages

```bash
# Development tools
sudo pacman -S vulkan-headers spirv-headers spirv-tools

# Validation layers (for debugging)
sudo pacman -S vulkan-validation-layers

# Optional tools
sudo pacman -S vulkan-tools    # Includes vulkaninfo
```

## Verification

After installation, verify the NVIDIA Vulkan driver is detected:

```bash
vulkaninfo | grep -i nvidia
# or
vulkaninfo --summary
```

Expected output should show your NVIDIA GPU (e.g., "GeForce RTX 3080").

## How It Works

1. `nvidia-utils` installs the NVIDIA Vulkan ICD to `/usr/share/vulkan/icd.d/nvidia_icd.json`
2. `vulkan-icd-loader` reads this file at runtime to discover the driver
3. llama.cpp's ggml-vulkan uses the Vulkan API to offload tensor operations to the GPU
4. All model layers are transferred to VRAM for accelerated inference

## Troubleshooting

- **No GPU detected**: Ensure you're running on the NVIDIA card (not integrated graphics)
- **Permission denied**: Add your user to the `video` group: `sudo usermod -aG video $USER`
- **Out of memory**: Close other GPU-intensive applications, or reduce model size

## See Also

- [AMD Vulkan Setup](./amd.md) - For AMD graphics cards
- [GPU Acceleration](../gpu-acceleration.md) - General GPU setup guide