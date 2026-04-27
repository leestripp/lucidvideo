// video_item.hpp - Video item data structure
#pragma once

#include <string>

namespace lucid {

// Forward declaration
struct VideoEntry;

/**
 * VideoItem - Video metadata for grid display
 */
struct VideoItem {
    std::string id;          // Database ID
    std::string path;        // File path
    std::string title;       // Display title
    std::string thumbnail_path; // Thumbnail image path
    int duration_sec = 0;    // Duration in seconds
    bool is_transcribing = false;  // Currently being transcribed
    bool is_indexed = false;        // Has transcription/markers
    float transcribe_progress = 0.0f; // Transcription progress 0.0-1.0
    
    // Create VideoItem from VideoEntry
    static VideoItem from_entry(const VideoEntry& entry);
};

} // namespace lucid