# Lucid Video Analyst

## Overview

The Video Analyst module handles the complete video analysis pipeline in Lucid, including:
1. **Video Library** - Permanent storage with unique paths and status tracking
2. **Transcription** - Converting speech to text with timestamp alignment
3. **Thumbnail Generation** - Creating preview images (without overlays)
4. **Marker System** - Timecode-based markers for transcripts and thumbnails
5. **Metadata Storage** - Storing timecodes for synchronization

**Repository**: [github.com/ggml-org/lucid](https://github.com/ggml-org/lucid)

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              Video Analyst Pipeline                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────┐    ┌──────────────┐    ┌─────────────────┐    ┌─────────────┐ │
│  │  Video  │───▶│   Video      │───▶│ Transcription   │───▶│   SQLite   │ │
│  │   File  │    │   Library    │    │   Engine        │    │  Database  │ │
│  └──────────┘    │              │    │                 │    │            │ │
│                  │  - Status    │    │  - Segments     │    │ - Videos   │ │
│                  │  - No Dup    │    │  - Markers      │    │ - Timeline │ │
│                  │  - UUID      │    │  - Sentences    │    │ - Thumbs   │ │
│                  └──────────────┘    └─────────────────┘    │ - Markers  │ │
│                                                             └─────────────┘ │
│                                                                              │
│  Thumbnails stored separately, timecodes as DATABASE METADATA               │
│  Transcript segments become MARKERS for video navigation                    │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Video Library System

### Requirements

1. **Permanent Storage** - All videos added to Lucid are stored permanently
2. **Unique File Paths** - Prevent duplicate entries
3. **Status Tracking** - Track processing state for each video
4. **UUID Identification** - Each video has a unique identifier

### Video Status States

```cpp
enum class VideoStatus {
    PENDING,           // Added, not yet processed
    ANALYZING,         // Currently being analyzed
    TRANSCRIBING,      // Transcription in progress
    THUMBNAILING,      // Thumbnail generation in progress
    COMPLETED,         // All processing complete
    FAILED,            // Processing failed
    STALE              // Source file moved/deleted
};

struct VideoEntry {
    std::string id;                    // UUID - primary identifier
    std::string path;                  // Full file path (UNIQUE constraint)
    std::string title;                 // Display title
    std::optional<std::string> thumbnail;
    int64_t duration_ms = 0;
    std::string source = "local";       // "local", "youtube", etc.
    std::optional<std::string> channel_id;
    VideoStatus status = VideoStatus::PENDING;
    std::string error_message;         // If status == FAILED
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
    std::string metadata_json;
};
```

### No Duplicates Logic

```cpp
// Check if video already exists by path before adding
std::optional<std::string> VideoLibrary::find_by_path(const std::string& path) {
    const char* sql = "SELECT id FROM videos WHERE path = ?";
    // ... return video_id if exists
}

// Add video only if path doesn't exist
bool VideoLibrary::add_video(const std::string& path, const std::string& title) {
    if (find_by_path(path).has_value()) {
        // Video already exists - return existing ID
        return false;
    }
    // ... insert new video
}

// Alternative: import with duplicate detection
std::variant<std::string, DuplicateFound> VideoLibrary::import(
    const std::string& path,
    ImportOptions opts) {
    
    // Check exact path match
    auto existing = find_by_path(path);
    if (existing) {
        return DuplicateFound{.existing_id = *existing};
    }
    
    // Check by filename+size (optional heuristic)
    auto by_hash = find_by_hash(compute_file_hash(path));
    if (by_hash) {
        return DuplicateFound{.existing_id = *by_hash, .reason = "same content"};
    }
    
    // Proceed with import
    return add_video(path, title);
}
```

---

## Key Design Decisions

### Timecode as Metadata (Not on Images)

**Rationale**: Storing timecode as metadata provides more flexibility:
- Thumbnails remain clean, unmodified images
- Timecode can be displayed in UI however needed
- Easy to change timecode format (HH:MM:SS vs MM:SS vs frame numbers)
- Thumbnail files are smaller and more reusable
- Search by timecode without parsing image content

### Whisper Segments as Markers

**Rationale**: Whisper already provides natural sentence/paragraph boundaries:
- Whisper segments naturally align with speech sentence boundaries
- Each segment has start/end timestamps in milliseconds
- Segments average 3-7 seconds, ideal for navigation markers
- No need for additional sentence detection - already built-in
- Word-level timestamps available for finer granularity if needed

---

## Data Flow

### 1. Video Import

```
Video File (.mp4, .mkv, .avi, etc.)
         │
         ▼
┌────────────────────────┐
│   Video Library        │
│  ────────────────────  │
│  • Check duplicates    │  If exists, return existing
│  • Generate UUID       │
│  • Set status=PENDING  │
│  • Store path          │
└────────┬───────────────┘
         │
         ▼ (trigger analysis)
```

### 2. Analysis Pipeline

```
         ▼
┌────────────────────────┐
│   FFmpegPipeline       │
│  ────────────────────  │
│  • get_metadata()      │  Returns: duration, resolution, fps
│  • extract_audio()     │  Returns: 16kHz mono WAV
│  • extract_thumbnails()│  Returns: [Thumbnail{path, timestamp_ms}]
└────────┬───────────────┘
         │
         ▼
┌────────────────────────┐     ┌─────────────────────────────┐
│  Thumbnail Generation  │     │      Database Metadata      │
│  ────────────────────  │     │  ─────────────────────────  │
│  • Interval: 10 sec    │     │  • thumbnail.timestamp_ms   │
│  • Max edge: 512px     │     │  • thumbnail.timecode       │
│  • Plain JPEG images   │────▶│  • segment.start_ms         │
│  • NO timecode overlay │     │  • segment.end_ms           │
└────────┬───────────────┘     └─────────────────────────────┘
         │
         ▼
┌────────────────────────┐
│ Transcription Engine   │
│  ────────────────────  │
│  • 30-second chunks    │
│  • Whisper.cpp         │
│  • Segments = Markers  │────▶ Markers table
│  • Word timestamps     │
└────────────────────────┘
```

---

## Markers System

Transcription segments become the primary marker system for video navigation.

### Marker Structure

```cpp
struct Marker {
    int64_t id = 0;
    std::string video_id;
    
    // Timing (from Whisper segment)
    int64_t start_ms = 0;
    int64_t end_ms = 0;
    std::string timecode_start;      // "HH:MM:SS.mmm"
    std::string timecode_end;
    
    // Content (transcribed text)
    std::string text;
    float confidence = 1.0f;
    
    // Hierarchy
    int segment_index = 0;          // Order in video
    int word_count = 0;             // Words in segment
    
    // Navigation aids
    std::optional<std::string> speaker;    // If diarization added
    std::vector<int64_t> word_timestamps_ms; // Word-level timestamps
};

struct MarkerSearchResult {
    std::string video_id;
    std::string video_title;
    std::vector<Marker> markers;
    std::string matched_text;
};
```

### Marker Table Schema

```sql
CREATE TABLE IF NOT EXISTS markers (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    video_id            TEXT NOT NULL REFERENCES videos(id) ON DELETE CASCADE,
    segment_index       INTEGER NOT NULL,       -- Order in video (0, 1, 2...)
    start_ms            INTEGER NOT NULL,       -- Start timestamp (ms)
    end_ms              INTEGER NOT NULL,       -- End timestamp (ms)
    timecode_start      TEXT NOT NULL,          -- "HH:MM:SS.mmm"
    timecode_end        TEXT NOT NULL,
    text                TEXT NOT NULL,          -- Transcribed text (sentence/paragraph)
    confidence          REAL DEFAULT 1.0,
    word_count          INTEGER DEFAULT 0,
    word_timestamps     TEXT,                   -- JSON: [0, 320, 580, ...] ms per word
    speaker             TEXT,
    created_at          TEXT DEFAULT (datetime('now')),
    UNIQUE(video_id, segment_index)
);

CREATE INDEX IF NOT EXISTS idx_markers_video ON markers(video_id);
CREATE INDEX IF NOT EXISTS idx_markers_timestamp ON markers(start_ms);
CREATE INDEX IF NOT EXISTS idx_markers_text ON markers(text);  -- For FTS
```

### FTS for Marker Search

```sql
-- Full-text search on markers
CREATE VIRTUAL TABLE IF NOT EXISTS markers_fts USING fts5(
    video_id, text, tokenize='porter unicode61'
);

-- Search markers
SELECT m.*, v.title as video_title
FROM markers_fts
JOIN markers m ON markers_fts.rowid = m.id
JOIN videos v ON m.video_id = v.id
WHERE markers_fts MATCH 'search query'
ORDER BY rank;
```

---

## Whisper Segment Extraction

Whisper naturally segments speech into sentence-like units. Each segment includes:
- Start time (t0) and end time (t1) in centiseconds (divide by 10 for milliseconds)
- Text content (transcribed speech)
- Token count (number of tokens in segment)

### Important: Timestamp Unit Conversion

**Critical**: Whisper returns timestamps in **centiseconds** (1/100th of a second), NOT milliseconds!

```cpp
// Whisper returns: t0, t1 in centiseconds (time_base = 10ms)
// To convert to milliseconds: multiply by 10
int64_t t0_centiseconds = whisper_full_get_segment_t0(ctx, i);
int64_t t0_milliseconds = t0_centiseconds * 10;
```

This was confirmed in the whisper.cpp source:
> "need to multiply times returned from whisper_full_get_segment_t{0,1}() by 10 to get milliseconds"

### Complete Whisper.cpp Code Example

This is an actual working example showing how to extract transcription with timestamps:

```cpp
// Complete example based on whisper.cpp examples/main/main.cpp
extern "C" {
#include "whisper.h"
}

#include <stdio.h>
#include <vector>
#include <string>
#include <sstream>

// Helper: Convert milliseconds to timecode string "HH:MM:SS.mmm"
std::string to_timecode(int64_t ms) {
    int64_t total_seconds = ms / 1000;
    int64_t hours = total_seconds / 3600;
    int64_t minutes = (total_seconds % 3600) / 60;
    int64_t seconds = total_seconds % 60;
    int64_t millis = ms % 1000;
    
    char buf[32];
    snprintf(buf, sizeof(buf), "%02lld:%02lld:%02lld.%03lld", 
             hours, minutes, seconds, millis);
    return std::string(buf);
}

// Load 16kHz mono WAV audio into float samples
std::vector<float> load_audio(const char* path, int& sample_rate) {
    // ... WAV loading implementation ...
    // Returns samples normalized to [-1, 1] range
    // Sets sample_rate to 16000
}

// Main transcription function with timestamps
void transcribe_with_timestamps(const char* model_path, const char* audio_path) {
    // Initialize whisper context
    struct whisper_context* ctx = whisper_init_from_file(model_path);
    if (!ctx) {
        fprintf(stderr, "Failed to load model from: %s\n", model_path);
        return;
    }
    
    // Load audio samples
    int sample_rate = 16000;
    std::vector<float> samples = load_audio(audio_path, sample_rate);
    
    if (samples.empty()) {
        fprintf(stderr, "Failed to load audio from: %s\n", audio_path);
        whisper_free(ctx);
        return;
    }
    
    // Configure parameters
    struct whisper_full_params params = whisper_default_params(
        WHISPER_SAMPLING_GREEDY  // Or WHISPER_SAMPLING_BEAM_SEARCH
    );
    params.language = "en";                    // Or nullptr for auto-detect
    params.n_threads = 4;                      // Adjust based on CPU
    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    
    // Optional: Enable word timestamps
    params.output_word_timestamps = true;
    
    // Run transcription
    if (whisper_full(ctx, params, samples.data(), samples.size()) != 0) {
        fprintf(stderr, "Transcription failed\n");
        whisper_free(ctx);
        return;
    }
    
    // Extract segments with timestamps
    const int n_segments = whisper_full_n_segments(ctx);
    printf("Transcription completed. Found %d segments:\n\n", n_segments);
    
    for (int i = 0; i < n_segments; ++i) {
        // Get text content
        const char* text = whisper_full_get_segment_text(ctx, i);
        
        // Get timestamps in CENTISECONDS
        // IMPORTANT: Multiply by 10 to get milliseconds!
        int64_t t0_cs = whisper_full_get_segment_t0(ctx, i);  // Start time (centiseconds)
        int64_t t1_cs = whisper_full_get_segment_t1(ctx, i);  // End time (centiseconds)
        
        // Convert to milliseconds
        int64_t t0_ms = t0_cs * 10;
        int64_t t1_ms = t1_cs * 10;
        
        // Format timecode strings
        std::string timecode_start = to_timecode(t0_ms);
        std::string timecode_end = to_timecode(t1_ms);
        
        // Get word count and word timestamps (if enabled)
        int n_words = whisper_full_n_words(ctx, i);
        
        printf("[%s --> %s]  %s\n", 
               timecode_start.c_str(), 
               timecode_end.c_str(),
               text ? text : "");
        
        // Extract word-level timestamps if available
        if (n_words > 0 && params.output_word_timestamps) {
            printf("  Words: ");
            for (int w = 0; w < n_words; ++w) {
                // Get word start time in centiseconds, convert to ms
                int64_t word_t_cs = whisper_full_get_segment_word_t0(ctx, i, w);
                int64_t word_t_ms = word_t_cs * 10;
                
                printf("%lldms ", (long long)word_t_ms);
            }
            printf("\n");
        }
        
        // Duration of segment
        int64_t duration_ms = t1_ms - t0_ms;
        printf("  Duration: %lld ms, Words: %d\n\n", 
               (long long)duration_ms, n_words);
    }
    
    whisper_free(ctx);
}
```

### JSON Output Example

This example shows how to output transcription as JSON with timestamps:

```cpp
#include <nlohmann/json.hpp>  // Or any JSON library

using json = nlohmann::json;

json transcription_to_json(struct whisper_context* ctx, const std::string& video_id) {
    json result;
    result["video_id"] = video_id;
    
    const int n_segments = whisper_full_n_segments(ctx);
    json segments = json::array();
    
    for (int i = 0; i < n_segments; ++i) {
        json seg;
        
        // Timestamps (convert from centiseconds to milliseconds)
        int64_t t0_ms = whisper_full_get_segment_t0(ctx, i) * 10;
        int64_t t1_ms = whisper_full_get_segment_t1(ctx, i) * 10;
        
        seg["index"] = i;
        seg["start_ms"] = t0_ms;
        seg["end_ms"] = t1_ms;
        seg["timecode_start"] = to_timecode(t0_ms);
        seg["timecode_end"] = to_timecode(t1_ms);
        
        // Text content
        const char* text = whisper_full_get_segment_text(ctx, i);
        seg["text"] = text ? text : "";
        
        // Word timestamps
        int n_words = whisper_full_n_words(ctx, i);
        seg["word_count"] = n_words;
        
        if (n_words > 0) {
            json word_times = json::array();
            for (int w = 0; w < n_words; ++w) {
                int64_t word_ms = whisper_full_get_segment_word_t0(ctx, i, w) * 10;
                word_times.push_back(word_ms);
            }
            seg["word_timestamps_ms"] = word_times;
        }
        
        segments.push_back(seg);
    }
    
    result["segments"] = segments;
    result["segment_count"] = n_segments;
    
    return result;
}
```

### CSV Output Example

Simple CSV format for spreadsheet compatibility:

```cpp
void export_to_csv(struct whisper_context* ctx, FILE* csv_file) {
    fprintf(csv_file, "index,start_ms,end_ms,timecode_start,timecode_end,duration_ms,word_count,text\n");
    
    const int n_segments = whisper_full_n_segments(ctx);
    
    for (int i = 0; i < n_segments; ++i) {
        const char* text = whisper_full_get_segment_text(ctx, i);
        
        int64_t t0_ms = whisper_full_get_segment_t0(ctx, i) * 10;
        int64_t t1_ms = whisper_full_get_segment_t1(ctx, i) * 10;
        int64_t duration_ms = t1_ms - t0_ms;
        
        int n_words = whisper_full_n_words(ctx, i);
        
        fprintf(csv_file, "%d,%lld,%lld,%s,%s,%lld,%d,\"%s\"\n",
                i,
                (long long)t0_ms,
                (long long)t1_ms,
                to_timecode(t0_ms).c_str(),
                to_timecode(t1_ms).c_str(),
                (long long)duration_ms,
                n_words,
                text ? text : "");
    }
}
```

### SRT/Subtitle Format Example

```cpp
void export_to_srt(struct whisper_context* ctx, FILE* srt_file) {
    const int n_segments = whisper_full_n_segments(ctx);
    
    for (int i = 0; i < n_segments; ++i) {
        const char* text = whisper_full_get_segment_text(ctx, i);
        
        int64_t t0_ms = whisper_full_get_segment_t0(ctx, i) * 10;
        int64_t t1_ms = whisper_full_get_segment_t1(ctx, i) * 10;
        
        // SRT time format: HH:MM:SS,mmm
        auto format_srt_time = [](int64_t ms) -> std::string {
            int64_t hours = ms / 3600000;
            int64_t minutes = (ms % 3600000) / 60000;
            int64_t seconds = (ms % 60000) / 1000;
            int64_t millis = ms % 1000;
            
            char buf[32];
            snprintf(buf, sizeof(buf), "%02lld:%02lld:%02lld,%03lld",
                     hours, minutes, seconds, millis);
            return std::string(buf);
        };
        
        fprintf(srt_file, "%d\n", i + 1);
        fprintf(srt_file, "%s --> %s\n", 
                format_srt_time(t0_ms).c_str(),
                format_srt_time(t1_ms).c_str());
        fprintf(srt_file, "%s\n\n", text ? text : "");
    }
}
```

### Whisper Segment API Reference

From `whisper.h`:

```cpp
// =============================================================================
// Segment iteration
// =============================================================================

// Number of text segments (i.e. non-empty segments containing text)
WHISPER_API int whisper_full_n_segments(struct whisper_context* ctx);

// Get text of the specified segment
WHISPER_API const char* whisper_full_get_segment_text(
    struct whisper_context* ctx,
    int i_segment
);

// Get timing of the specified segment
// Returns: start and end timestamps in centiseconds (1/100th second)
// IMPORTANT: Multiply by 10 to get milliseconds!
WHISPER_API int64_t whisper_full_get_segment_t0(
    struct whisper_context* ctx,
    int i_segment
);

WHISPER_API int64_t whisper_full_get_segment_t1(
    struct whisper_context* ctx,
    int i_segment
);

// Number of words in the segment (only if output_word_timestamps=true)
WHISPER_API int whisper_full_n_words(
    struct whisper_context* ctx,
    int i_segment
);

// Get timestamp of the i-th word in the segment (centiseconds)
// IMPORTANT: Multiply by 10 to get milliseconds!
WHISPER_API int64_t whisper_full_get_segment_word_t0(
    struct whisper_context* ctx,
    int i_segment,
    int i_word
);
```

---

## Marker Structure

```cpp
struct Marker {
    int64_t id = 0;
    std::string video_id;