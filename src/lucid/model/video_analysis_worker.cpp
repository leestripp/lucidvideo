// video_analysis_worker.cpp - Background worker service implementation
//
// Runs in its own thread, processes videos from the queue

#include "video_analysis_worker.hpp"

#include "../logging.hpp"
#include <chrono>
#include <algorithm>

namespace lucid {

// ============================================================================
// VideoAnalysisWorker Implementation
// ============================================================================

VideoAnalysisWorker::VideoAnalysisWorker(Database& db, const std::string& cache_dir)
    : m_db(db)
    , m_library(db)
    , m_cache_dir(cache_dir) {
}

VideoAnalysisWorker::~VideoAnalysisWorker() {
    stop();
}

void VideoAnalysisWorker::start() {
    if (m_running.load()) {
        return;
    }
    
    LUCID_INFO("VideoAnalysisWorker: Starting worker thread");
    m_running = true;
    m_stop_requested = false;
    m_worker_thread = std::thread(&VideoAnalysisWorker::worker_loop, this);
}

void VideoAnalysisWorker::stop() {
    if (!m_running.load()) {
        return;
    }
    
    LUCID_INFO("VideoAnalysisWorker: Stopping worker thread");
    m_stop_requested = true;
    m_cv.notify_one();
    
    if (m_worker_thread.joinable()) {
        m_worker_thread.join();
    }
    
    m_running = false;
    LUCID_INFO("VideoAnalysisWorker: Worker thread stopped");
}

void VideoAnalysisWorker::trigger_check() {
    m_check_requested = true;
    m_cv.notify_one();
}

void VideoAnalysisWorker::worker_loop() {
    LUCID_DEBUG("VideoAnalysisWorker: Worker loop started");
    
    while (!m_stop_requested.load()) {
        // Wait for either:
        // - Poll interval timeout (m_poll_interval)
        // - Stop request
        // - Manual check trigger
        
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait_for(lock, std::chrono::seconds(m_poll_interval), [this] {
                // Only wake if stop requested or check explicitly requested
                // Don't wake on paused state change - just timeout
                return m_stop_requested.load() || m_check_requested.load();
            });
            m_check_requested = false;
        }
        
        if (m_stop_requested.load()) {
            break;
        }
        
        // If paused, wait for resume
        if (m_paused.load()) {
            LUCID_DEBUG("VideoAnalysisWorker: Paused, waiting for resume...");
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return !m_paused.load() || m_stop_requested.load(); });
            if (m_stop_requested.load()) break;
            LUCID_DEBUG("VideoAnalysisWorker: Resumed");
        }
        
        // Check for work (skip if idle mode)
        check_for_work();
        
        // If we found work, process it
        if (is_processing()) {
            // Continue processing loop - will wait for completion before checking again
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    LUCID_DEBUG("VideoAnalysisWorker: Worker loop ended");
}

void VideoAnalysisWorker::check_for_work() {
    if (is_processing()) {
        // Already processing a video
        return;
    }
    
    // Find a pending video
    auto pending_videos = m_library.get_pending_videos();
    
    if (pending_videos.empty()) {
        // Enter idle mode if not already in it - this prevents repeated logging
        if (!m_idle_mode.load()) {
            m_idle_mode.store(true);
            LUCID_INFO("VideoAnalysisWorker: Entering idle mode (poll interval: %ds)", m_idle_poll_interval);
            // Update queue callback with 0 pending
            if (m_queue_update_callback) {
                m_queue_update_callback(0, 0);
            }
        }
        // In idle mode - don't keep checking, just wait for trigger
        return;
    }
    
    // Found work - exit idle mode
    bool was_idle = m_idle_mode.exchange(false);
    if (was_idle) {
        LUCID_INFO("VideoAnalysisWorker: Found work, exiting idle mode");
    }
    
    // Get the first pending video
    auto& video = pending_videos[0];
    
    LUCID_INFO("VideoAnalysisWorker: Found pending video: %s", video.title.c_str());
    
    // Set current processing state
    m_current_video_id = video.id;
    m_current_phase = "queued";
    m_current_progress = 0.0f;
    
    // Update queue callback
    if (m_queue_update_callback) {
        m_queue_update_callback(static_cast<int>(pending_videos.size()) - 1, 1);
    }
    
    // Start processing
    process_next_video();
}

