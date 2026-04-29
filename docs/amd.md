# AMD Vulkan Setup

Required packages for GPU acceleration via Vulkan on AMD graphics cards.

## Required Packages

```bash
sudo pacman -S vulkan-radeon vulkan-icd-loader vulkan-mesa-implicit-layers
```

### Package Details

| Package | Description |
|---------|-------------|
| `vulkan-radeon` | Open-source Vulkan driver for AMD GPUs (RADV) |
| `vulkan-icd-loader` | Vulkan ICD loader - discovers Vulkan drivers at runtime |
| `vulkan-mesa-implicit-layers` | Mesa's implicit Vulkan layers (required for RADV) |

## Optional Packages

```bash
# Development tools
sudo pacman -S vulkan-headers spirv-headers spirv-tools

# Explicit layers (may be needed for some apps)
sudo pacman -S vulkan-mesa-layers

# Validation layers (for debugging)
sudo pacman -S vulkan-validation-layers

# Optional tools
sudo pacman -S vulkan-tools    # Includes vulkaninfo
```

## Verification

After installation, verify the AMD Vulkan driver is detected:

```bash
vulkaninfo | grep -i amd
# or
vulkaninfo --summary
```

Expected output should show your AMD GPU (e.g., "AMD Radeon RX 9070 XT (RADV GFX1201)").

## How It Works

1. `vulkan-radeon` provides the RADV open-source Vulkan driver
2. `vulkan-mesa-implicit-layers` enables the Mesa implicit layers needed for RADV
3. `vulkan-icd-loader` reads the ICD configuration at runtime
4. llama.cpp's ggml-vulkan uses the Vulkan API to offload tensor operations to the GPU
5. All model layers are transferred to VRAM for accelerated inference

## Tested GPUs

This setup has been tested and confirmed working with:

- AMD Radeon RX 9070 XT (RADV GFX1201) - **16GB VRAM**

Other AMD GPUs with Vulkan support should work similarly:

- RX 7000 series (RDNA 3)
- RX 6000 series (RDNA 2)
- RX 5000 series (RDNA 1)
- Older AMD GPUs with proper Vulkan support

## Troubleshooting

- **"radv is not a conformant Vulkan implementation"**: This is normal for AMD's open-source driver. It's safe to ignore (testing use only warning).
- **No GPU detected**: Ensure the kernel module is loaded: `lsmod | grep amdgpu`
- **Permission denied**: Add your user to the `video` group: `sudo usermod -aG video $USER`
- **Out of memory**: Close other GPU-intensive applications, or reduce model size

## See Also

- [NVIDIA Vulkan Setup](./nvidia.md) - For NVIDIA graphics cards
- [GPU Acceleration](../gpu-acceleration.md) - General GPU setup guide