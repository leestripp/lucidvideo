# LucidVideo Performance Enhancements - Task List

## System Information
| Component | Value |
|-----------|-------|
| Kernel | Linux 6.18.22-1.stable |
| OS | Arch Linux (x86_64) |
| Container | docker/podman (archlinux:latest) |
| Display | Wayland, GNOME |
| Architecture | x86_64 |

## Project Info
- **Version**: 0.4.1
- **Binary**: build/lucidvideo (71MB)
- **Build System**: CMake with C++20

## Overview
Performance enhancements to the video analysis pipeline, focusing on:
- Two-pass analysis optimization
- Vision scanning efficiency
- Database query optimization
- Batch processing improvements

---

## Tasks

### 1. Database Query Optimization (DONE ✓)
**Issue**: `update_marker_description()` in video_analyst.cpp used inefficient workaround:
```cpp
// Before (inefficient):
auto markers = m_db.get_markers("");  // Gets ALL markers from ALL videos
for (auto& marker : markers) {
    if (marker.id == marker_id) { ... }
}
```

**Action**: Added optimized methods to Database class
- [x] Add `get_marker_by_id(int64_t marker_id)` method declaration to database.hpp
- [x] Implement method in database.cpp with direct SQL query by ID
- [x] Update `update_marker_description()` in video_analyst.cpp to use new method
- [x] Build and verify ✓ (all 45 database tests passed)

**Impact**: O(n) → O(1) for marker description updates during vision scanning

### 2. Batch Processing for Vision Scanning (Optimized)
**Issue**: Vision scanning had redundant marker lookups

**Action**: Added marker count optimization
- [x] Add `get_marker_count_for_video(const std::string& video_id)` method to database.hpp
- [x] Implement in database.cpp with COUNT query
- [x] Update `transcribe_video()` to use optimized count instead of loading all markers
- [x] Update `scan_thumbnails_with_vision()` to use optimized count instead of loading all markers
- [x] Build and verify ✓

**Impact**: Reduces memory usage and query time for segment_index calculation

### 3. Thumbnail Generation Cache Optimization
**Issue**: Double thumbnail generation (display + vision)
**Action**: Avoid regenerating vision thumbnails if they already exist
- [ ] Review generate_thumbnails() in video_analyst.cpp
- [ ] Add cache check before generating high-res vision thumbnails
- [ ] Log cache hits vs regenerations

### 4. Database Index Optimization
**Issue**: Markers table queries could benefit from better indexing
**Action**: Review and add indexes for common query patterns
- [ ] Check existing indexes in initialize_schema()
- [ ] Add index on markers.id if not present
- [ ] Add index on markers.video_id + segment_index

### 5. Two-Pass Analysis Streamlining
**Issue**: Pass 1 and Pass 2 have redundant operations
**Action**: Refine pass boundaries to avoid duplicate work
- [ ] Review analyze_pass1() implementation
- [ ] Review fill_vision_descriptions() implementation
- [ ] Ensure VISION markers created once, not recreated on re-analysis

---

## Implementation Notes

### Task 1 Details: get_marker_by_id()

**database.hpp additions** (in Database class):
```cpp
// Get a single marker by ID (optimization)
std::optional<Marker> get_marker_by_id(int64_t marker_id);
```

**database.cpp implementation**:
```cpp
std::optional<Marker> Database::get_marker_by_id(int64_t marker_id) {
    const char* sql = R"(
        SELECT id, video_id, segment_index, start_ms, end_ms, 
               timecode_start, timecode_end, text, confidence, 
               word_count, word_timestamps, speaker, marker_type, thumbnail_path, frame_number, created_at
        FROM markers WHERE id = ?
    )";
    // ... execute with marker_id, return marker or nullopt
}
```

**video_analyst.cpp fix**:
Replace the inefficient loop in `update_marker_description()` with a direct call:
```cpp
void VideoAnalyst::update_marker_description(int64_t marker_id, const std::string& description, float confidence) {
    auto marker_opt = m_db.get_marker_by_id(marker_id);
    if (!marker_opt) {
        PIPELINE_DEBUG("Marker %ld not found for description update", marker_id);
        return;
    }
    
    auto marker = *marker_opt;
    marker.text = description;
    marker.confidence = confidence;
    m_db.update_marker(marker);
    
    // Generate embedding for the vision description
    generate_embedding(marker.video_id, marker.id, marker.start_ms, description);
    
    PIPELINE_DEBUG("Updated marker %ld with description: %s (confidence: %.2f)", 
                  marker_id, description.c_str(), confidence);
}
```

---

## Testing
- Run `./build/tests/lucid_*` test suites
- Test video analysis pipeline manually
- Verify marker updates work correctly

---

## Status
- **Last Updated**: 2026-04-30
- **Started**: Phase 6 - Performance Enhancements
