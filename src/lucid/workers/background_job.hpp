// background_job.hpp - Background job for video processing
#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <atomic>
#include <functional>

namespace lucid {

enum class JobStatus {
    PENDING,
    RUNNING,
    COMPLETED,
    FAILED,
    CANCELLED
};

enum class JobType {
    SCAN_FOLDER,
    TRANSCRIBE_VIDEO,
    GENERATE_EMBEDDINGS,
    EXTRACT_THUMBNAILS,
    EXTRACT_AUDIO
};

struct BackgroundJob {
    std::string id;
    JobType type;
    std::string target_path;     // Video file path or folder path
    std::string target_id;       // Video ID in database
    JobStatus status = JobStatus::PENDING;
    std::string status_message;
    float progress = 0.0f;        // 0.0 to 1.0
    std::chrono::steady_clock::time_point created_at;
    std::chrono::steady_clock::time_point started_at;
    std::chrono::steady_clock::time_point completed_at;
    std::string error_message;
    
    // Results
    std::vector<std::pair<uint64_t, std::string>> segments;  // timestamp_ms, text
};

class BackgroundJobManager {
public:
    BackgroundJobManager();
    ~BackgroundJobManager();
    
    // Prevent copying
    BackgroundJobManager(const BackgroundJobManager&) = delete;
    BackgroundJobManager& operator=(const BackgroundJobManager&) = delete;
    
    // Queue a folder scan
    std::string queue_scan_folder(const std::string& folder_path);
    
    // Queue transcription for a video
    std::string queue_transcribe(const std::string& video_path, const std::string& video_id);
    
    // Queue embedding generation
    std::string queue_embeddings(const std::string& video_id, const std::vector<std::pair<uint64_t, std::string>>& segments);
    
    // Queue audio extraction
    std::string queue_extract_audio(const std::string& video_path, const std::string& output_path);
    
    // Queue thumbnail generation
    std::string queue_thumbnails(const std::string& video_path, const std::string& output_dir);
    
    // Cancel a job
    bool cancel_job(const std::string& job_id);
    
    // Get job status
    BackgroundJob get_job(const std::string& job_id);
    
    // Get all jobs
    std::vector<BackgroundJob> get_all_jobs();
    
    // Get jobs for a video
    std::vector<BackgroundJob> get_jobs_for_video(const std::string& video_id);
    
    // Update job progress (called by workers)
    void update_progress(const std::string& job_id, float progress, const std::string& message = "");
    void complete_job(const std::string& job_id);
    void fail_job(const std::string& job_id, const std::string& error);
    
    // Get/Set callbacks
    using JobCallback = std::function<void(const BackgroundJob&)>;
    void set_on_job_started(JobCallback cb);
    void set_on_job_progress(JobCallback cb);
    void set_on_job_completed(JobCallback cb);
    void set_on_job_failed(JobCallback cb);
    
    // Shutdown
    void shutdown();

private:
    std::string generate_job_id();
    void execute_job(const BackgroundJob& job);
    
    class Impl;
    Impl* m_impl;
};

} // namespace lucid