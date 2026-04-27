# Vision Models - Qwen2-VL 2B

**ONLY Qwen2-VL 2B is used for all vision-language tasks.**

## ⚠️ Important: Vision Requires F16 Model

**Vision scanning only works with the F16 (float16) model, NOT with quantized models.**

### Why Only F16 Works for Vision

1. **Precision Requirements**: The vision projector (mmproj) contains visual feature extractors that require full floating-point precision
2. **Quantization Incompatibility**: Q4_K_M and other quantized formats lose critical precision for image understanding
3. **Hallucination Risk**: Quantized vision models tend to hallucinate content that isn't in the image
4. **mmproj Precision Match**: The mmproj file must match precision with the main model (both F16)

### Model File Requirements

| Use Case | Main Model | mmproj | Works for Vision? |
|----------|-----------|--------|-------------------|
| Chat only | Q4_K_M | - | ❌ No vision |
| Chat only | Q8_0 | - | ❌ No vision |
| Full vision | F16 | F16 | ✅ Yes |

---

## Qwen2-VL 2B

| Property | Value |
|----------|-------|
| **Parameters** | ~2B |
| **Chat Size** | ~940 MB (Q4_K_M) |
| **Vision Size** | ~4 GB (F16) + 746 MB (mmproj) |
| **License** | Apache 2.0 |
| **Context Length** | 32,768 tokens |
| **Features** | Text chat + Image understanding |

### Why Qwen2-VL 2B?
- Single model for both text generation and image understanding
- ~2B parameters - fits on most hardware
- Apache 2.0 license - commercial use allowed
- Successfully tested with image captioning
- ~18 tokens/sec on CPU

### Test Results (with F16 model)

| Image | Description |
|-------|-------------|
| llama0-banner.png | "logo with a llama and a red cross through it, along with text that reads 'LLaMA C++ Can it run DOOM LLaMA?'" |
| llama1-logo.png | "logo of the company LLaMAC++" |

### Performance (CPU-only, F16 model)

| Metric | Value |
|--------|-------|
| Image encoding | ~45-50 seconds |
| Image decoding | ~9 seconds |
| Text generation | ~32 tokens/sec |
| RAM usage | ~1.6 GB + 267 MB compute |

### Image Format Requirements

Images are passed as **file paths** (not base64). llama.cpp's mtmd API loads images directly from disk.

**Supported formats**: JPEG, PNG, BMP, GIF, PSD, HDR  
**NOT supported**: WebP (stb_image limitation)

### Usage with llama-mtmd-cli

```bash
# Set library path
export LD_LIBRARY_PATH=./build/bin:$LD_LIBRARY_PATH

# Test with F16 model and mmproj (vision works!)
./build/bin/llama-mtmd-cli \
  -m ~/.local/share/lucid/models/Qwen2-VL-2B-Instruct-F16.gguf \
  --mmproj ~/.local/share/lucid/models/mmproj-Qwen2-VL-2B-Instruct-F16.gguf \
  --image path/to/image.jpg \
  -p "Describe this image in one sentence." \
  -n 50 -t 4 --no-mmproj-offload
```

### Build llama-mtmd-cli

```bash
cmake -S . -B build -DLLAMA_BUILD_TOOLS=ON -DLLAMA_BUILD_COMMON=ON
cmake --build build -j$(nproc) --target llama-mtmd-cli
```

---

## Integration in Lucid

Lucid's VisionScanner (`src/lucid/vision/vision_scanner.cpp`) uses the same API:

```cpp
// Initialize vision scanner with model and mmproj paths
m_vision_scanner.initialize(model_path, mmproj_path);

// Analyze a thumbnail image
auto result = m_vision_scanner.analyze_image(thumbnail_path, "Describe this frame");
```

The mmproj path is automatically discovered by `ModelManager::get_vision_mmproj_path()` which searches for files with "mmproj" in the name.

---

## Notes

- **Vision requires F16 model** - Q4_K_M does NOT work for vision scanning
- Vision projector (mmproj) required for image understanding
- Both mmproj and main model files must use the same precision (F16)
- Apache 2.0 license allows commercial use
- Images loaded from file paths, not base64 encoded
