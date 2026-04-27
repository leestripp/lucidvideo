// video_library.hpp - Video Library for Duplicate Detection
//
// Permanent storage with unique paths and status tracking

#pragma once

#include "database.hpp"

#include <string>
#include <vector>
#include <optional>

namespace lucid {

// ============================================================================
// Video Library
// ============================================================================

class VideoLibrary {
public:
    explicit VideoLibrary(Database& db);
    ~VideoLibrary() = default;
    
    // Import video with duplicate detection
    // Returns the video ID (existing if duplicate)
    // On success: returns std::string(video_id)
    // On duplicate: returns DuplicateFound struct
    ImportResult import(const std::string& path, 
                        const std::string& title,
                        const std::string& source = "local");
    
    // Check if video exists by path
    std::optional<std::string> find_by_path(const std::string& path);
    
    // Check if video exists by ID
    std::optional<VideoEntry> get_video(const std::string& id);
    
    // Get all videos
    std::vector<VideoEntry> get_all_videos(int limit = 100, int offset = 0);
    
    // Get videos by status
    std::vector<VideoEntry> get_videos_by_status(VideoStatus status);
    
    // Get pending videos (not yet processed)
    std::vector<VideoEntry> get_pending_videos();
    
    // Status management
    void update_status(const std::string& id, VideoStatus status);
    void set_error(const std::string& id, const std::string& error);
    void mark_completed(const std::string& id);
    
    // Update video metadata
    void update_duration(const std::string& id, int64_t duration_ms);
    void update_thumbnail(const std::string& id, const std::string& thumbnail_path);
    
    // Delete video
    void delete_video(const std::string& id);
    
    // Reset all videos to PENDING status (for re-analysis)
    void reset_all_to_pending();
    
    // Statistics
    size_t count_by_status(VideoStatus status);
    size_t total_count();

private:
    Database& m_db;
    
    // Generate UUID for new video
    std::string generate_id();
};

} // namespace lucid