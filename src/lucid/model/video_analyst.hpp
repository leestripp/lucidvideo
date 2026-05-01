// video_analyst.hpp - Video Analyst Coordinator
//
// Coordinates thumbnail generation, transcription, and marker extraction
// Supports two-pass analysis:
//   Pass 1: Quick analysis (thumbnails + whisper) - fast, updates UI immediately
//   Pass 2: Vision enhancement (Qwen2-VL) - slow, fills in marker descriptions

#pragma once

#include "database.hpp"
#include "video_library.hpp"
#include "transcription_engine.hpp"
#include "../ffmpeg/thumbnail_generator.hpp"
#include "../inference/embedding_store.hpp"
#include "../vision/vision_scanner.hpp"

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <memory>

namespace lucid {

// ============================================================================
// Data Structures
// ============================================================================

// Speaker icon path for AUDIO markers (transcription)
inline constexpr const char* SPEAKER_ICON_PATH = "resource:speaker.svg";

// Vision scanner default configuration
inline constexpr int DEFAULT_THUMBNAIL_SIZE = 128;       // pixels - reduced for faster processing
inline constexpr int DEFAULT_VISION_SCAN_INTERVAL = 5;   // analyze every 5th thumbnail
inline constexpr int DEFAULT_VISION_MAX_PER_VIDEO = 50; // max thumbnails to analyze per video

// Description status for markers (especially VISION markers)
enum class DescriptionStatus {
    PENDING,  // Description not yet filled (vision scan pending)
    FILLED,   // Description is filled
    FAILED    // Vision scan failed for this marker
};

struct VideoAnalysisResult {
    std::string video_id;
    bool success = false;
    std::string error_message;
    
    // Statistics
    int64_t duration_ms = 0;
    int thumbnail_count = 0;
    int marker_count = 0;
    int vision_count = 0;  // Number of thumbnails analyzed by vision scan
    
    // Results
    std::vector<ThumbnailEntry> thumbnails;
    std::vector<Marker> markers;
    std::vector<VisionAnalysis> vision_analyses;
};

// Result for pass 1 (quick analysis)
struct Pass1Result {
    std::string video_id;
    bool success = false;
    std::string error_message;
    int thumbnail_count = 0;
    int audio_marker_count = 0;  // AUDIO markers from whisper
    int vision_marker_count = 0;  // VISION markers (blank descriptions)
    std::vector<Marker> audio_markers;
    std::vector<Marker> vision_markers;
    std::vector<ThumbnailEntry> thumbnails;  // Generated thumbnails for updating video thumbnail
};

// Result for pass 2 (vision enhancement)
struct Pass2Result {
    std::string video_id;
    bool success = false;
    std::string error_message;
    int updated_count = 0;      // Number of markers updated
    int failed_count = 0;       // Number of markers that failed
    std::vector<Marker> updated_markers;
};

struct AnalysisProgress {
    std::string phase;        // "metadata", "thumbnails", "transcription", "vision", "complete"
    double progress = 0.0;    // 0.0 to 1.0
    std::string message;
};

// ============================================================================
// VideoAnalyst
// ============================================================================

class VideoAnalyst {
public:
    VideoAnalyst(const std::string& cache_dir, Database& db);
    ~VideoAnalyst() = default;
    
    // Legacy: Single-pass analysis (thumbnail + whisper + vision)
    // Kept for backward compatibility, may be deprecated
    std::optional<VideoAnalysisResult> analyze(
        const std::string& video_path,
        const std::string& video_id,
        std::function<void(const AnalysisProgress&)> progress_callback = nullptr);
    
    // ========== Two-Pass Analysis ==========
    
    // Pass 1: Quick analysis (thumbnail + whisper + create markers)
    // Fast - updates UI immediately
    // Creates AUDIO markers from whisper transcription
    // Creates VISION markers from thumbnails (with blank descriptions)
    std::optional<Pass1Result> analyze_pass1(
        const std::string& video_path,
        const std::string& video_id,
        std::function<void(const AnalysisProgress&)> progress_callback = nullptr);
    
    // Pass 2: Vision enhancement
    // Fills in descriptions for VISION markers with pending status
    // Should be run in background after Pass 1 completes
    std::optional<Pass2Result> fill_vision_descriptions(
        const std::string& video_id,
        std::function<void(const AnalysisProgress&)> progress_callback = nullptr);
    
    // Get markers needing vision description
    std::vector<Marker> get_pending_vision_markers(const std::string& video_id);
    
    // ========== End Two-Pass Analysis ==========
    
