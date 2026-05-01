// video_analysis_worker.hpp - Background worker service for video analysis
//
// Runs in its own thread, processes videos from the queue
// Supports 2-pass analysis: Pass 1 (thumbnails + whisper), Pass 2 (vision scan)

#pragma once

#include "video_analyst.hpp"
#include "video_library.hpp"
#include "model_manager.hpp"
#include "../logging.hpp"

#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <functional>

// GLib for main context dispatch
#include <glib.h>

namespace lucid {

// ============================================================================
// Analysis Worker Service
// ============================================================================

class VideoAnalysisWorker {
public:
    // Callback types for UI updates - PASS-AWARE VERSION
    // Progress callback with pass state information
    // pass_state: "pass1", "pass2", "pass1_complete", "complete"
    // vision_analyzed and vision_total are for Pass 2 progress display
    using ProgressCallback = std::function<void(const std::string& video_id,
                                                const std::string& pass_state,
                                                const std::string& phase,
                                                float progress,
                                                int vision_analyzed,
                                                int vision_total)>;
    using CompletionCallback = std::function<void(const std::string& video_id, 
                                                  bool success,
                                                  const std::string& error)>;
    using QueueUpdateCallback = std::function<void(int pending_count, int processing_count)>;
    // Called after Pass 2 (vision enhancement) completes - to refresh UI with updated markers
    using VisionUpdateCallback = std::function<void(const std::string& video_id)>;
    
    VideoAnalysisWorker(Database& db, const std::string& cache_dir);
    ~VideoAnalysisWorker();
    
    // Prevent copying
    VideoAnalysisWorker(const VideoAnalysisWorker&) = delete;
    VideoAnalysisWorker& operator=(const VideoAnalysisWorker&) = delete;
    
    // Start/stop the worker
    void start();
    void stop();
    
    // Restart the worker (after stop) - clears flags and starts fresh
    void restart();
    
    // Check if running
    bool is_running() const { return m_running.load(); }
    
    // Check if processing
    bool is_processing() const { return m_current_video_id.has_value(); }
    
    // Get current video being processed
    std::optional<std::string> current_video_id() const { return m_current_video_id; }
    
    // Trigger a check for pending videos (for manual trigger)
    void trigger_check();
    
    // Get statistics
    struct Stats {
        int pending_count = 0;
        int processing_count = 0;
        int completed_count = 0;
        int failed_count = 0;
        std::string current_video_id;
        std::string current_phase;
        float current_progress = 0.0f;
    };
    Stats get_stats();
    
    // Set callbacks
    void set_progress_callback(ProgressCallback cb) { 
        LUCID_INFO("VideoAnalysisWorker::set_progress_callback called");
        m_progress_callback = std::move(cb); 
    }
    void set_completion_callback(CompletionCallback cb) { 
        LUCID_INFO("VideoAnalysisWorker::set_completion_callback called");
        m_completion_callback = std::move(cb);
        LUCID_INFO("VideoAnalysisWorker::set_completion_callback done, callback is now %s", 
                   m_completion_callback ? "SET" : "NOT SET");
    }
    void set_queue_update_callback(QueueUpdateCallback cb) { 
        LUCID_INFO("VideoAnalysisWorker::set_queue_update_callback called");
        m_queue_update_callback = std::move(cb); 
    }
    void set_vision_update_callback(VisionUpdateCallback cb) {
        LUCID_DEBUG("VideoAnalysisWorker::set_vision_update_callback called");
        m_vision_update_callback = std::move(cb);
    }
    
    // Reset all videos to pending and trigger check
    void reset_all_to_pending();
    
    // Reset only incomplete videos (completed but missing thumbnails/markers) to pending
    // This fixes videos that were incorrectly marked as completed
    void reset_incomplete_videos();
    
    // Request cancellation of current job (stops at next checkpoint)
    void request_cancel();
    
    // Check if cancel was requested
    bool is_cancel_requested() const { return m_cancel_requested.load(); }
    
    // Clear cancel flag
    void clear_cancel_request() { m_cancel_requested.store(false); }
    
    // Pause/resume the worker
    void pause() { m_paused.store(true); LUCID_INFO("VideoAnalysisWorker: Paused"); }
    void resume() { m_paused.store(false); m_cv.notify_one(); LUCID_INFO("VideoAnalysisWorker: Resumed"); }
    bool is_paused() const { return m_paused.load(); }
    
    // Configuration
    void set_poll_interval(int seconds) { m_poll_interval = seconds; }
    int get_poll_interval() const { return m_poll_interval; }
    
    void set_max_concurrent(int max) { m_max_concurrent = max; }
    int get_max_concurrent() const { return m_max_concurrent; }
    
    // Idle poll interval - longer interval when no work to reduce CPU usage
    void set_idle_poll_interval(int seconds) { m_idle_poll_interval = seconds; }
    int get_idle_poll_interval() const { return m_idle_poll_interval; }
    
    // Active poll interval - shorter interval when work is available
    void set_active_poll_interval(int seconds) { m_active_poll_interval = seconds; }
    int get_active_poll_interval() const { return m_active_poll_interval; }

private:
    void worker_loop();
    void check_for_work();
    void process_next_video();
    void mark_video_completed(const std::string& video_id);
    void mark_video_failed(const std::string& video_id, const std::string& error);
    
    Database& m_db;
    VideoLibrary m_library;
    std::string m_cache_dir;
    
    std::thread m_worker_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stop_requested{false};
    std::atomic<bool> m_check_requested{false};
    std::atomic<bool> m_cancel_requested{false};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_idle_mode{false};  // True when no work and waiting for trigger
    
    std::mutex m_mutex;
    std::condition_variable m_cv;
    
    // Current processing state
    std::optional<std::string> m_current_video_id;
    std::string m_current_phase;
    float m_current_progress = 0.0f;
    
    // Configuration
    int m_poll_interval = 5;  // seconds between checks
    int m_max_concurrent = 1;  // process one at a time
    
    // Idle/active poll intervals for adaptive polling
    int m_idle_poll_interval = 30;   // 30 seconds when idle (reduce CPU usage)
    int m_active_poll_interval = 2;  // 2 seconds when work is available
    
    // Callbacks
    ProgressCallback m_progress_callback;
    CompletionCallback m_completion_callback;
    QueueUpdateCallback m_queue_update_callback;
    VisionUpdateCallback m_vision_update_callback;
    
    // Statistics
    int m_completed_count = 0;
    int m_failed_count = 0;
};

} // namespace lucid