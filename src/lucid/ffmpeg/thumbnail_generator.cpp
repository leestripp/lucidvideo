// thumbnail_generator.cpp - Thumbnail Generator Implementation
//
// Generates thumbnails at configurable intervals without timecode overlays

#include "thumbnail_generator.hpp"
#include "../logging.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cinttypes>

namespace lucid {

namespace fs = std::filesystem;

// ============================================================================
// Adaptive Thumbnail Generation
// ============================================================================
// 
// Uses ThumbnailConfig (min=4, max=50, target=8) for adaptive thumbnail count
// See ThumbnailConfig struct in thumbnail_generator.hpp for details
// ============================================================================

ThumbnailGenerator::ThumbnailGenerator()
    : m_cache_dir(get_cache_dir()) {
}

bool ThumbnailGenerator::is_available() {
    // Check if ffmpeg is available
    int result = std::system("ffmpeg -version > /dev/null 2>&1");
    return result == 0;
}

std::string ThumbnailGenerator::get_cache_dir() {
    const char* xdg_cache = std::getenv("XDG_CACHE_HOME");
    if (xdg_cache) {
        return std::string(xdg_cache) + "/lucid/thumbnails";
    }
    
    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/.cache/lucid/thumbnails";
    }
    
    return "/tmp/lucid_thumbnails";
}

std::string ThumbnailGenerator::generate_filename(const std::string& video_path, int64_t timestamp_ms) {
    // Create a hash-like name from video path and timestamp
    std::hash<std::string> hasher;
    size_t hash = hasher(video_path);
    
    std::ostringstream ss;
    ss << std::hex << hash << "_" << timestamp_ms << ".jpg";
    return ss.str();
}

bool ThumbnailGenerator::run_command(const std::string& command) {
    // Use timeout command to prevent hanging on problematic videos
    // Default timeout: 30 seconds
    std::ostringstream timed_cmd;
    timed_cmd << "timeout 30 " << command;
    int result = std::system(timed_cmd.str().c_str());
    
    // timeout returns 124 if command timed out
    // We treat timeout as a failure but don't crash
    if (result == 124) {
        return false;
    }
    return result == 0;
}

bool ThumbnailGenerator::run_command_with_timeout(const std::string& command, int timeout_seconds) {
    std::ostringstream timed_cmd;
    timed_cmd << "timeout " << timeout_seconds << " " << command;
    int result = std::system(timed_cmd.str().c_str());
    return result == 0;
}

std::optional<std::pair<int, int>> ThumbnailGenerator::get_image_size(const std::string& image_path) {
    // Use ffprobe to get image dimensions
    std::ostringstream cmd;
    cmd << "ffprobe -v error -select_streams v:0 -show_entries stream=width,height "
        << "-of csv=p=0 \"" << image_path << "\" 2>/dev/null";
    
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        return std::nullopt;
    }
    
    char buffer[128];
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        pclose(pipe);

        
        int width = 0, height = 0;
        if (sscanf(buffer, "%d,%d", &width, &height) == 2) {
            return std::make_pair(width, height);
        }
    }
    
    pclose(pipe);
    return std::nullopt;
}

std::string ThumbnailGenerator::format_timecode(int64_t ms) {
    int64_t total_seconds = ms / 1000;
    int64_t hours = total_seconds / 3600;
    int64_t minutes = (total_seconds % 3600) / 60;
    int64_t seconds = total_seconds % 60;
    int64_t millis = ms % 1000;
    
    char buf[32];
    snprintf(buf, sizeof(buf), "%02" PRId64 ":%02" PRId64 ":%02" PRId64 ".%03" PRId64, 
             hours, minutes, seconds, millis);
    return std::string(buf);
}

std::optional<int64_t> ThumbnailGenerator::parse_timecode(const std::string& timecode) {
    int64_t hours = 0, minutes = 0, seconds = 0, millis = 0;
    
    // Parse format: HH:MM:SS.mmm or HH:MM:SS
    int n = sscanf(timecode.c_str(), "%" SCNd64 ":%" SCNd64 ":%" SCNd64 ".%" SCNd64, 
                   &hours, &minutes, &seconds, &millis);
    
    if (n < 3) {
        // Try without milliseconds
        n = sscanf(timecode.c_str(), "%" SCNd64 ":%" SCNd64 ":%" SCNd64, 
                   &hours, &minutes, &seconds);
        if (n < 3) {
            return std::nullopt;
        }
    }
    
    return (hours * 3600 + minutes * 60 + seconds) * 1000 + millis;
}

