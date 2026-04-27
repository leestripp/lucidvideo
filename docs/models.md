# Model URLs

**Qwen2-VL 2B is used for both LLM chat and vision tasks.**

## ⚠️ Important: Vision Requires F16 Model

**Vision scanning only works with the F16 (float16) model, NOT with quantized models.**

Quantized models (Q4_K_M, Q8_0, etc.) do not work for vision tasks because:
- Vision projector (mmproj) requires full precision for visual feature extraction
- Quantization causes hallucination and incorrect vision analysis
- The mmproj file must match precision with the main model

## Qwen2-VL 2B (LLM + Vision)

Single model for text chat and image understanding.
**From:** `ggml-org/Qwen2-VL-2B-Instruct-GGUF`

### For Chat Only (LLM)
If you only need text chat (no vision scanning), use the quantized model:

| File | URL | Size |
|------|-----|------|
| Text Model (Q4_K_M) | `https://huggingface.co/ggml-org/Qwen2-VL-2B-Instruct-GGUF/resolve/main/Qwen2-VL-2B-Instruct-Q4_K_M.gguf` | ~940 MB |

### For Vision Scanning (Required)
If you need vision scanning, you MUST download:

| File | URL | Size |
|------|-----|------|
| **Text Model (F16)** | `https://huggingface.co/ggml-org/Qwen2-VL-2B-Instruct-GGUF/resolve/main/Qwen2-VL-2B-Instruct-F16.gguf` | ~4 GB |
| **Vision Projector (F16)** | `https://huggingface.co/ggml-org/Qwen2-VL-2B-Instruct-GGUF/resolve/main/mmproj-Qwen2-VL-2B-Instruct-F16.gguf` | ~746 MB |

### Model Details

| Property | Value |
|----------|-------|
| Parameters | ~2B |
| License | Apache 2.0 |
| Context Length | 32,768 tokens |
| Vocab Size | 151,936 |

### Download Command

```bash
# Create models directory
mkdir -p ~/.local/share/lucid/models

# For VISION SCANNING (required for vision functionality):
# Download F16 text model (~4 GB)
curl -L -o ~/.local/share/lucid/models/Qwen2-VL-2B-Instruct-F16.gguf \
  "https://huggingface.co/ggml-org/Qwen2-VL-2B-Instruct-GGUF/resolve/main/Qwen2-VL-2B-Instruct-F16.gguf"

# Download F16 vision projector (~746 MB)
curl -L -o ~/.local/share/lucid/models/mmproj-Qwen2-VL-2B-Instruct-F16.gguf \
  "https://huggingface.co/ggml-org/Qwen2-VL-2B-Instruct-GGUF/resolve/main/mmproj-Qwen2-VL-2B-Instruct-F16.gguf"

# For CHAT ONLY (no vision):
# Download quantized model (~940 MB)
curl -L -o ~/.local/share/lucid/models/Qwen2-VL-2B-Instruct-Q4_K_M.gguf \
  "https://huggingface.co/ggml-org/Qwen2-VL-2B-Instruct-GGUF/resolve/main/Qwen2-VL-2B-Instruct-Q4_K_M.gguf"
```

### File Naming Convention

Lucid looks for mmproj files by matching patterns in the models directory:

```
# Expected files for vision scanning:
~/.local/share/lucid/models/Qwen2-VL-2B-Instruct-F16.gguf           # Main model
~/.local/share/lucid/models/mmproj-Qwen2-VL-2B-Instruct-F16.gguf   # Vision projector
```

The mmproj file must be in the same directory as the main model. Lucid searches for files with "mmproj" in the name.

---

## Whisper (Speech Recognition)

Whisper models are used for speech-to-text transcription.
From: `ggerganov/whisper.cpp`

| Model | URL | Size |
|-------|-----|------|
| Tiny | `https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.bin` | ~75 MB |
| Base | `https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin` | ~148 MB |
| Small | `https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin` | ~488 MB |
| Medium | `https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.bin` | ~1.5 GB |

### Download Command

```bash
# Whisper Tiny (recommended for testing)
curl -L -o ~/.local/share/lucid/models/ggml-tiny.bin \
  "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.bin"

# Whisper Base (balanced speed/accuracy)
curl -L -o ~/.local/share/lucid/models/ggml-base.bin \
  "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin"
```

---

## Embedding (Text Embeddings)

From: `second-state/All-MiniLM-L6-v2-Embedding-GGUF` and `nomic-ai/nomic-embed-text-v1.5-GGUF`

| Model | URL | Size |
|-------|-----|------|
| All-MiniLM-L6-v2 (Q4_K_M) | `https://huggingface.co/second-state/All-MiniLM-L6-v2-Embedding-GGUF/resolve/main/all-MiniLM-L6-v2-Q4_K_M.gguf` | ~90 MB |
| Nomic Embed Text v1.5 (Q4_K_M) | `https://huggingface.co/nomic-ai/nomic-embed-text-v1.5-GGUF/resolve/main/nomic-embed-text-v1.5.Q4_K_M.gguf` | ~274 MB |

### Download Command

```bash
# Nomic Embed Text v1.5 (recommended for semantic search)
curl -L -o ~/.local/share/lucid/models/nomic-embed-text-v1.5.Q4_K_M.gguf \
  "https://huggingface.co/nomic-ai/nomic-embed-text-v1.5-GGUF/resolve/main/nomic-embed-text-v1.5.Q4_K_M.gguf"
```

---

## Storage Requirements

| Use Case | Required Models | Total Size |
|----------|-----------------|------------|
| Basic (chat only) | Q4_K_M model only | ~940 MB |
| Full (chat + vision) | F16 model + F16 mmproj | ~4.7 GB |
| With transcription | Add Whisper Tiny | +75 MB |
| With semantic search | Add Nomic Embed | +274 MB |

---

## Notes

- **Vision requires F16 model** - Q4_K_M does NOT work for vision scanning
- Vision projector (mmproj) must be present for vision functionality
- Both mmproj and main model files must use the same precision (F16)
- Apache 2.0 license allows commercial use
