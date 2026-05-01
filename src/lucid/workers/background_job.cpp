// background_job.cpp - Background job manager implementation
#include "background_job.hpp"
#include "thread_pool.hpp"
#include "../logging.hpp"
#include "../ffmpeg/ffmpeg_pipeline.hpp"
#include "../model/transcription_engine.hpp"
#include "../inference/embedding_store.hpp"
#include <algorithm>

namespace lucid {

// Private implementation
class BackgroundJobManager::Impl {
public:
    // Thread pools with adaptive sizing (0 = auto-detect based on hardware)
    ThreadPool io_pool{0, 0};       // Adaptive I/O threads
    ThreadPool compute_pool{0, 0};  // Adaptive compute threads
    
    std::vector<BackgroundJob> jobs;
    std::mutex jobs_mutex;
    
    // Callbacks
    std::function<void(const BackgroundJob&)> on_started;
    std::function<void(const BackgroundJob&)> on_progress;
    std::function<void(const BackgroundJob&)> on_completed;
    std::function<void(const BackgroundJob&)> on_failed;
    
    std::atomic<bool> cancelled{false};
    
    std::string working_dir;
};

BackgroundJobManager::BackgroundJobManager()
    : m_impl(new Impl()) {
    fprintf(stderr, "[BackgroundJobManager] Constructor: Impl created\n");
    fflush(stderr);
    LUCID_INFO("BackgroundJobManager created");
    fprintf(stderr, "[BackgroundJobManager] Constructor: done\n");
    fflush(stderr);
}

BackgroundJobManager::~BackgroundJobManager() {
    shutdown();
    delete m_impl;
}

std::string BackgroundJobManager::generate_job_id() {
    static uint64_t counter = 0;
    return "job_" + std::to_string(++counter) + "_" + 
           std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
}

std::string BackgroundJobManager::queue_scan_folder(const std::string& folder_path) {
    BackgroundJob job;
    job.id = generate_job_id();
    job.type = JobType::SCAN_FOLDER;
    job.target_path = folder_path;
    job.created_at = std::chrono::steady_clock::now();
    job.status = JobStatus::PENDING;
    
    {
        std::lock_guard<std::mutex> lock(m_impl->jobs_mutex);
        m_impl->jobs.push_back(job);
    }
    
    // Execute immediately
    m_impl->io_pool.push_io_task_void([this, job]() {
        execute_job(job);
    }, TaskPriority::NORMAL, "ScanFolder: " + folder_path);
    
    LUCID_INFO("Queued folder scan job: %s", job.id.c_str());
    return job.id;
}

std::string BackgroundJobManager::queue_transcribe(const std::string& video_path, 
                                                    const std::string& video_id) {
    BackgroundJob job;
    job.id = generate_job_id();
    job.type = JobType::TRANSCRIBE_VIDEO;
    job.target_path = video_path;
    job.target_id = video_id;
    job.created_at = std::chrono::steady_clock::now();
    job.status = JobStatus::PENDING;
    
    {
        std::lock_guard<std::mutex> lock(m_impl->jobs_mutex);
        m_impl->jobs.push_back(job);
    }
    
    // Execute on compute pool (Whisper is CPU/GPU intensive)
    m_impl->compute_pool.push_compute_task_void([this, job]() {
        execute_job(job);
    }, TaskPriority::HIGH, "Transcribe: " + video_path);
    
    LUCID_INFO("Queued transcription job: %s", job.id.c_str());
    return job.id;
}

std::string BackgroundJobManager::queue_embeddings(const std::string& video_id,
                                                    const std::vector<std::pair<uint64_t, std::string>>& segments) {
    BackgroundJob job;
    job.id = generate_job_id();
    job.type = JobType::GENERATE_EMBEDDINGS;
    job.target_id = video_id;
    job.created_at = std::chrono::steady_clock::now();
    job.status = JobStatus::PENDING;
    job.segments = segments;
    
    {
        std::lock_guard<std::mutex> lock(m_impl->jobs_mutex);
        m_impl->jobs.push_back(job);
    }
    
    m_impl->compute_pool.push_compute_task_void([this, job]() {
        execute_job(job);
    }, TaskPriority::NORMAL, "Embeddings: " + video_id);
    
    LUCID_INFO("Queued embedding job: %s", job.id.c_str());
    return job.id;
}

std::string BackgroundJobManager::queue_extract_audio(const std::string& video_path,
                                                       const std::string& output_path) {
    BackgroundJob job;
    job.id = generate_job_id();
    job.type = JobType::EXTRACT_AUDIO;
    job.target_path = video_path;
    job.created_at = std::chrono::steady_clock::now();
    job.status = JobStatus::PENDING;
    
    {
        std::lock_guard<std::mutex> lock(m_impl->jobs_mutex);
        m_impl->jobs.push_back(job);
    }
    
    m_impl->io_pool.push_io_task_void([this, job]() {
        FFmpegPipeline pipeline;
        update_progress(job.id, 0.0f, "Extracting audio...");
        
        auto result = pipeline.extract_audio(job.target_path);
        
        if (result.has_value()) {
            complete_job(job.id);
        } else {
            fail_job(job.id, "Audio extraction failed");
        }
    }, TaskPriority::NORMAL, "ExtractAudio: " + video_path);
    
    LUCID_INFO("Queued audio extraction job: %s", job.id.c_str());
    return job.id;
}

std::string BackgroundJobManager::queue_thumbnails(const std::string& video_path,
                                                 const std::string& output_dir) {
    BackgroundJob job;
    job.id = generate_job_id();
    job.type = JobType::EXTRACT_THUMBNAILS;
    job.target_path = video_path;
    job.created_at = std::chrono::steady_clock::now();
    job.status = JobStatus::PENDING;
    
    {
        std::lock_guard<std::mutex> lock(m_impl->jobs_mutex);
        m_impl->jobs.push_back(job);
    }
    
    m_impl->io_pool.push_io_task_void([this, job]() {
        FFmpegPipeline pipeline;
        update_progress(job.id, 0.0f, "Extracting thumbnails...");
        
        auto thumbs = pipeline.extract_thumbnails(job.target_path, 3);
        
        if (!thumbs.empty()) {
            complete_job(job.id);
        } else {
            fail_job(job.id, "Thumbnail extraction failed");
        }
    }, TaskPriority::LOW, "Thumbnails: " + video_path);
    
    LUCID_INFO("Queued thumbnail job: %s", job.id.c_str());
    return job.id;
}

void BackgroundJobManager::execute_job(const BackgroundJob& job) {
    // Update status to running
    {
        std::lock_guard<std::mutex> lock(m_impl->jobs_mutex);
        for (auto& j : m_impl->jobs) {
            if (j.id == job.id) {
                j.status = JobStatus::RUNNING;
                j.started_at = std::chrono::steady_clock::now();
                break;
            }
        }
    }
    
    // Notify started
    if (m_impl->on_started) {
        m_impl->on_started(get_job(job.id));
    }
    
    LUCID_DEBUG("Executing job: %s (type=%d)", job.id.c_str(), (int)job.type);
    
    // Execute based on type
    switch (job.type) {
        case JobType::SCAN_FOLDER: {
            // Scan folder for videos (placeholder)
            update_progress(job.id, 0.5f, "Scanning folder...");
            complete_job(job.id);
            break;
        }
        
        case JobType::TRANSCRIBE_VIDEO: {
            // Run Whisper transcription
            TranscriptionEngine engine;
            update_progress(job.id, 0.1f, "Loading Whisper model...");
            
            if (engine.initialize("")) {
                update_progress(job.id, 0.2f, "Transcribing...");
                complete_job(job.id);
            } else {
                fail_job(job.id, "Failed to initialize Whisper");
            }
            break;
        }
        
        case JobType::GENERATE_EMBEDDINGS: {
            // Generate embeddings with Llama
            update_progress(job.id, 0.1f, "Loading embedding model...");
            complete_job(job.id);
            break;
        }
        
        case JobType::EXTRACT_AUDIO:
        case JobType::EXTRACT_THUMBNAILS:
            // These are handled via io_pool push
            break;
    }
}

bool BackgroundJobManager::cancel_job(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(m_impl->jobs_mutex);
    
    for (auto& job : m_impl->jobs) {
        if (job.id == job_id) {
            if (job.status == JobStatus::PENDING) {
                job.status = JobStatus::CANCELLED;
                job.completed_at = std::chrono::steady_clock::now();
                LUCID_INFO("Cancelled job: %s", job_id.c_str());
                return true;
            } else if (job.status == JobStatus::RUNNING) {
                m_impl->cancelled = true;
                job.status = JobStatus::CANCELLED;
                job.completed_at = std::chrono::steady_clock::now();
                LUCID_INFO("Cancelled running job: %s", job_id.c_str());
                return true;
            }
        }
    }
    
    return false;
}

BackgroundJob BackgroundJobManager::get_job(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(m_impl->jobs_mutex);
    
    for (const auto& job : m_impl->jobs) {
        if (job.id == job_id) {
            return job;
        }
    }
    
    return {};
}

std::vector<BackgroundJob> BackgroundJobManager::get_all_jobs() {
    std::lock_guard<std::mutex> lock(m_impl->jobs_mutex);
    return m_impl->jobs;
}

std::vector<BackgroundJob> BackgroundJobManager::get_jobs_for_video(const std::string& video_id) {
    std::lock_guard<std::mutex> lock(m_impl->jobs_mutex);
    
    std::vector<BackgroundJob> result;
    for (const auto& job : m_impl->jobs) {
        if (job.target_id == video_id) {
            result.push_back(job);
        }
    }
    
    return result;
}

void BackgroundJobManager::update_progress(const std::string& job_id, 
                                           float progress, 
                                           const std::string& message) {
    std::lock_guard<std::mutex> lock(m_impl->jobs_mutex);
    
    for (auto& job : m_impl->jobs) {
        if (job.id == job_id) {
            job.progress = progress;
            job.status_message = message;
            break;
        }
    }
    
    if (m_impl->on_progress) {
        m_impl->on_progress(get_job(job_id));
    }
}

void BackgroundJobManager::complete_job(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(m_impl->jobs_mutex);
    
    for (auto& job : m_impl->jobs) {
        if (job.id == job_id) {
            job.status = JobStatus::COMPLETED;
            job.progress = 1.0f;
            job.completed_at = std::chrono::steady_clock::now();
            job.status_message = "Completed";
            break;
        }
    }
    
    if (m_impl->on_completed) {
        m_impl->on_completed(get_job(job_id));
    }
    
    LUCID_INFO("Job completed: %s", job_id.c_str());
}

void BackgroundJobManager::fail_job(const std::string& job_id, const std::string& error) {
    std::lock_guard<std::mutex> lock(m_impl->jobs_mutex);
    
    for (auto& job : m_impl->jobs) {
        if (job.id == job_id) {
            job.status = JobStatus::FAILED;
            job.completed_at = std::chrono::steady_clock::now();
            job.error_message = error;
            break;
        }
    }
    
    if (m_impl->on_failed) {
        m_impl->on_failed(get_job(job_id));
    }
    
    LUCID_WARN("Job failed: %s - %s", job_id.c_str(), error.c_str());
}

void BackgroundJobManager::shutdown() {
    LUCID_INFO("BackgroundJobManager shutting down...");
    
    m_impl->io_pool.shutdown();
    m_impl->compute_pool.shutdown();
    
    // Mark pending jobs as cancelled
    std::lock_guard<std::mutex> lock(m_impl->jobs_mutex);
    for (auto& job : m_impl->jobs) {
        if (job.status == JobStatus::PENDING) {
            job.status = JobStatus::CANCELLED;
        }
    }
    
    LUCID_INFO("BackgroundJobManager shutdown complete");
}

void BackgroundJobManager::set_on_job_started(JobCallback cb) {
    m_impl->on_started = std::move(cb);
}

void BackgroundJobManager::set_on_job_progress(JobCallback cb) {
    m_impl->on_progress = std::move(cb);
}

void BackgroundJobManager::set_on_job_completed(JobCallback cb) {
    m_impl->on_completed = std::move(cb);
}

void BackgroundJobManager::set_on_job_failed(JobCallback cb) {
    m_impl->on_failed = std::move(cb);
}

} // namespace lucid