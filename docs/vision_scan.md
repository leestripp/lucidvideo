# Vision Scan Feature - Implementation Complete

## Overview

Vision scanning analyzes video thumbnails using Qwen2-VL 2B to extract visual information for enhanced search. Similar to how audio is scanned for transcription/markers, vision scanning creates searchable markers with visual descriptions.

## ⚠️ Important: Model Requirements

**Vision scanning ONLY works with the F16 (float16) model, NOT with quantized models.**

| Model Type | Vision Works? | Notes |
|------------|---------------|-------|
| Q4_K_M | ❌ No | Quantized - vision disabled |
| Q8_0 | ❌ No | Quantized - vision disabled |
| F16 | ✅ Yes | Full precision required |

### Required Files
1. **Main model**: `Qwen2-VL-2B-Instruct-F16.gguf` (~4 GB)
2. **Vision projector**: `mmproj-Qwen2-VL-2B-Instruct-F16.gguf` (~746 MB)

Both files must use F16 precision and be in the same directory.

---

## Architecture

### Two-Pass Analysis

```
VideoImport → VideoAnalysisWorker
              ├─ Pass 1: Quick Analysis (UI updates immediately)
              │   ├─ Metadata (duration)
              │   ├─ Thumbnails (ThumbnailGenerator)
              │   ├─ Transcription (Whisper)
              │   └─ Mark video COMPLETED
              │
              └─ Pass 2: Vision Enhancement (background)
                  └─ Qwen2-VL scans thumbnails
                     → Creates VISION markers with descriptions
```

### Key Components

- **VisionScanner** (`src/lucid/vision/vision_scanner.cpp`): Wraps llama.cpp mtmd API
- **VideoAnalyst** (`src/lucid/model/video_analyst.cpp`): Coordinates analysis phases
- **VideoAnalysisWorker** (`src/lucid/model/video_analysis_worker.cpp`): Background processing
- **ModelManager** (`src/lucid/model/model_manager.cpp`): Model discovery and mmproj lookup

### Database Schema

```sql
-- Vision analysis results
CREATE TABLE IF NOT EXISTS vision_analyses (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    thumbnail_id    INTEGER NOT NULL REFERENCES thumbnails(id) ON DELETE CASCADE,
    video_id        TEXT NOT NULL REFERENCES videos(id) ON DELETE CASCADE,
    description     TEXT NOT NULL DEFAULT '',
    scene_type      TEXT,
    text_content    TEXT,
    confidence      REAL DEFAULT 1.0,
    model_used      TEXT,
    processing_time_ms INTEGER,
    created_at      TEXT DEFAULT (datetime('now'))
);

-- Thumbnails table extended
ALTER TABLE thumbnails ADD COLUMN vision_description TEXT;
ALTER TABLE thumbnails ADD COLUMN vision_analyzed INTEGER DEFAULT 0;

-- VISION markers created alongside AUDIO markers
-- segment_index is shared across marker types
```

---

## Image Format

Images are passed as **file paths** (PNG thumbnails), NOT base64 encoded.

**Supported formats**: JPEG, PNG, BMP, GIF, PSD, HDR  
**NOT supported**: WebP (stb_image limitation in llama.cpp)

### Internal Processing

```cpp
// vision_scanner.cpp - image loading
auto bmp = mtmd_helper_bitmap_init_from_file(ctx, image_path.c_str());
```

The llama.cpp mtmd-helper:
1. Reads image file from disk
2. Decodes using stb_image
3. Converts to RGB format
4. Creates mtmd_bitmap for processing

---

## Usage

### Settings

Vision scan is **always enabled** - no toggle needed. You can only configure the interval:

```cpp
// Configure (interval = every Nth thumbnail)
ModelManager::instance().set_vision_scan_interval(10);  // Every 10th thumbnail
ModelManager::instance().set_vision_max_per_video(25); // Max per video
```

### API Control

```bash
# Vision scan is always enabled, configure interval via API
curl -X POST http://127.0.0.1:8765/api/command \
  -H "Content-Type: application/json" \
  -d '{"command":"set_vision_scan","args":{"interval":10}}'
```

### Processing Flow

1. Video is imported and queued for analysis
2. Pass 1 runs immediately, video shows thumbnails + audio markers
3. Pass 2 runs in background when vision model is available:
   - Selects thumbnails based on interval setting
   - Runs Qwen2-VL analysis on each selected thumbnail
   - Creates VISION markers with descriptions
   - Updates thumbnail records with vision_analyzed flag

---

## Performance

| Aspect | Value |
|--------|-------|
| Batch size | 4 images per batch |
| Image encoding | ~45-50 seconds per image |
| Text generation | ~32 tokens/sec |
| RAM usage | ~1.6 GB model + 267 MB compute |
| Default interval | Every 10th thumbnail |
| Max per video | 25 thumbnails |

---

## Testing

### Unit Tests

```bash
# Run vision scanner tests
ctest --test-dir build --output-on-failure -R vision
```

### Manual Testing

```bash
# Test with llama-mtmd-cli directly
LD_LIBRARY_PATH=./build/bin ./build/bin/llama-mtmd-cli \
  -m ~/.local/share/lucid/models/Qwen2-VL-2B-Instruct-F16.gguf \
  --mmproj ~/.local/share/lucid/models/mmproj-Qwen2-VL-2B-Instruct-F16.gguf \
  --image path/to/thumbnail.png \
  -p "Describe this video frame in one sentence." \
  -n 50 -t 4 --no-mmproj-offload
```

### App Testing

```bash
# Vision scan is always enabled by default
# Configure in settings.json if needed
echo '{"vision_scan_interval":10}' > ~/.config/lucid/settings.json
LD_LIBRARY_PATH=./build/bin ./build/lucid
```

---

## Implementation Checklist

- [x] Add database schema for `vision_analyses` table
- [x] Create `VisionScanner` class
- [x] Integrate vision scan into `VideoAnalyst`
- [x] Add settings UI for vision scan toggle
- [x] Implement thumbnail sampling (interval-based)
- [x] Add vision descriptions to search
- [x] Add progress reporting for vision phase
- [x] Add mmproj to model catalog
- [x] Write tests for VisionScanner
- [x] Document usage in user guide

---

## References

- llama.cpp multimodal: `third_party/llama.cpp/docs/multimodal.md`
- Qwen2-VL GGUF: https://huggingface.co/ggml-org/Qwen2-VL-2B-Instruct-GGUF
- MTMD API: `third_party/llama.cpp/include/mtmd.h`
- Image format: `docs/image_format.md`
- Model download: `docs/models.md`
