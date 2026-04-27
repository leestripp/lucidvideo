// thumbnail_generator.hpp - Thumbnail Generator for Video Previews
//
// Generates thumbnails at configurable intervals without timecode overlays

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <random>

namespace lucid {

// ============================================================================
// Data Structures
// ============================================================================

struct GeneratedThumbnail {
    std::string path;           // Path to saved thumbnail image
    int64_t timestamp_ms;       // Timestamp in video (ms)
    std::string timecode;       // Formatted timecode "HH:MM:SS.mmm"
    int width = 0;              // Image width
    int height = 0;             // Image height
};

// ============================================================================
// Thumbnail Distribution Configuration
// ============================================================================

struct ThumbnailConfig {
    int min_count = 4;          // Minimum thumbnails for any video
    int max_count = 50;         // Maximum thumbnails (long videos)
    int target_count = 8;       // Target for medium-length videos (~5 min)
    int short_video_threshold_ms = 30000;  // 30 sec
    int long_video_threshold_ms = 1800000; // 30 min
    double jitter_amount = 0.02; // ±2% randomization to avoid same frames
};

// ============================================================================
// ThumbnailGenerator
// ============================================================================

class ThumbnailGenerator {
public:
    ThumbnailGenerator();
    ~ThumbnailGenerator() = default;
    
    // Check if FFmpeg is available
    static bool is_available();
    
    // Generate thumbnails at regular intervals
    // interval_sec: seconds between thumbnails (default: 10)
    // max_edge_px: max width or height (default: 512)
    std::vector<GeneratedThumbnail> generate(
        const std::string& video_path,
        const std::string& output_dir,
        int interval_sec = 10,
        int max_edge_px = 512);
    
    // Adaptive generation - calculates optimal thumbnail count based on duration
    // Returns thumbnails distributed evenly across the video duration
    std::vector<GeneratedThumbnail> generate_adaptive(
        const std::string& video_path,
        const std::string& output_dir,
        int max_edge_px = 512,
        const ThumbnailConfig& config = ThumbnailConfig{});
    
    // Calculate optimal thumbnail count based on video duration
    static int calculate_adaptive_count(int64_t duration_ms, const ThumbnailConfig& config = ThumbnailConfig{});
    
    // Get thumbnail timestamps at evenly distributed positions
    static std::vector<int64_t> get_adaptive_positions(int64_t duration_ms, int count, double jitter = 0.02);
    
    // Generate single thumbnail at specific timestamp
    std::optional<GeneratedThumbnail> extract_at(
        const std::string& video_path,
        const std::string& output_path,
        int64_t timestamp_ms,
        int max_edge_px = 512);
    
    // Get video duration in milliseconds
    std::optional<int64_t> get_duration_ms(const std::string& video_path);
    
    // Format milliseconds to timecode string "HH:MM:SS.mmm"
    static std::string format_timecode(int64_t ms);
    
    // Parse timecode string to milliseconds
    static std::optional<int64_t> parse_timecode(const std::string& timecode);

private:
    std::string m_cache_dir;
    
    std::string get_cache_dir();
    std::string generate_filename(const std::string& video_path, int64_t timestamp_ms);
    bool run_command(const std::string& command);
    bool run_command_with_timeout(const std::string& command, int timeout_seconds);
    std::optional<std::pair<int, int>> get_image_size(const std::string& image_path);
};

} // namespace lucid