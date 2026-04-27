# Qwen2-VL Image Format for GGUF Models in llama.cpp

## Overview

This document describes how images are passed to Qwen2-VL GGUF models when using llama.cpp's multimodal (mtmd) API.

## ⚠️ Important: Vision Requires F16 Model

**Vision scanning only works with F16 (float16) models, NOT with quantized models.**

Quantized models (Q4_K_M, Q8_0, etc.) do not work for vision tasks. The vision projector (mmproj) requires full floating-point precision for visual feature extraction.

See `docs/models.md` and `docs/vision-models-LLMs.md` for download instructions.

## Image Format Requirements

### Supported Image Formats
llama.cpp's mtmd uses **stb_image.h** for image loading. The supported formats are:
- **JPEG** (.jpg, .jpeg)
- **PNG** (.png)
- **BMP** (.bmp)
- **GIF** (.gif)
- **PSD** (.psd) - Photoshop format
- **HDR** (.hdr) - Radiance HDR format

**Note**: WebP format is NOT supported by llama.cpp (stb_image doesn't support it by default).

### Image Input Method
Images are passed as **file paths**, NOT base64 encoded strings.

The llama.cpp mtmd API uses:
```cpp
mtmd_bitmap* mtmd_helper_bitmap_init_from_file(mtmd_context* ctx, const char* fname);
```

This function:
1. Reads the image file from disk
2. Decodes it using stb_image
3. Converts to RGB format (3 channels)
4. Creates an `mtmd_bitmap` structure for processing

### No Base64 Encoding Required
Unlike REST API endpoints that accept base64 data URIs (e.g., `data:image/jpeg;base64,...`), llama.cpp's C/C++ API works directly with image files on disk.

## Internal Processing

### From File to Bitmap
The image loading process:
1. File is read into memory (binary)
2. `stbi_load_from_memory()` decodes the image
3. Image is converted to RGB (3 channels) if needed
4. `mtmd_bitmap_init()` creates the bitmap structure
5. Memory from stb is freed after bitmap creation

### Implementation in Lucid
In `vision_scanner.cpp`:
```cpp
auto bmp = mtmd_helper_bitmap_init_from_file(ctx, image_path.c_str());
```

The image path should be a valid filesystem path to a supported image file.

## Important Notes for Qwen2-VL

### Model Precision Requirements
**Vision only works reliably with F16 (float16) models, NOT with quantized models.**

Quantized models (Q4_K_M, Q8_0, etc.) do not work well with vision tasks due to:
- Loss of precision in visual feature extraction
- Incompatible quantization for vision projector (mmproj)
- Hallucination issues with quantized vision models

### Required Files
For vision scanning with Qwen2-VL, you need:
1. **Main model**: The GGUF file (e.g., `Qwen2-VL-2B-Instruct-F16.gguf`)
2. **Vision projector (mmproj)**: A separate file containing vision-specific weights (e.g., `mmproj-Qwen2-VL-2B-Instruct-Q8_0.gguf`)

Without the mmproj file, vision scanning is skipped with the message:
```
"Vision model not available (model=yes, mmproj=no)"
```

## Example Usage

### Using llama-mtmd-cli
```bash
./llama-mtmd-cli \
  -m ~/.local/share/lucid/models/Qwen2-VL-2B-Instruct-F16.gguf \
  --mmproj ~/.local/share/lucid/models/mmproj-Qwen2-VL-2B-Instruct-Q8_0.gguf \
  --image /path/to/image.png \
  -p "Describe this image in one sentence." \
  -n 50 -t 4
```

### Using Lucid's VisionScanner
Images are loaded from thumbnail paths stored in the database:
```cpp
std::string image_path = "/home/user/.local/share/lucid/thumbnails/video_001/frame_001.png";
auto result = vision_scanner->analyze_image(image_path, prompt);
```

## Thumbnail Generation
Lucid generates thumbnails as PNG files using FFmpeg. These PNG files are then passed directly to the VisionScanner for vision analysis. No additional conversion or base64 encoding is needed.

## References
- llama.cpp mtmd-helper: `third_party/llama.cpp/tools/mtmd/mtmd-helper.cpp`
- stb_image documentation: https://github.com/nothings/stb
- Qwen2-VL Hugging Face: https://huggingface.co/Qwen/Qwen2-VL-2B-Instruct