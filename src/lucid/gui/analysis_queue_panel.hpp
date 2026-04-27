// analysis_queue_panel.hpp - Analysis Queue Panel
// Shows video analysis job queue, current job progress, and recent completions
// Supports 2-pass analysis: Pass 1 (thumbnails + whisper), Pass 2 (vision scan)

#pragma once

#include <gtk/gtk.h>
#include <string>
#include <vector>
#include <functional>

namespace lucid {

// ============================================================================
// Pass State - represents the 2-pass analysis state
// ============================================================================
enum class PassState {
    PENDING,           // Not started
    PASS1_ACTIVE,      // Pass 1 in progress (metadata → thumbnails → transcription)
    PASS1_COMPLETE,    // Pass 1 done, Pass 2 pending (has thumbnails, AUDIO markers)
    PASS2_ACTIVE,      // Pass 2 running (Qwen2-VL analyzing thumbnails)
    PASS2_COMPLETE,    // Both passes complete (has VISION markers with descriptions)
    FAILED             // Analysis failed
};

// Convert PassState to readable string
inline const char* pass_state_to_string(PassState state) {
    switch (state) {
        case PassState::PENDING: return "Pending";
        case PassState::PASS1_ACTIVE: return "Pass 1";
        case PassState::PASS1_COMPLETE: return "Pass 1 Done";
        case PassState::PASS2_ACTIVE: return "Pass 2";
        case PassState::PASS2_COMPLETE: return "Complete";
        case PassState::FAILED: return "Failed";
        default: return "Unknown";
    }
}

// Convert string to PassState
inline PassState pass_state_from_string(const std::string& str) {
    if (str == "pass1") return PassState::PASS1_ACTIVE;
    if (str == "pass2") return PassState::PASS2_ACTIVE;
    if (str == "pass1_complete" || str == "pass1-done") return PassState::PASS1_COMPLETE;
    if (str == "complete") return PassState::PASS2_COMPLETE;
    if (str == "failed") return PassState::FAILED;
    return PassState::PENDING;
}

// ============================================================================
// QueueJob - represents a job in the queue
// ============================================================================
struct QueueJob {
    std::string video_id;
    std::string video_title;
    
    PassState pass_state = PassState::PENDING;
    std::string phase;        // "metadata", "thumbnails", "transcription", "vision", "complete"
    float progress = 0.0f;    // 0.0 to 1.0
    
    // Vision scan progress (Pass 2)
    int vision_analyzed_count = 0;  // Thumbnails analyzed by vision
    int vision_total_count = 0;      // Total thumbnails to analyze
    
    // Analysis results
    int thumbnail_count = 0;    // Total thumbnails generated
    int audio_marker_count = 0; // AUDIO markers from whisper
    int vision_marker_count = 0; // VISION markers with descriptions
    
    // Error info
    std::string error_message;
};

// ============================================================================
// QueueStats - statistics about the video library
// ============================================================================
struct QueueStats {
    int total_videos = 0;
    
    // Video counts by pass state
    int pending = 0;
    int pass1_active = 0;              // Pass 1 in progress
    int pass1_complete_pass2_pending = 0;  // Pass 1 done, waiting for Pass 2
    int pass2_active = 0;              // Pass 2 in progress
    int completed = 0;                  // Full complete (both passes)
    int partial_complete = 0;           // Pass 1 done, Pass 2 skipped/failed
    int failed = 0;
    
    // Marker counts
    int total_markers = 0;
    int audio_markers = 0;   // AUDIO markers from whisper
    int vision_markers = 0;  // VISION markers with descriptions
    
    // Legacy alias for backward compatibility
    int processing() const { return pass1_active + pass2_active; }
};

// ============================================================================
// AnalysisQueuePanel
// ============================================================================
class AnalysisQueuePanel {
public:
    AnalysisQueuePanel();
    ~AnalysisQueuePanel();
    
    // Initialize the panel UI
    void initialize(GtkWidget* parent);
    
    // Get the main widget
    GtkWidget* get_widget() const { return m_box; }
    
    // Update the queue with new jobs
    void update_queue(int pending_count, int processing_count);
    
    // Update progress for current job - pass-aware version (with PassState enum)
    void on_progress(const std::string& video_id,
                    PassState pass_state,
                    const std::string& phase,
                    float progress,
                    int vision_analyzed = 0,
                    int vision_total = 0);
    
    // Update progress with string-based pass state (converts to PassState enum internally)
    void on_progress(const std::string& video_id,
                    const std::string& pass_state_str,
                    const std::string& phase,
                    float progress,
                    int vision_analyzed = 0,
                    int vision_total = 0);
    