void VideoAnalysisWorker::process_next_video() {
    if (!m_current_video_id.has_value()) {
        return;
    }
    
    std::string video_id = *m_current_video_id;
    
    // Get video from database
    auto video_opt = m_library.get_video(video_id);
    if (!video_opt.has_value()) {
        LUCID_ERROR("VideoAnalysisWorker: Video not found: %s", video_id.c_str());
        m_current_video_id.reset();
        return;
    }
    
    auto video = *video_opt;
    
    LUCID_INFO("VideoAnalysisWorker: Processing video: %s (%s)", 
               video.title.c_str(), video.path.c_str());
    
    // Create VideoAnalyst for this job
    VideoAnalyst analyst(m_cache_dir, m_db);
    
    // Initialize vision scanner (always enabled)
    auto& model_mgr = ModelManager::instance();
    auto vision_model = model_mgr.get_vision_model_path();
    auto vision_mmproj = model_mgr.get_vision_mmproj_path();
    
    if (vision_model.has_value() && vision_mmproj.has_value()) {
        analyst.set_vision_scan_interval(model_mgr.get_vision_scan_interval());
        analyst.set_vision_max_per_video(model_mgr.get_vision_max_per_video());
        analyst.set_vision_thumbnail_size(
            lucid::get_vision_extract_pixels(model_mgr.get_vision_extract_size()));
        
        if (analyst.init_vision_scanner(vision_model.value(), vision_mmproj.value())) {
            LUCID_INFO("VideoAnalysisWorker: Vision scanner initialized successfully");
        } else {
            LUCID_WARN("VideoAnalysisWorker: Failed to initialize vision scanner");
        }
    } else {
        LUCID_INFO("VideoAnalysisWorker: Vision model not available (model=%s, mmproj=%s)",
                   vision_model.has_value() ? "yes" : "no",
                   vision_mmproj.has_value() ? "yes" : "no");
    }
    
    // Progress callback for this job - PASS 1
    auto progress_cb = [this, video_id](const AnalysisProgress& progress) {
        m_current_phase = progress.phase;
        m_current_progress = progress.progress;
        
        if (m_progress_callback) {
            m_progress_callback(video_id, "pass1", progress.phase, progress.progress, 0, 0);
        }
    };
    
    // =========================================================================
    // TWO-PASS ANALYSIS
    // =========================================================================
    
    // Pass 1: Quick analysis (thumbnails + whisper + create markers)
    // This is fast and updates UI immediately
    LUCID_INFO("VideoAnalysisWorker: Starting Pass 1 (quick analysis)");
    
    auto pass1_result = analyst.analyze_pass1(video.path, video_id, progress_cb);
    
    if (!pass1_result.has_value()) {
        LUCID_ERROR("VideoAnalysisWorker: Pass 1 returned null");
        mark_video_failed(video_id, "Pass 1 analysis failed");
        return;
    }
    
    if (!pass1_result->success) {
        LUCID_ERROR("VideoAnalysisWorker: Pass 1 failed: %s", pass1_result->error_message.c_str());
        mark_video_failed(video_id, pass1_result->error_message);
        return;
    }
    
    LUCID_INFO("VideoAnalysisWorker: Pass 1 complete - %d thumbnails, %d audio markers, %d vision markers",
               pass1_result->thumbnail_count, pass1_result->audio_marker_count, pass1_result->vision_marker_count);
    
    // Update video thumbnail to first thumbnail path after Pass 1
    if (!pass1_result->thumbnails.empty()) {
        LUCID_DEBUG("VideoAnalysisWorker: Setting video thumbnail to first thumbnail");
        m_library.update_thumbnail(video_id, pass1_result->thumbnails[0].path);
    }
    
    // Mark video as complete after Pass 1 - UI will show thumbnails immediately
    mark_video_completed(video_id);
    
    // =========================================================================
    // Pass 2: Vision enhancement (runs in background after UI update)
    // This is slow - runs Qwen2-VL on thumbnails to create markers with descriptions
    // =========================================================================
    
    LUCID_INFO("VideoAnalysisWorker: Starting Pass 2 (vision enhancement)");
    LUCID_INFO("VideoAnalysisWorker: analyst.has_vision_model()=%s",
               analyst.has_vision_model() ? "true" : "false");
    
    // Get thumbnails for this video
    auto thumbnails = analyst.get_thumbnails(video_id);
    if (!thumbnails.empty() && analyst.has_vision_model()) {
        LUCID_INFO("VideoAnalysisWorker: Running vision scan on %zu thumbnails", thumbnails.size());
        
        // Create a progress callback for vision scanning - PASS 2
        int vision_total = static_cast<int>(thumbnails.size());
        int vision_analyzed = 0;
        auto vision_progress_cb = [this, video_id, &vision_analyzed, vision_total](double progress) {
            m_current_phase = "vision";
            m_current_progress = progress;
            vision_analyzed = static_cast<int>(progress * vision_total);
            
            if (m_progress_callback) {
                m_progress_callback(video_id, "pass2", "Analyzing frames with AI",
                                   progress, vision_analyzed, vision_total);
            }
        };
        
        // scan_thumbnails_with_vision creates markers WITH actual descriptions from the model
        auto vision_results = analyst.scan_thumbnails_with_vision(video_id, thumbnails, vision_progress_cb);
        
        LUCID_INFO("VideoAnalysisWorker: Pass 2 complete - %zu vision analyses created", vision_results.size());
        
        // Trigger UI refresh if any markers were created
        if (!vision_results.empty() && m_vision_update_callback) {
            LUCID_DEBUG("VideoAnalysisWorker: Triggering vision update callback");
            m_vision_update_callback(video_id);
        }
    } else {
        LUCID_INFO("VideoAnalysisWorker: Pass 2 skipped (no thumbnails or vision model not available)");
    }
    
    LUCID_INFO("VideoAnalysisWorker: All analysis complete for video: %s", video_id.c_str());
}