std::vector<GeneratedThumbnail> ThumbnailGenerator::generate(
    const std::string& video_path,
    const std::string& output_dir,
    int interval_sec,
    int max_edge_px) {
    
    // Use adaptive generation with config based on legacy interval_sec parameter
    ThumbnailConfig config;
    
    // Convert interval_sec to approximate duration-based config
    // For legacy compatibility, treat interval as hint for thumbnail spacing
    config.min_count = 4;
    config.max_count = 50;
    config.target_count = 8;
    
    return generate_adaptive(video_path, output_dir, max_edge_px, config);
}

std::optional<GeneratedThumbnail> ThumbnailGenerator::extract_at(
    const std::string& video_path,
    const std::string& output_path,
    int64_t timestamp_ms,
    int max_edge_px) {
    
    GeneratedThumbnail result;
    result.timestamp_ms = timestamp_ms;
    result.timecode = format_timecode(timestamp_ms);
    result.path = output_path;
    
    // Format timestamp as HH:MM:SS.ms for FFmpeg
    int64_t total_seconds = timestamp_ms / 1000;
    int64_t hours = total_seconds / 3600;
    int64_t minutes = (total_seconds % 3600) / 60;
    int64_t seconds = total_seconds % 60;
    int64_t ms = timestamp_ms % 1000;
    
    char time_str[32];
    snprintf(time_str, sizeof(time_str), "%02" PRId64 ":%02" PRId64 ":%02" PRId64 ".%03" PRId64,
             hours, minutes, seconds, ms);
    
    // Build FFmpeg command
    // -ss before -i for fast seeking
    // -vf scale with force_original_aspect_ratio to resize
    // -q:v 2 for good quality JPEG (2-31, lower is better)
    std::ostringstream cmd;
    cmd << "ffmpeg -y -ss " << time_str << " -i \"" << video_path << "\" "
        << "-vframes 1 "
        << "-vf \"scale=" << max_edge_px << ":-1:force_original_aspect_ratio=decrease\" "
        << "-q:v 2 "
        << "\"" << output_path << "\" 2>/dev/null";
    
    if (!run_command(cmd.str())) {
        return std::nullopt;
    }
    
    // Get image dimensions
    auto size = get_image_size(output_path);
    if (size.has_value()) {
        result.width = size->first;
        result.height = size->second;
    }
    
    return result;
}

std::optional<int64_t> ThumbnailGenerator::get_duration_ms(const std::string& video_path) {
    // Use ffprobe to get duration
    std::ostringstream cmd;
    cmd << "timeout 10 ffprobe -v error -show_entries format=duration "
        << "-of default=noprint_wrappers=1:nokey=1 \"" << video_path << "\" 2>/dev/null";
    
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        return std::nullopt;
    }
    
    char buffer[128];
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        pclose(pipe);

        
        double duration_sec = 0;
        if (sscanf(buffer, "%lf", &duration_sec) == 1) {
            return static_cast<int64_t>(duration_sec * 1000);
        }
    }
    
    pclose(pipe);
    return std::nullopt;
}

// ============================================================================
// Adaptive Thumbnail Generation
// ============================================================================

int ThumbnailGenerator::calculate_adaptive_count(int64_t duration_ms, const ThumbnailConfig& config) {
    if (duration_ms <= 0) {
        return config.min_count;
    }
    
    // Convert to seconds for easier calculation
    double duration_sec = duration_ms / 1000.0;
    double duration_min = duration_sec / 60.0;
    
    int count;
    
    if (duration_min < 0.5) {
        // Very short video (< 30 sec): minimum thumbnails
        count = config.min_count;
    } else if (duration_min < 5.0) {
        // Short video (30 sec - 5 min): linear growth
        count = static_cast<int>(config.min_count + (duration_min - 0.5) * 1.5);
    } else if (duration_min < 30.0) {
        // Medium video (5-30 min): steady growth with logarithmic factor
        double log_factor = std::log(duration_min / 5.0 + 1.0);
        count = static_cast<int>(config.target_count + log_factor * 4.0);
    } else {
        // Long video (30+ min): logarithmic growth toward max
        double log_factor = std::log(duration_min / 30.0 + 1.0);
        count = static_cast<int>(config.target_count * 1.5 + log_factor * 8.0);
    }
    
    // Clamp to min/max bounds
    return std::clamp(count, config.min_count, config.max_count);
}