    // Legacy overload for backward compatibility (treats as PASS1_ACTIVE)
    void on_progress(const std::string& video_id,
                    const std::string& phase,
                    float progress) {
        on_progress(video_id, PassState::PASS1_ACTIVE, phase, progress, 0, 0);
    }
    
    // Job completed
    void on_complete(const std::string& video_id, bool success,
                     const std::string& error = "");
    
    // Clear the recent completions list
    void clear_recent();
    
    // Set video title lookup callback
    void set_title_lookup(std::function<std::string(const std::string& video_id)> cb);
    
    // Set callback for reset to pending button
    void set_reset_callback(std::function<void()> cb);
    
    // Set callback for re-analyze all
    void set_reanalyze_all_callback(std::function<void()> cb);
    
    // Set callback for clear all analysis data
    void set_clear_all_callback(std::function<void()> cb);
    
    // Set callback for cancel current job
    void set_cancel_callback(std::function<void()> cb);
    
    // Set callback for remove failed videos
    void set_remove_failed_callback(std::function<void()> cb);
    
    // Set callback to get queue stats from database
    void set_stats_callback(std::function<QueueStats()> cb);
    
    // Refresh the queue from database
    void refresh();
    
    // Update stats display
    void update_stats(const QueueStats& stats);
    
    // Update overall progress
    void update_overall_progress(int completed, int total);
    
    // Set a callback to get pending video IDs (for populating list)
    void set_pending_videos_callback(
        std::function<std::vector<std::pair<std::string, std::string>>()> cb);
    
    // Update pending list display (public for refresh)
    void update_pending_list();
    
    // Public helper for getting title from ID
    std::string get_title(const std::string& video_id) const {
        if (m_title_lookup) {
            return m_title_lookup(video_id);
        }
        return video_id;
    }
    
    // Show/hide cancel button based on processing state
    void set_processing(bool is_processing);
    
    // Set vision progress callback (for Pass 2 updates)
    void set_vision_progress_callback(
        std::function<void(int analyzed, int total)> cb);
    
    // Set video stats lookup callback (to get actual counts from database)
    void set_video_stats_callback(
        std::function<void(const std::string& video_id, int& thumb_count, int& marker_count, int& vision_count)> cb);

private:
    void create_ui();
    void update_current_job_display();
    void add_to_recent(const QueueJob& job);
    GtkWidget* create_job_item(const QueueJob& job);
    
    GtkWidget* m_box = nullptr;
    GtkWidget* m_scrolled = nullptr;
    GtkWidget* m_current_job_box = nullptr;
    GtkWidget* m_current_title_label = nullptr;
    GtkWidget* m_current_phase_label = nullptr;
    GtkWidget* m_current_progress_bar = nullptr;
    GtkWidget* m_pending_list = nullptr;
    GtkWidget* m_recent_list = nullptr;
    GtkWidget* m_pending_count_label = nullptr;
    
    // Stats display labels
    GtkWidget* m_stats_label = nullptr;
    GtkWidget* m_overall_progress_bar = nullptr;
    GtkWidget* m_overall_progress_label = nullptr;
    
    // Action buttons
    GtkWidget* m_cancel_btn = nullptr;
    GtkWidget* m_reanalyze_all_btn = nullptr;
    
    // Current job info
    std::string m_current_video_id;
    std::string m_current_video_title;
    PassState m_current_pass_state = PassState::PENDING;
    std::string m_current_phase;
    float m_current_progress = 0.0f;
    bool m_is_processing = false;
    
    // Vision progress
    int m_vision_analyzed_count = 0;
    int m_vision_total_count = 0;
    
    // Analysis results
    int m_thumbnail_count = 0;
    int m_audio_marker_count = 0;
    int m_vision_marker_count = 0;
    
    // Pending jobs
    int m_pending_count = 0;
    int m_processing_count = 0;
    
    // Overall progress
    int m_total_videos = 0;
    int m_completed_videos = 0;
    
    // Recent completions (max 5)
    std::vector<QueueJob> m_recent_completions;
    
    // Callback to get video title from ID
    std::function<std::string(const std::string&)> m_title_lookup;
    
    // Callbacks for actions
    std::function<void()> m_reset_callback;
    std::function<void()> m_reanalyze_all_callback;
    std::function<void()> m_clear_all_callback;
    std::function<void()> m_cancel_callback;
    std::function<void()> m_remove_failed_callback;
    std::function<QueueStats()> m_stats_callback;
    
    // Callback to get pending video list (id, title pairs)
    std::function<std::vector<std::pair<std::string, std::string>>()> m_pending_videos_callback;
    
    // Vision progress callback
    std::function<void(int, int)> m_vision_progress_callback;
    
    // Video stats lookup callback (to get actual counts from database)
    std::function<void(const std::string&, int&, int&, int&)> m_video_stats_callback;
};

} // namespace lucid
