// database.hpp - Lucid Database Module
//
// SQLite3 with sqlite-vec for vector search

#pragma once

#include <sqlite3.h>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <functional>
#include <variant>

namespace lucid {

// ============================================================================
// Enums
// ============================================================================

enum class VideoStatus {
    PENDING,
    ANALYZING,
    TRANSCRIBING,
    THUMBNAILING,
    VISION_SCANNING,
    COMPLETED,
    FAILED,
    STALE,
    BROKEN  // Failed more than 3 times - manual intervention required
};

const char* video_status_to_string(VideoStatus status);
VideoStatus video_status_from_string(const std::string& str);

// Marker type enum for unified marker system
enum class MarkerType {
    AUDIO,   // From whisper transcription - has speaker icon
    VISION   // From vision scan - has actual thumbnail
};

inline const char* marker_type_to_string(MarkerType type) {
    switch (type) {
        case MarkerType::AUDIO: return "audio";
        case MarkerType::VISION: return "vision";
        default: return "unknown";
    }
}

inline MarkerType marker_type_from_string(const std::string& str) {
    if (str == "vision") return MarkerType::VISION;
    return MarkerType::AUDIO;  // Default to AUDIO for backwards compatibility
}

// ============================================================================
// Data Structures
// ============================================================================

struct VideoEntry {
    std::string id;
    std::string path;
    std::string title;
    std::optional<std::string> thumbnail;
    int64_t duration_ms = 0;                    // Duration in milliseconds
    std::string source = "local";
    std::optional<std::string> channel_id;
    VideoStatus status = VideoStatus::PENDING;  // Processing status
    std::string error_message;                  // Set if status == FAILED
    int retry_count = 0;                       // Number of analysis retry attempts
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
    std::string metadata_json;
};

struct Marker {
    int64_t id = 0;
    std::string video_id;
    int segment_index = 0;                      // Order in video (0, 1, 2...)
    int64_t start_ms = 0;                      // Start timestamp (ms)
    int64_t end_ms = 0;                        // End timestamp (ms)
    int64_t frame_number = -1;                  // Frame number in video (-1 if unknown)
    std::string timecode_start;                // "HH:MM:SS.mmm"
    std::string timecode_end;                  // "HH:MM:SS.mmm"
    std::string text;                          // Transcribed text or vision description
    float confidence = 1.0f;
    int word_count = 0;
    std::string word_timestamps_json;           // JSON: [0, 320, 580, ...] ms
    std::optional<std::string> speaker;
    
    // Unified marker system fields
    MarkerType marker_type = MarkerType::AUDIO; // AUDIO or VISION
    std::string thumbnail_path;                 // Path to thumbnail icon
    // For AUDIO markers: uses speaker icon (generated path)
    // For VISION markers: uses actual thumbnail path
    
    std::chrono::system_clock::time_point created_at;
};

struct ThumbnailEntry {
    int64_t id = 0;
    std::string video_id;
    std::string path;                           // Path to thumbnail image
    int64_t timestamp_ms = 0;                   // Timestamp in video
    std::string timecode;                       // "HH:MM:SS.mmm"
    int width = 0;
    int height = 0;
    std::string vision_description;           // Vision scan description
    bool vision_analyzed = false;             // Has this thumbnail been vision-scanned?
    std::chrono::system_clock::time_point created_at;
};

// Vision scan result for a thumbnail
struct VisionAnalysis {
    int64_t id = 0;
    int64_t thumbnail_id = 0;
    std::string video_id;
    std::string description;                    // Natural language description
    std::vector<std::string> objects;           // Detected objects (JSON array)
    std::string scene_type;                     // indoor/outdoor/screen_capture/motion_graphics
    std::string text_content;                   // Any visible text in frame
    float confidence = 1.0f;
    std::string model_used;                     // e.g., "qwen2-vl-2b"
    int64_t processing_time_ms = 0;
    std::chrono::system_clock::time_point created_at;
};

struct TranscriptionSegment {
    int64_t id = 0;
    std::string video_id;
    int start_ms = 0;                         // Start timestamp (ms)
    int end_ms = 0;                           // End timestamp (ms)
    std::string timecode_start;                // "HH:MM:SS.mmm"
    std::string timecode_end;                  // "HH:MM:SS.mmm"
    std::string text;
    float confidence = 0.0f;
    int word_count = 0;
    std::string word_timestamps_json;          // JSON: [0, 320, 580, ...] ms per word
    std::optional<int64_t> chunk_id;
    bool is_complete = false;
};

struct TimelineEntry {
    int64_t timestamp_ms;
    std::string entry_type;                     // "thumbnail" or "marker"
    std::optional<int64_t> thumbnail_id;
    std::optional<int64_t> marker_id;
};

// Match type for search results
enum class MatchType {
    TITLE,       // Match is in video title (filename)
    TRANSCRIPT,  // Match is in transcript/marker text
    UNKNOWN
};

struct SearchResult {
    std::string video_id;
    std::string title;
    int timestamp_ms = 0;
    double relevance_score = 0.0;
    std::string snippet;
    MatchType match_type = MatchType::UNKNOWN;
};

struct SemanticQuery {
    std::string query_text;
    std::vector<float> query_embedding;
    int max_results = 20;
    float similarity_threshold = 0.5f;
};

// Duplicate detection result
struct DuplicateFound {
    std::string existing_id;
    std::string reason;
};

using ImportResult = std::variant<std::string, DuplicateFound>;

// ============================================================================
// Database Class
// ============================================================================

class Database {
public:
    Database(const std::string& db_path);
    ~Database();
    
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    
    // Video CRUD
    std::optional<VideoEntry> get_video(const std::string& id);
    std::vector<VideoEntry> get_all_videos(int limit = 100, int offset = 0);
    std::string add_video(const VideoEntry& video);
    void update_video(const VideoEntry& video);
    void delete_video(const std::string& id);
    
