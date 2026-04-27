// video_library.cpp - Video Library Implementation
//
// Permanent storage with unique paths and status tracking

#include "video_library.hpp"

#include <sstream>
#include <iomanip>
#include <random>
#include <chrono>
#include <cstring>
#include <stdexcept>

namespace lucid {

// ============================================================================
// Video Library Implementation
// ============================================================================

VideoLibrary::VideoLibrary(Database& db)
    : m_db(db) {
}

std::string VideoLibrary::generate_id() {
    // Generate UUID v4
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::uniform_int_distribution<> dis2(8, 11);
    
    std::ostringstream ss;
    ss << std::hex;
    
    // First 8 hex digits
    for (int i = 0; i < 8; i++) {
        ss << std::setfill('0') << std::setw(1) << dis(gen);
    }
    ss << "-";
    
    // Next 4 hex digits
    for (int i = 0; i < 4; i++) {
        ss << std::setfill('0') << std::setw(1) << dis(gen);
    }
    ss << "-4";  // Version 4
    
    // Next 3 hex digits
    for (int i = 0; i < 3; i++) {
        ss << std::setfill('0') << std::setw(1) << dis(gen);
    }
    ss << "-";
    
    // Variant (8, 9, a, or b)
    ss << std::setfill('0') << std::setw(1) << dis2(gen);
    for (int i = 0; i < 3; i++) {
        ss << std::setfill('0') << std::setw(1) << dis(gen);
    }
    ss << "-";
    
    // Last 12 hex digits
    for (int i = 0; i < 12; i++) {
        ss << std::setfill('0') << std::setw(1) << dis(gen);
    }
    
    return ss.str();
}

ImportResult VideoLibrary::import(const std::string& path, 
                                  const std::string& title,
                                  const std::string& source) {
    // Check for existing video by path
    auto existing_id = m_db.find_by_path(path);
    if (existing_id.has_value()) {
        // Return duplicate found
        return DuplicateFound{*existing_id, "path"};
    }
    
    // Create new video entry
    try {
        VideoEntry video;
        video.id = generate_id();
        video.path = path;
        video.title = title;
        video.source = source;
        video.status = VideoStatus::PENDING;
        
        m_db.add_video(video);
        
        // Return video ID on success
        return video.id;
        
    } catch (const std::exception& e) {
        // Return error as string (failure case)
        return std::string("Import failed: ") + e.what();
    }
}

std::optional<std::string> VideoLibrary::find_by_path(const std::string& path) {
    return m_db.find_by_path(path);
}

std::optional<VideoEntry> VideoLibrary::get_video(const std::string& id) {
    return m_db.get_video(id);
}

std::vector<VideoEntry> VideoLibrary::get_all_videos(int limit, int offset) {
    return m_db.get_all_videos(limit, offset);
}

std::vector<VideoEntry> VideoLibrary::get_videos_by_status(VideoStatus status) {
    auto all_videos = m_db.get_all_videos(1000, 0);
    std::vector<VideoEntry> filtered;
    
    for (const auto& video : all_videos) {
        if (video.status == status) {
            filtered.push_back(video);
        }
    }
    
    return filtered;
}

std::vector<VideoEntry> VideoLibrary::get_pending_videos() {
    return get_videos_by_status(VideoStatus::PENDING);
}

void VideoLibrary::update_status(const std::string& id, VideoStatus status) {
    m_db.update_video_status(id, status);
}

void VideoLibrary::set_error(const std::string& id, const std::string& error) {
    m_db.set_video_error(id, error);
}

void VideoLibrary::mark_completed(const std::string& id) {
    m_db.update_video_status(id, VideoStatus::COMPLETED);
}

void VideoLibrary::update_duration(const std::string& id, int64_t duration_ms) {
    m_db.update_video_duration(id, duration_ms);
}

void VideoLibrary::update_thumbnail(const std::string& id, const std::string& thumbnail_path) {
    auto video = m_db.get_video(id);
    if (video) {
        video->thumbnail = thumbnail_path;
        m_db.update_video(*video);
    }
}

void VideoLibrary::delete_video(const std::string& id) {
    m_db.delete_video(id);
}

void VideoLibrary::reset_all_to_pending() {
    m_db.reset_all_videos_to_pending();
}

size_t VideoLibrary::count_by_status(VideoStatus status) {
    return get_videos_by_status(status).size();
}

size_t VideoLibrary::total_count() {
    return m_db.get_all_videos(10000, 0).size();
}

} // namespace lucid