std::vector<int64_t> ThumbnailGenerator::get_adaptive_positions(
    int64_t duration_ms, 
    int count, 
    double jitter) {
    
    std::vector<int64_t> positions;
    
    if (duration_ms <= 0 || count <= 0) {
        return positions;
    }
    
    // Initialize random engine with a seed for reproducibility
    // Use a fixed seed so same video gets same positions
    static std::mt19937 engine(42);  // Fixed seed for consistency
    
    for (int i = 0; i < count; ++i) {
        // Calculate base position as fraction of duration
        double fraction = (count == 1) ? 0.5 : (double)i / (count - 1);
        
        // Add jitter if specified (skip jitter for first and last)
        if (jitter > 0.0 && i > 0 && i < count - 1) {
            std::uniform_real_distribution<double> dist(-jitter, jitter);
            fraction += dist(engine);
            fraction = std::clamp(fraction, 0.0, 1.0);
        }
        
        // Convert to timestamp
        int64_t ts = static_cast<int64_t>(duration_ms * fraction);
        positions.push_back(ts);
    }
    
    // Ensure first thumbnail is at 0 (or very close)
    if (!positions.empty() && positions.front() > 100) {
        positions.front() = 0;
    }
    
    // Ensure last thumbnail isn't beyond duration (use -1 to ensure valid frame)
    if (!positions.empty() && positions.back() > duration_ms - 100) {
        positions.back() = duration_ms - 1;  // -1ms to ensure FFmpeg can extract
    }
    
    return positions;
}

std::vector<GeneratedThumbnail> ThumbnailGenerator::generate_adaptive(
    const std::string& video_path,
    const std::string& output_dir,
    int max_edge_px,
    const ThumbnailConfig& config) {
    
    std::vector<GeneratedThumbnail> results;
    
    THUMB_DEBUG("=== Adaptive Thumbnail Generation Started ===");
    THUMB_DEBUG("Video: %s", video_path.c_str());
    THUMB_DEBUG("Config: min=%d, max=%d, target=%d", 
                config.min_count, config.max_count, config.target_count);
    
    // Get video duration
    auto duration_opt = get_duration_ms(video_path);
    if (!duration_opt.has_value()) {
        THUMB_DEBUG("Failed to get video duration");
        return results;
    }
    
    int64_t duration_ms = *duration_opt;
    THUMB_DEBUG("Video duration: %ld ms (%.1f seconds, %.1f minutes)", 
                duration_ms, duration_ms / 1000.0, duration_ms / 60000.0);
    
    // Create output directory if needed
    fs::create_directories(output_dir);
    
    // Calculate optimal thumbnail count
    int thumb_count = calculate_adaptive_count(duration_ms, config);
    THUMB_DEBUG("Calculated thumbnail count: %d", thumb_count);
    
    // Get positions
    auto timestamps = get_adaptive_positions(duration_ms, thumb_count, config.jitter_amount);
    THUMB_DEBUG("Generated %zu timestamp positions", timestamps.size());
    // Log first few positions for debugging
    for (size_t i = 0; i < std::min(timestamps.size(), size_t(5)); ++i) {
        THUMB_DEBUG("  Position %zu: %ld ms (%.1f%%)", i, timestamps[i], (timestamps[i] * 100.0) / duration_ms);
    }
    
    // Generate thumbnails at each position
    [[maybe_unused]] int thumbnail_num = 0;
    for (int64_t ts : timestamps) {
        THUMB_DEBUG("Generating thumbnail %d at %ld ms (%.1f%%)...",
                   ++thumbnail_num, ts, (ts * 100.0) / duration_ms);
        
        std::string filename = generate_filename(video_path, ts);
        std::string output_path = (fs::path(output_dir) / filename).string();
        
        auto thumb = extract_at(video_path, output_path, ts, max_edge_px);
        if (thumb.has_value()) {
            THUMB_DEBUG("  -> Success: %s (%dx%d)", 
                       thumb->path.c_str(), thumb->width, thumb->height);
            results.push_back(*thumb);
        } else {
            THUMB_DEBUG("  -> Failed to extract thumbnail");
        }
    }
    
    THUMB_DEBUG("=== Adaptive Thumbnail Generation Complete: %zu/%d thumbnails ===", 
               results.size(), thumb_count);
    
    return results;
}

} // namespace lucid