    // Chat query helpers (for LLM context)
    int get_video_count();                                        // Total video count
    std::vector<VideoEntry> get_recent_videos(int limit = 10);  // Most recent videos
    std::vector<VideoEntry> get_videos_by_duration(int64_t min_ms, int64_t max_ms = -1);  // By duration
    // Video status management
    std::optional<std::string> find_by_path(const std::string& path);
    void update_video_status(const std::string& id, VideoStatus status);
    void set_video_error(const std::string& id, const std::string& error);
    void update_video_duration(const std::string& id, int64_t duration_ms);
    void reset_all_videos_to_pending();  // For re-analysis
    
    // Markers (from transcription)
    int64_t add_marker(const Marker& marker);
    void add_markers(const std::string& video_id, const std::vector<Marker>& markers);
    std::vector<Marker> get_markers(const std::string& video_id);
    std::vector<Marker> search_markers(const std::string& query, int limit = 20);
    void update_marker(const Marker& marker);  // Update existing marker (e.g., fill in vision description)
    void delete_marker(int64_t marker_id);  // Delete a single marker by ID
    void clear_markers(const std::string& video_id);
    
    // Thumbnails
    int64_t add_thumbnail(const ThumbnailEntry& thumbnail);
    void add_thumbnails(const std::string& video_id, const std::vector<ThumbnailEntry>& thumbnails);
    std::vector<ThumbnailEntry> get_thumbnails(const std::string& video_id);
    void clear_thumbnails(const std::string& video_id);
    
    // Vision Analysis
    int64_t add_vision_analysis(const VisionAnalysis& analysis);
    std::vector<VisionAnalysis> get_vision_analyses(const std::string& video_id);
    std::vector<VisionAnalysis> get_vision_analyses_for_thumbnail(int64_t thumbnail_id);
    void clear_vision_analyses(const std::string& video_id);
    void mark_thumbnail_vision_analyzed(int64_t thumbnail_id, const std::string& description);
    
    // Timeline (combined thumbnails + markers)
    std::vector<TimelineEntry> get_timeline(const std::string& video_id);
    
    // Transcription CRUD
    std::vector<TranscriptionSegment> get_transcription(const std::string& video_id);
    void add_transcription(const std::string& video_id,
                          const std::vector<TranscriptionSegment>& segments);
    void clear_transcription(const std::string& video_id);
    
    // Search
    std::vector<SearchResult> keyword_search(const std::string& query, int limit = 20);
    std::vector<SearchResult> semantic_search(const std::vector<float>& embedding, int limit = 20);
    std::vector<SearchResult> hybrid_search(const std::string& query, int limit = 20);
    
    // Embeddings
    void add_embedding(const std::string& id, const std::string& text,
                       const std::vector<float>& embedding,
                       int64_t timestamp_ms = 0);
    void delete_embeddings_for_video(const std::string& video_id);

    // Embedding data (loaded from DB for semantic search)
    struct StoredEmbedding {
        std::string id;         // embedding id (e.g. "video_id:marker_index")
        std::string video_id;   // video this belongs to
        std::string text;       // original text (marker transcript)
        std::vector<float> embedding;  // the embedding vector
        int64_t timestamp_ms = 0;      // associated marker timestamp
    };

    // Load all stored embeddings from database (for semantic search)
    std::vector<StoredEmbedding> get_all_embeddings();

    // Get embeddings for a specific video
    std::vector<StoredEmbedding> get_embeddings_for_video(const std::string& video_id);
    
    // Statistics
    int get_video_count_by_status(VideoStatus status);
    int get_total_video_count();
    int get_marker_count();
    
    // Bulk operations for queue panel
    void clear_all_markers();                           // Clear all markers from all videos
    void clear_all_thumbnails();                        // Clear all thumbnails
    void clear_all_embeddings();                        // Clear all embeddings
    void reset_all_to_pending();                        // Reset all videos to pending
    std::vector<VideoEntry> get_failed_videos();        // Get all failed videos
    void delete_failed_videos();                        // Delete all failed videos
    void reset_completed_to_pending();                  // Reset completed videos for re-analysis
    
    // Retry/broken video handling
    void reset_failed_to_pending(int max_retries = 3);     // Reset failed videos back to pending if retry_count < max_retries
    void increment_retry_count(const std::string& id);       // Increment retry count when video fails
    int get_retry_count(const std::string& id);             // Get retry count for a video
    void mark_video_broken(const std::string& id);          // Mark video as BROKEN (failed too many times)
    
    // Utility
    sqlite3* raw() { return m_db; }
    std::string get_database_path() const { return m_db_path; }

private:
    void initialize_schema();
    void initialize_vec_extension();
    
    sqlite3* m_db = nullptr;
    std::string m_db_path;
};

} // namespace lucid