void VideoAnalysisWorker::mark_video_completed(const std::string& video_id) {
    LUCID_INFO("VideoAnalysisWorker: Video completed: %s", video_id.c_str());
    
    m_completed_count++;
    m_current_video_id.reset();
    m_current_phase = "";
    m_current_progress = 0.0f;
    
    LUCID_INFO("VideoAnalysisWorker: m_completion_callback is %s", 
               m_completion_callback ? "SET" : "NOT SET");
    
    LUCID_INFO("VideoAnalysisWorker: Invoking completion callback");
    if (m_completion_callback) {
        LUCID_INFO("VideoAnalysisWorker: Calling completion callback with video_id=%s", video_id.c_str());
        m_completion_callback(video_id, true, "");
        LUCID_INFO("VideoAnalysisWorker: Completion callback returned");
    } else {
        LUCID_WARN("VideoAnalysisWorker: Completion callback is NULL!");
    }
    LUCID_INFO("VideoAnalysisWorker: Completion callback invoked");
    
    // Trigger immediate check for next video
    LUCID_INFO("VideoAnalysisWorker: Calling trigger_check()");
    trigger_check();
    LUCID_INFO("VideoAnalysisWorker: trigger_check() done");
}

void VideoAnalysisWorker::mark_video_failed(const std::string& video_id, const std::string& error) {
    LUCID_ERROR("VideoAnalysisWorker: Video failed: %s - %s", video_id.c_str(), error.c_str());
    
    m_failed_count++;
    m_current_video_id.reset();
    m_current_phase = "";
    m_current_progress = 0.0f;
    
    if (m_completion_callback) {
        m_completion_callback(video_id, false, error);
    }
    
    // Trigger immediate check for next video
    trigger_check();
}

VideoAnalysisWorker::Stats VideoAnalysisWorker::get_stats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Stats stats;
    
    // Count videos by status
    auto all_videos = m_db.get_all_videos(10000, 0);
    
    for (const auto& video : all_videos) {
        switch (video.status) {
            case VideoStatus::PENDING:
                stats.pending_count++;
                break;
            case VideoStatus::ANALYZING:
            case VideoStatus::TRANSCRIBING:
            case VideoStatus::THUMBNAILING:
            case VideoStatus::VISION_SCANNING:
                stats.processing_count++;
                break;
            case VideoStatus::COMPLETED:
                stats.completed_count++;
                break;
            case VideoStatus::FAILED:
                stats.failed_count++;
                break;
            default:
                break;
        }
    }
    
    // If worker isn't actually processing but DB shows processing entries,
    // those are stale - don't count them
    if (!m_current_video_id.has_value() && stats.processing_count > 0) {
        LUCID_WARN("Found %d stale processing entries in DB (worker idle)", stats.processing_count);
        stats.processing_count = 0;
    }
    
    stats.current_video_id = m_current_video_id.value_or("");
    stats.current_phase = m_current_phase;
    stats.current_progress = m_current_progress;
    
    return stats;
}

void VideoAnalysisWorker::reset_all_to_pending() {
    LUCID_INFO("VideoAnalysisWorker: Resetting all videos to pending");
    m_library.reset_all_to_pending();
    trigger_check();
}

void VideoAnalysisWorker::reset_incomplete_videos() {
    LUCID_INFO("VideoAnalysisWorker: Resetting incomplete videos to pending");
    
    // Get all videos that are completed but missing thumbnails
    auto all_videos = m_db.get_all_videos(10000, 0);
    int reset_count = 0;
    
    for (const auto& video : all_videos) {
        if (video.status == VideoStatus::COMPLETED) {
            // Check if this video has thumbnails
            auto thumbnails = m_db.get_thumbnails(video.id);
            if (thumbnails.empty()) {
                // Video is marked complete but has no thumbnails - reset to pending
                LUCID_DEBUG("VideoAnalysisWorker: Resetting incomplete video: %s", video.title.c_str());
                m_library.update_status(video.id, VideoStatus::PENDING);
                reset_count++;
            }
        }
    }
    
    LUCID_INFO("VideoAnalysisWorker: Reset %d incomplete videos to pending", reset_count);
    
    if (reset_count > 0) {
        trigger_check();
    }
}

void VideoAnalysisWorker::request_cancel() {
    LUCID_DEBUG("VideoAnalysisWorker: Cancel requested");
    m_cancel_requested.store(true);
}

} // namespace lucid