    // Import video and optionally analyze
    ImportResult import_and_analyze(
        const std::string& path,
        const std::string& title,
        std::function<void(const AnalysisProgress&)> progress_callback = nullptr);
    
    // Get analysis results
    std::vector<Marker> get_markers(const std::string& video_id);
    std::vector<ThumbnailEntry> get_thumbnails(const std::string& video_id);
    std::vector<TimelineEntry> get_timeline(const std::string& video_id);
    
    // Search markers
    std::vector<Marker> search_markers(const std::string& query, int limit = 20);

    // Get default embedding model path
    static std::string get_default_embedding_model_path();
    
    // Configuration
    void set_thumbnail_interval(int seconds) { m_thumbnail_interval = seconds; }
    int get_thumbnail_interval() const { return m_thumbnail_interval; }
    
    void set_thumbnail_size(int pixels) { m_thumbnail_size = pixels; }
    int get_thumbnail_size() const { return m_thumbnail_size; }
    
    // Vision scan configuration (always enabled - no setter)
    bool is_vision_scan_enabled() const { return true; }  // Always returns true
    
    void set_vision_scan_interval(int interval) { m_vision_scan_interval = interval; }
    int get_vision_scan_interval() const { return m_vision_scan_interval; }
    
    void set_vision_max_per_video(int max) { m_vision_max_per_video = max; }
    int get_vision_max_per_video() const { return m_vision_max_per_video; }
    
    void set_vision_thumbnail_size(int pixels) { m_vision_thumbnail_size = pixels; }
    int get_vision_thumbnail_size() const { return m_vision_thumbnail_size; }
    
    // Status
    bool is_analyzing() const { return m_is_analyzing; }
    std::string current_phase() const { return m_current_phase; }
    
    // Get vision scanner for initialization
    VisionScanner& get_vision_scanner() { return m_vision_scanner; }
    bool has_vision_model() const { return m_vision_model_ready; }
    
    // Vision scan - analyze thumbnails with Qwen2-VL
    bool init_vision_scanner(const std::string& model_path, const std::string& mmproj_path);
    
    // Public: scan thumbnails with vision model (creates markers WITH descriptions)
    std::vector<VisionAnalysis> scan_thumbnails_with_vision(
        const std::string& video_id,
        const std::vector<ThumbnailEntry>& thumbnails,
        std::function<void(double)> progress_callback = nullptr,
        std::function<bool()> cancel_callback = nullptr);
    
private:
    Database& m_db;
    VideoLibrary m_library;
    
    std::string m_cache_dir;
    int m_thumbnail_interval = 10;  // seconds
    int m_thumbnail_size = 128;     // pixels (reduced to 128 for faster processing)
    
    // Vision scan settings (always enabled - no toggle)
    int m_vision_scan_interval = 2;  // Analyze every 2nd thumbnail (50% coverage)
    int m_vision_max_per_video = 50; // Max thumbnails to analyze per video
    bool m_vision_model_ready = false;
    
    // Vision thumbnail size for AI analysis (separate from display thumbnail)
    int m_vision_thumbnail_size = 512;  // pixels - 512px default for AI analysis
    
    // Vision scanner (lazy initialized)
    VisionScanner m_vision_scanner;
    
    bool m_is_analyzing = false;
    std::string m_current_phase;
    
    // Internal pipeline steps
    void update_status(const std::string& video_id, VideoStatus status);
    std::optional<int64_t> get_video_duration(const std::string& video_path);
    std::vector<ThumbnailEntry> generate_thumbnails(const std::string& video_path,
                                                     const std::string& video_id);
    std::vector<Marker> transcribe_video(const std::string& video_path,
                                          const std::string& video_id,
                                          std::function<void(double)> progress_callback);

    // Generate and store embedding for marker text (called after transcription)
    void generate_embedding(const std::string& video_id,
                           int64_t marker_id,
                           int64_t timestamp_ms,
                           const std::string& text);
    
    // Create VISION markers from thumbnails (with blank/pending descriptions)
    std::vector<Marker> create_vision_markers(
        const std::string& video_id,
        const std::vector<ThumbnailEntry>& thumbnails,
        bool include_all = false);  // If false, only every m_vision_scan_interval-th thumbnail
    
    // Update marker description from vision scan result
    void update_marker_description(int64_t marker_id, const std::string& description, float confidence);
    
    // Timecode helpers
    static std::string format_timecode(int64_t ms);
    static int64_t whisper_timestamp_to_ms(int64_t cs);  // centiseconds -> milliseconds
};

} // namespace lucid