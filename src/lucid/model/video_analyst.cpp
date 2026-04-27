// video_analyst.cpp - Video Analyst Coordinator Implementation
//
// Coordinates thumbnail generation, transcription, and marker extraction

#include "video_analyst.hpp"
#include "../ffmpeg/thumbnail_generator.hpp"
#include "../logging.hpp"

#include <giomm.h>
#include <glib.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>

namespace fs = std::filesystem;

namespace lucid {

// ============================================================================
// VideoAnalyst Implementation
// ============================================================================

VideoAnalyst::VideoAnalyst(const std::string& cache_dir, Database& db)
    : m_db(db)
    , m_library(db)
    , m_cache_dir(cache_dir) {
    
    // Create cache directories
    fs::create_directories(m_cache_dir + "/thumbnails");
    fs::create_directories(m_cache_dir + "/audio");
}

std::string VideoAnalyst::format_timecode(int64_t ms) {
    int64_t total_seconds = ms / 1000;
    int64_t hours = total_seconds / 3600;
    int64_t minutes = (total_seconds % 3600) / 60;
    int64_t seconds = total_seconds % 60;
    int64_t millis = ms % 1000;
    
    char buf[32];
    snprintf(buf, sizeof(buf), "%02lld:%02lld:%02lld.%03lld", 
             (long long)hours, (long long)minutes, (long long)seconds, (long long)millis);
    return std::string(buf);
}

int64_t VideoAnalyst::whisper_timestamp_to_ms(int64_t cs) {
    // Whisper returns timestamps in centiseconds (1/100th second)
    return cs * 10;
}

ImportResult VideoAnalyst::import_and_analyze(
    const std::string& path,
    const std::string& title,
    std::function<void(const AnalysisProgress&)> progress_callback) {
    
    // Import video
    ImportResult result = m_library.import(path, title);
    
    // Check if import succeeded
    if (std::holds_alternative<std::string>(result)) {
        // Check if it's an error message
        std::string id = std::get<std::string>(result);
        if (id.find("failed") != std::string::npos) {
            // Import failed
            return result;
        }
        
        // Import succeeded with new video ID
        // Analyze it
        auto analysis = analyze(path, id, progress_callback);
        if (!analysis.has_value() || !analysis->success) {
            // Analysis failed, but import succeeded
            // The error is stored in the database
        }
        
        return result;
    }
    
    // Duplicate found - return the DuplicateFound result
    return result;
}

std::optional<VideoAnalysisResult> VideoAnalyst::analyze(
    const std::string& video_path,
    const std::string& video_id,
    std::function<void(const AnalysisProgress&)> progress_callback) {
    
    VideoAnalysisResult result;
    result.video_id = video_id;
    
    PIPELINE_DEBUG("=== Video Analysis Pipeline Started ===");
    PIPELINE_DEBUG("Video ID: %s", video_id.c_str());
    PIPELINE_DEBUG("Video path: %s", video_path.c_str());
    PIPELINE_DEBUG("Vision scan: ALWAYS ENABLED (hardcoded)");
    
    if (m_is_analyzing) {
        PIPELINE_DEBUG("Analysis already in progress - returning error");
        result.success = false;
        result.error_message = "Analysis already in progress";
        return result;
    }
    
    m_is_analyzing = true;
    m_current_phase = "starting";
    
    auto report_progress = [&](const std::string& phase, double progress, const std::string& message) {
        if (progress_callback) {
            AnalysisProgress p;
            p.phase = phase;
            p.progress = progress;
            p.message = message;
            progress_callback(p);
        }
    };
    
    try {
        // Phase 1: Update status to ANALYZING
        PIPELINE_DEBUG("Phase 1: Getting video metadata...");
        report_progress("metadata", 0.0, "Getting video information...");
        update_status(video_id, VideoStatus::ANALYZING);
        
        // Get video duration
        auto duration_opt = get_video_duration(video_path);
        if (!duration_opt.has_value()) {
            PIPELINE_DEBUG("Failed to get video duration");
            result.success = false;
            result.error_message = "Failed to get video duration";
            m_library.set_error(video_id, result.error_message);
            m_is_analyzing = false;
            return result;
        }
        result.duration_ms = *duration_opt;
        PIPELINE_DEBUG("Video duration: %ld ms (%.1f seconds)", result.duration_ms, result.duration_ms / 1000.0);
        m_library.update_duration(video_id, result.duration_ms);
        
        // Phase 2: Generate thumbnails
        PIPELINE_DEBUG("Phase 2: Generating thumbnails...");
        report_progress("thumbnails", 0.1, "Generating thumbnails...");
        update_status(video_id, VideoStatus::THUMBNAILING);
        
        auto thumbnails = generate_thumbnails(video_path, video_id);
        result.thumbnail_count = static_cast<int>(thumbnails.size());
        result.thumbnails = thumbnails;
        PIPELINE_DEBUG("Generated %d thumbnails", result.thumbnail_count);
        
        // Phase 3: Transcription
        PIPELINE_DEBUG("Phase 3: Transcription...");
        report_progress("transcription", 0.3, "Transcribing audio...");
        update_status(video_id, VideoStatus::TRANSCRIBING);
        
        auto markers = transcribe_video(video_path, video_id, 
            [&](double progress) {
                report_progress("transcription", 0.3 + progress * 0.6, "Transcribing...");
            });
        result.marker_count = static_cast<int>(markers.size());
        result.markers = markers;
        PIPELINE_DEBUG("Created %d transcription markers", result.marker_count);
        
        // Phase 4: Vision scanning (if enabled)
        if (m_vision_model_ready && !thumbnails.empty()) {
            PIPELINE_DEBUG("Phase 4: Vision scanning (%d thumbnails available)...", static_cast<int>(thumbnails.size()));
            report_progress("vision_scan", 0.9, "Analyzing video frames...");
            update_status(video_id, VideoStatus::VISION_SCANNING);
            
            auto vision_results = scan_thumbnails_with_vision(video_id, thumbnails,
                [&](double progress) {
                    report_progress("vision_scan", 0.9 + progress * 0.09, "Vision analysis...");
                });
            
            result.vision_count = static_cast<int>(vision_results.size());
            result.vision_analyses = std::move(vision_results);
            PIPELINE_DEBUG("Vision scan complete: %d analyses", result.vision_count);
        } else {
            PIPELINE_DEBUG("Phase 4: Vision scan skipped (model_ready=%s, thumbnails=%zu)",
                         m_vision_model_ready ? "yes" : "no",
                         thumbnails.size());
        }
        
        // Phase 5: Complete
        PIPELINE_DEBUG("Phase 5: Marking video as complete");
        report_progress("complete", 1.0, "Analysis complete");
        update_status(video_id, VideoStatus::COMPLETED);
        
        result.success = true;
        PIPELINE_DEBUG("=== Video Analysis Pipeline Complete ===");
        PIPELINE_DEBUG("Results: %d thumbnails, %d markers, %d vision analyses",
                     result.thumbnail_count, result.marker_count, result.vision_count);
        
    } catch (const std::exception& e) {
        PIPELINE_DEBUG("EXCEPTION during analysis: %s", e.what());
        result.success = false;
        result.error_message = e.what();
        m_library.set_error(video_id, result.error_message);
    }
    
    m_is_analyzing = false;
    m_current_phase = "";
    
    PIPELINE_DEBUG("Returning VideoAnalysisResult: success=%d, thumbnails=%d, markers=%d, duration=%ld",
                   result.success, result.thumbnail_count, result.marker_count, result.duration_ms);
    return result;
}

void VideoAnalyst::update_status(const std::string& video_id, VideoStatus status) {
    m_library.update_status(video_id, status);
}

std::optional<int64_t> VideoAnalyst::get_video_duration(const std::string& video_path) {
    ThumbnailGenerator gen;
    return gen.get_duration_ms(video_path);
}

std::vector<ThumbnailEntry> VideoAnalyst::generate_thumbnails(
    const std::string& video_path,
    const std::string& video_id) {
    
    std::vector<ThumbnailEntry> results;
    
    // Create thumbnails directory for this video
    std::string thumb_dir = m_cache_dir + "/thumbnails/" + video_id;
    fs::create_directories(thumb_dir);
    
    ThumbnailGenerator gen;
    
    // Use adaptive generation for better distribution
    // Get duration first to log the thumbnail count we're using
    auto duration_opt = gen.get_duration_ms(video_path);
    if (duration_opt.has_value()) {
        ThumbnailConfig config;
        [[maybe_unused]] int thumbnail_count = ThumbnailGenerator::calculate_adaptive_count(*duration_opt, config);
        THUMB_DEBUG("[VideoAnalyst] Using adaptive thumbnails: %d for %.1f min video", 
                   thumbnail_count, *duration_opt / 60000.0);
    }
    
    auto thumbnails = gen.generate_adaptive(video_path, thumb_dir, m_thumbnail_size);
    
    for (const auto& thumb : thumbnails) {
        ThumbnailEntry entry;
        entry.video_id = video_id;
        entry.path = thumb.path;
        entry.timestamp_ms = thumb.timestamp_ms;
        entry.timecode = thumb.timecode;
        entry.width = thumb.width;
        entry.height = thumb.height;
        
        // Save to database
        int64_t id = m_db.add_thumbnail(entry);
        entry.id = id;
        results.push_back(entry);
    }
    
    // Generate high-res thumbnails for vision scanning
    if (m_vision_thumbnail_size > 0) {
        LUCID_DEBUG("[VideoAnalyst] Generating vision thumbnails at %dpx", m_vision_thumbnail_size);
        std::string vision_dir = thumb_dir + "/vision";
        fs::create_directories(vision_dir);
        
        // Use extract_at to generate vision thumbnails
        for (const auto& thumb : thumbnails) {
            std::string vision_path = vision_dir + "/" + 
                fs::path(thumb.path).filename().string();
            
            auto vision_thumb = gen.extract_at(video_path, vision_path, 
                                                thumb.timestamp_ms, m_vision_thumbnail_size);
            if (vision_thumb) {
                LUCID_DEBUG("[VideoAnalyst] Created vision thumbnail: %s (%dx%d)", 
                           vision_thumb->path.c_str(), vision_thumb->width, vision_thumb->height);
            }
        }
    }
    
    return results;
}

std::vector<Marker> VideoAnalyst::transcribe_video(
    const std::string& video_path,
    const std::string& video_id,
    std::function<void(double)> progress_callback) {
    
    std::vector<Marker> results;
    
    TRANSCRIBE_DEBUG("=== Transcription Started ===");
    TRANSCRIBE_DEBUG("Video: %s", video_path.c_str());
    TRANSCRIBE_DEBUG("Video ID: %s", video_id.c_str());
    
    // FIRST: Check if video has an audio stream
    {
        std::ostringstream probe_cmd;
        probe_cmd << "ffprobe -v error -select_streams a -show_entries stream=codec_name -of csv=p=0 \""
                  << video_path << "\" 2>/dev/null";
        FILE* fp = popen(probe_cmd.str().c_str(), "r");
        bool has_audio = false;
        if (fp) {
            char buf[256];
            if (fgets(buf, sizeof(buf), fp)) {
                has_audio = (strlen(buf) > 0);
            }
            pclose(fp);
        }
        
        if (!has_audio) {
            TRANSCRIBE_DEBUG("No audio stream found");
            MARKER_DEBUG("Adding [NO AUDIO] marker for video %s", video_id.c_str());
            
            // Check existing markers to avoid unique constraint conflict
            auto existing = m_db.get_markers(video_id);
            int next_index = static_cast<int>(existing.size());
            
            // Add a "NO AUDIO" marker so user knows this video has no audio
            Marker marker;
            marker.video_id = video_id;
            marker.segment_index = next_index;
            marker.start_ms = 0;
            marker.end_ms = 0;
            marker.timecode_start = "00:00:00.000";
            marker.timecode_end = "00:00:00.000";
            marker.text = "[NO AUDIO]";
            marker.confidence = 1.0f;
            marker.word_count = 0;
            marker.marker_type = MarkerType::AUDIO;
            marker.thumbnail_path = SPEAKER_ICON_PATH;
            m_db.add_marker(marker);
            return results;
        }
        TRANSCRIBE_DEBUG("Audio stream detected");
    }
    
    // Check if transcription engine is available
    TranscriptionEngine engine;
    
    // Try to find Whisper model
    std::string model_path = TranscriptionEngine::get_default_model_path();
    TRANSCRIBE_DEBUG("Whisper model path: %s", model_path.c_str());
    
    // If no model, skip transcription but don't fail
    if (!fs::exists(model_path)) {
        TRANSCRIBE_DEBUG("Whisper model not found - skipping transcription");
        // Return empty results - transcription is optional
        return results;
    }
    
    // Initialize engine
    if (!engine.initialize(model_path)) {
        TRANSCRIBE_DEBUG("Failed to initialize transcription engine");
        return results;
    }
    TRANSCRIBE_DEBUG("Transcription engine initialized");
    
    // Extract audio to WAV
    std::string audio_path = m_cache_dir + "/audio/" + video_id + ".wav";
    fs::create_directories(m_cache_dir + "/audio");
    
    // Create a simple FFmpeg command to extract audio
    // IMPORTANT: Add -ss 0 before -i for fast seeking to prevent hangs
    // Also add timeout to prevent indefinite blocking
    std::ostringstream audio_cmd;
    audio_cmd << "timeout 60 ffmpeg -y -ss 0 -i \"" << video_path << "\" "
              << "-vn -acodec pcm_s16le -ar 16000 -ac 1 "
              << "\"" << audio_path << "\" 2>/dev/null";
    
    int result = std::system(audio_cmd.str().c_str());
    // timeout returns 124 if command timed out
    if (result == 124) {
        TRANSCRIBE_DEBUG("Audio extraction timed out");
        std::cerr << "[WARN] Audio extraction timed out for: " << video_path << std::endl;
        return results;
    }
    if (result != 0) {
        TRANSCRIBE_DEBUG("Audio extraction failed with code: %d", result);
        // Audio extraction failed (video has audio but extraction failed)
        std::cerr << "[WARN] Audio extraction failed with code: " << result << " for: " << video_path << std::endl;
        return results;
    }
    
    if (!fs::exists(audio_path)) {
        TRANSCRIBE_DEBUG("Audio file not created");
        std::cerr << "[WARN] Audio file not created: " << audio_path << std::endl;
        return results;
    }
    
    TRANSCRIBE_DEBUG("Audio extracted to: %s (size: %ld bytes)", 
                   audio_path.c_str(), fs::file_size(audio_path));
    
    // Run transcription
    TRANSCRIBE_DEBUG("Starting Whisper transcription...");
    auto transcription_result = engine.transcribe(audio_path, video_id, progress_callback);
    
    if (!transcription_result.has_value()) {
        TRANSCRIBE_DEBUG("Transcription returned no result");
        return results;
    }
    
    TRANSCRIBE_DEBUG("Transcription complete: %zu segments", transcription_result->segments.size());
    
    // Convert transcription segments to markers
    int segment_index = 0;
    for (const auto& seg : transcription_result->segments) {
        MARKER_DEBUG("Creating AUDIO marker #%d at %s-%s: \"%s...\"", 
                    segment_index, format_timecode(seg.start_ms).c_str(),
                    format_timecode(seg.end_ms).c_str(),
                    seg.text.substr(0, 30).c_str());
        
        Marker marker;
        marker.video_id = video_id;
        marker.segment_index = segment_index++;
        marker.start_ms = seg.start_ms;
        marker.end_ms = seg.end_ms;
        marker.timecode_start = format_timecode(seg.start_ms);
        marker.timecode_end = format_timecode(seg.end_ms);
        marker.text = seg.text;
        marker.confidence = seg.confidence;
        marker.word_count = static_cast<int>(marker.text.length() / 5);  // Rough estimate
        
        // Set AUDIO marker type with speaker icon thumbnail
        marker.marker_type = MarkerType::AUDIO;
        marker.thumbnail_path = SPEAKER_ICON_PATH;
        MARKER_DEBUG("  -> AUDIO marker with speaker icon thumbnail");

        // Save to database
        int64_t id = m_db.add_marker(marker);
        MARKER_DEBUG("  -> Marker saved with id=%ld", id);
        marker.id = id;
        results.push_back(marker);

        // Generate and store embedding for this marker text
        generate_embedding(video_id, id, seg.start_ms, seg.text);
    }

    TRANSCRIBE_DEBUG("=== Transcription Complete: %zu AUDIO markers created ===", results.size());
    
    // Cleanup audio file
    fs::remove(audio_path);
    
    return results;
}

std::vector<Marker> VideoAnalyst::get_markers(const std::string& video_id) {
    return m_db.get_markers(video_id);
}

std::vector<ThumbnailEntry> VideoAnalyst::get_thumbnails(const std::string& video_id) {
    return m_db.get_thumbnails(video_id);
}

std::vector<TimelineEntry> VideoAnalyst::get_timeline(const std::string& video_id) {
    return m_db.get_timeline(video_id);
}

std::vector<Marker> VideoAnalyst::search_markers(const std::string& query, int limit) {
    return m_db.search_markers(query, limit);
}

std::string VideoAnalyst::get_default_embedding_model_path() {
    return Glib::build_filename(Glib::get_user_data_dir(), "lucid", "models", "nomic-embed-text-v1.5.Q4_K_M.gguf");
}

void VideoAnalyst::generate_embedding(const std::string& video_id,
                                       int64_t marker_id,
                                       int64_t timestamp_ms,
                                       const std::string& text) {
    // Lazy init embedding store
    static std::unique_ptr<lucid::EmbeddingStore> embed_store;
    static bool checked_model = false;

    if (!checked_model) {
        checked_model = true;
        std::string model_path = get_default_embedding_model_path();
        if (!fs::exists(model_path)) {
            std::cerr << "[WARN] Embedding model not found: " << model_path << std::endl;
            return;
        }
        embed_store = std::make_unique<lucid::EmbeddingStore>();
        embed_store->set_num_threads(4);
        embed_store->set_context_size(2048);
        embed_store->set_use_gpu(false);
        if (!embed_store->initialize(model_path)) {
            std::cerr << "[WARN] Failed to init embedding store" << std::endl;
            embed_store.reset();
            return;
        }
        std::cerr << "[INFO] Embedding store initialized" << std::endl;
    }

    if (!embed_store || text.empty()) return;

    auto result = embed_store->get_embedding(text);
    if (!result.has_value()) return;

    std::string emb_id = video_id + ":" + std::to_string(marker_id);
    m_db.add_embedding(emb_id, text, result->embedding, timestamp_ms);
    std::cerr << "[INFO] Stored embedding for marker " << marker_id << std::endl;
}

// ============================================================================
// Vision Scanner Methods
// ============================================================================

bool VideoAnalyst::init_vision_scanner(const std::string& model_path, 
                                       const std::string& mmproj_path) {
    if (!fs::exists(model_path)) {
        std::cerr << "[WARN] Vision model not found: " << model_path << std::endl;
        return false;
    }
    
    if (!fs::exists(mmproj_path)) {
        std::cerr << "[WARN] Vision projector not found: " << mmproj_path << std::endl;
        return false;
    }
    
    std::cerr << "[INFO] Initializing vision scanner with:" << std::endl;
    std::cerr << "  - Model: " << model_path << std::endl;
    std::cerr << "  - Projector: " << mmproj_path << std::endl;
    
    if (m_vision_scanner.initialize(model_path, mmproj_path)) {
        m_vision_model_ready = true;
        std::cerr << "[INFO] Vision scanner ready" << std::endl;
        return true;
    }
    
    std::cerr << "[WARN] Failed to initialize vision scanner" << std::endl;
    return false;
}

std::vector<VisionAnalysis> VideoAnalyst::scan_thumbnails_with_vision(
    const std::string& video_id,
    const std::vector<ThumbnailEntry>& thumbnails,
    std::function<void(double)> progress_callback) {
    
    std::vector<VisionAnalysis> results;
    
    VISION_DEBUG("=== Vision Scan Started ===");
    VISION_DEBUG("Video ID: %s", video_id.c_str());
    VISION_DEBUG("Total thumbnails: %zu", thumbnails.size());
    VISION_DEBUG("Vision scan interval: %d, max per video: %d", 
                 m_vision_scan_interval, m_vision_max_per_video);
    
    if (thumbnails.empty()) {
        VISION_DEBUG("No thumbnails available - returning empty");
        return results;
    }
    
    // Select thumbnails to analyze based on interval and max
    std::vector<std::pair<std::string, int64_t>> to_analyze;
    for (size_t i = 0; i < thumbnails.size(); ++i) {
        // Apply interval (every Nth thumbnail)
        if (static_cast<int>(i) % m_vision_scan_interval != 0) {
            continue;
        }
        // Apply max limit
        if (static_cast<int>(to_analyze.size()) >= m_vision_max_per_video) {
            break;
        }
        // Skip already analyzed thumbnails
        if (thumbnails[i].vision_analyzed) {
            VISION_DEBUG("Skipping thumbnail %zu - already analyzed", i);
            continue;
        }
        
        // Use vision thumbnail if available, otherwise fall back to display thumbnail
        std::string thumb_path = thumbnails[i].path;
        std::string vision_path = fs::path(thumbnails[i].path).parent_path().parent_path().string() + 
                                   "/vision/" + fs::path(thumbnails[i].path).filename().string();
        
        if (m_vision_thumbnail_size > 0 && fs::exists(vision_path)) {
            thumb_path = vision_path;
            VISION_DEBUG("Using vision thumbnail: %s", thumb_path.c_str());
        } else {
            VISION_DEBUG("Using display thumbnail (no vision version): %s", thumb_path.c_str());
        }
        
        VISION_DEBUG("Selected thumbnail %zu for vision scan: %s at %ld ms", 
                    i, thumb_path.c_str(), thumbnails[i].timestamp_ms);
        to_analyze.emplace_back(thumb_path, thumbnails[i].timestamp_ms);
    }
    
    if (to_analyze.empty()) {
        VISION_DEBUG("No thumbnails selected for analysis");
        std::cerr << "[INFO] No thumbnails to analyze for video " << video_id << std::endl;
        return results;
    }
    
    VISION_DEBUG("Analyzing %zu thumbnails with vision...", to_analyze.size());
    std::cerr << "[INFO] Analyzing " << to_analyze.size() << " thumbnails with vision..." << std::endl;
    
    // Process in batches for efficiency
    const int batch_size = 4;
    size_t processed = 0;
    
    while (processed < to_analyze.size()) {
        size_t end = std::min(processed + batch_size, to_analyze.size());
        std::vector<std::pair<std::string, int64_t>> batch(to_analyze.begin() + processed, to_analyze.begin() + end);
        
        VISION_DEBUG("Processing batch %zu-%zu of %zu", processed, end, to_analyze.size());
        auto batch_results = m_vision_scanner.analyze_batch(batch, batch_size);
        VISION_DEBUG("Batch complete: %zu successful analyses", batch_results.size());
        
        for (auto& scan_result : batch_results) {
            if (scan_result.success) {
                VISION_DEBUG("Processing vision result for: %s", scan_result.thumbnail_path.c_str());
                
                VisionAnalysis analysis;
                analysis.video_id = video_id;
                analysis.description = scan_result.description;
                analysis.scene_type = scan_result.scene_type;
                analysis.text_content = scan_result.text_content;
                analysis.confidence = scan_result.confidence;
                analysis.processing_time_ms = scan_result.processing_time_ms;
                analysis.model_used = m_vision_scanner.get_model_id();
                
                // Store in database
                // Note: We need to find the thumbnail ID by path/timestamp
                for (const auto& thumb : thumbnails) {
                    if (thumb.path == scan_result.thumbnail_path) {
                        analysis.thumbnail_id = thumb.id;
                        VISION_DEBUG("Matched thumbnail ID: %ld", thumb.id);
                        break;
                    }
                }
                
                int64_t id = m_db.add_vision_analysis(analysis);
                VISION_DEBUG("Vision analysis stored with id=%ld", id);
                analysis.id = id;
                results.push_back(analysis);
                
                // Mark thumbnail as analyzed
                if (analysis.thumbnail_id > 0) {
                    m_db.mark_thumbnail_vision_analyzed(analysis.thumbnail_id, scan_result.description);
                    VISION_DEBUG("Thumbnail %ld marked as vision-analyzed", analysis.thumbnail_id);
                }
                
                // Create a VISION marker for this analysis
                MARKER_DEBUG("Creating VISION marker for thumbnail at %ld ms", scan_result.timestamp_ms);
                
                Marker marker;
                marker.video_id = video_id;
                marker.marker_type = MarkerType::VISION;
                marker.start_ms = scan_result.timestamp_ms;
                marker.end_ms = scan_result.timestamp_ms;
                marker.timecode_start = format_timecode(scan_result.timestamp_ms);
                marker.timecode_end = format_timecode(scan_result.timestamp_ms);
                marker.text = scan_result.description;
                marker.confidence = scan_result.confidence;
                marker.thumbnail_path = scan_result.thumbnail_path;
                marker.word_count = static_cast<int>(scan_result.description.length() / 5);
                
                // Set segment_index based on existing markers
                auto existing_markers = m_db.get_markers(video_id);
                marker.segment_index = static_cast<int>(existing_markers.size());
                
                int64_t marker_id = m_db.add_marker(marker);
                MARKER_DEBUG("  -> VISION marker created with id=%ld", marker_id);
                
                // Generate embedding for the vision description
                generate_embedding(video_id, marker_id, marker.start_ms, scan_result.description);
                MARKER_DEBUG("  -> VISION marker embedding generated for id=%ld", marker_id);
                
            } else {
                VISION_DEBUG("Vision analysis failed: %s", scan_result.error_message.c_str());
            }
        }
        
        processed = end;
        
        // Report progress
        if (progress_callback) {
            progress_callback(static_cast<double>(processed) / to_analyze.size());
        }
        
        std::cerr << "[INFO] Processed " << processed << "/" << to_analyze.size() << " thumbnails" << std::endl;
    }
    
    VISION_DEBUG("=== Vision Scan Complete: %zu analyses ===", results.size());
    std::cerr << "[INFO] Vision scan complete: " << results.size() << " analyses stored" << std::endl;
    return results;
}

// ============================================================================
// Two-Pass Analysis Implementation
// ============================================================================

std::vector<Marker> VideoAnalyst::create_vision_markers(
    const std::string& video_id,
    const std::vector<ThumbnailEntry>& thumbnails,
    bool include_all) {
    
    std::vector<Marker> markers;
    
    // Clear existing VISION markers for this video to avoid duplicates
    // This handles the case where a video is re-analyzed
    // We'll delete VISION markers after getting all existing to determine segment_index
    auto existing = m_db.get_markers(video_id);
    
    // Count non-VISION markers to determine starting segment_index
    int segment_index = 0;
    for (const auto& marker : existing) {
        if (marker.marker_type != MarkerType::VISION) {
            segment_index = std::max(segment_index, marker.segment_index + 1);
        }
    }
    
    // Now delete existing VISION markers (we've already determined segment_index)
    for (const auto& marker : existing) {
        if (marker.marker_type == MarkerType::VISION) {
            m_db.delete_marker(marker.id);
            LUCID_DEBUG("Deleted existing VISION marker %ld for re-analysis", marker.id);
        }
    }
    
    for (size_t i = 0; i < thumbnails.size(); ++i) {
        // If not include_all, only take every m_vision_scan_interval-th thumbnail
        if (!include_all && static_cast<int>(i) % m_vision_scan_interval != 0) {
            continue;
        }
        
        const auto& thumb = thumbnails[i];
        
        Marker marker;
        marker.video_id = video_id;
        marker.segment_index = segment_index++;
        marker.marker_type = MarkerType::VISION;
        marker.start_ms = thumb.timestamp_ms;
        marker.end_ms = thumb.timestamp_ms;
        marker.timecode_start = thumb.timecode;
        marker.timecode_end = thumb.timecode;
        marker.text = "";  // Blank - will be filled by vision scan
        marker.confidence = 1.0f;  // No confidence yet
        marker.thumbnail_path = thumb.path;
        marker.word_count = 0;
        
        int64_t id = m_db.add_marker(marker);
        marker.id = id;
        
        PIPELINE_DEBUG("Created VISION marker #%d at %ld ms (blank description)", 
                      marker.segment_index, thumb.timestamp_ms);
        
        markers.push_back(marker);
    }
    
    return markers;
}

void VideoAnalyst::update_marker_description(int64_t marker_id, const std::string& description, float confidence) {
    // Get all markers and find the one with matching ID
    // Note: This is inefficient but works for now
    // TODO: Add a direct get_marker_by_id() method to Database for better performance
    
    auto markers = m_db.get_markers("");  // Get all markers (empty video_id returns all)
    for (auto& marker : markers) {
        if (marker.id == marker_id) {
            marker.text = description;
            marker.confidence = confidence;
            m_db.update_marker(marker);
            
            // Generate embedding for the vision description
            generate_embedding(marker.video_id, marker.id, marker.start_ms, description);
            
            PIPELINE_DEBUG("Updated marker %ld with description: %s (confidence: %.2f)", 
                          marker_id, description.c_str(), confidence);
            return;
        }
    }
    
    PIPELINE_DEBUG("Marker %ld not found for description update", marker_id);
}

std::optional<Pass1Result> VideoAnalyst::analyze_pass1(
    const std::string& video_path,
    const std::string& video_id,
    std::function<void(const AnalysisProgress&)> progress_callback) {
    
    Pass1Result result;
    result.video_id = video_id;
    
    if (m_is_analyzing) {
        PIPELINE_DEBUG("Analysis already in progress");
        result.error_message = "Analysis already in progress";
        return std::nullopt;
    }
    
    m_is_analyzing = true;
    m_current_phase = "pass1";
    
    auto report_progress = [&](const std::string& phase, double progress, const std::string& message) {
        if (progress_callback) {
            AnalysisProgress p;
            p.phase = phase;
            p.progress = progress;
            p.message = message;
            progress_callback(p);
        }
    };
    
    try {
        // Phase 1: Get video duration
        PIPELINE_DEBUG("[Pass1] Phase 1: Getting video metadata...");
        report_progress("metadata", 0.0, "Getting video information...");
        update_status(video_id, VideoStatus::ANALYZING);
        
        auto duration_opt = get_video_duration(video_path);
        if (!duration_opt.has_value()) {
            result.success = false;
            result.error_message = "Failed to get video duration";
            m_is_analyzing = false;
            return result;
        }
        
        PIPELINE_DEBUG("[Pass1] Video duration: %ld ms", *duration_opt);
        m_library.update_duration(video_id, *duration_opt);
        
        // Phase 2: Generate thumbnails (fast - just FFmpeg)
        PIPELINE_DEBUG("[Pass1] Phase 2: Generating thumbnails...");
        report_progress("thumbnails", 0.2, "Generating thumbnails...");
        update_status(video_id, VideoStatus::THUMBNAILING);
        
        auto thumbnails = generate_thumbnails(video_path, video_id);
        result.thumbnail_count = static_cast<int>(thumbnails.size());
        result.thumbnails = thumbnails;  // Store for updating video thumbnail
        PIPELINE_DEBUG("[Pass1] Generated %d thumbnails", result.thumbnail_count);
        
        if (thumbnails.empty()) {
            PIPELINE_DEBUG("[Pass1] No thumbnails generated - video may be unreadable");
        }
        
        // Phase 3: Transcription (if video has audio)
        PIPELINE_DEBUG("[Pass1] Phase 3: Transcription...");
        report_progress("transcription", 0.4, "Transcribing audio...");
        update_status(video_id, VideoStatus::TRANSCRIBING);
        
        auto audio_markers = transcribe_video(video_path, video_id,
            [&](double progress) {
                report_progress("transcription", 0.4 + progress * 0.4, "Transcribing...");
            });
        result.audio_markers = audio_markers;
        result.audio_marker_count = static_cast<int>(audio_markers.size());
        PIPELINE_DEBUG("[Pass1] Created %d AUDIO markers", result.audio_marker_count);
        
        // NOTE: Don't create VISION markers here - Pass 2 will call scan_thumbnails_with_vision
        // which creates markers WITH actual descriptions from the vision model.
        // Creating blank markers here would overwrite any real analysis results.
        PIPELINE_DEBUG("[Pass1] Vision markers will be created in Pass 2 by scan_thumbnails_with_vision");
        result.vision_markers = {};
        result.vision_marker_count = 0;
        
        // Phase 5: Mark as complete
        PIPELINE_DEBUG("[Pass1] Phase 5: Marking as complete");
        report_progress("complete", 1.0, "Quick analysis complete");
        update_status(video_id, VideoStatus::COMPLETED);
        
        result.success = true;
        PIPELINE_DEBUG("[Pass1] === Pass 1 Complete ===");
        PIPELINE_DEBUG("[Pass1] Results: %d thumbnails, %d audio markers, %d vision markers",
                      result.thumbnail_count, result.audio_marker_count, result.vision_marker_count);
        
    } catch (const std::exception& e) {
        PIPELINE_DEBUG("[Pass1] EXCEPTION: %s", e.what());
        result.success = false;
        result.error_message = e.what();
        update_status(video_id, VideoStatus::FAILED);
    }
    
    m_is_analyzing = false;
    m_current_phase = "";
    
    return result;
}

std::vector<Marker> VideoAnalyst::get_pending_vision_markers(const std::string& video_id) {
    // Get all markers for this video
    auto all_markers = m_db.get_markers(video_id);
    
    std::vector<Marker> pending;
    for (const auto& marker : all_markers) {
        // VISION markers with blank text are pending
        if (marker.marker_type == MarkerType::VISION && marker.text.empty()) {
            pending.push_back(marker);
        }
    }
    
    return pending;
}

std::optional<Pass2Result> VideoAnalyst::fill_vision_descriptions(
    const std::string& video_id,
    std::function<void(const AnalysisProgress&)> progress_callback) {
    
    Pass2Result result;
    result.video_id = video_id;
    
    LUCID_INFO("[Pass2] fill_vision_descriptions called for video: %s", video_id.c_str());
    LUCID_INFO("[Pass2] Vision scan always enabled, model_ready=%s", 
               m_vision_model_ready ? "true" : "false");
    
    if (!m_vision_model_ready) {
        PIPELINE_DEBUG("[Pass2] Vision model not ready - skipping");
        LUCID_INFO("[Pass2] Skipping - vision model not ready (files missing?)");
        result.error_message = "Vision model not ready";
        return std::nullopt;
    }
    
    // Get pending vision markers
    auto pending = get_pending_vision_markers(video_id);
    
    if (pending.empty()) {
        PIPELINE_DEBUG("[Pass2] No pending VISION markers for %s", video_id.c_str());
        result.success = true;
        return result;
    }
    
    PIPELINE_DEBUG("[Pass2] Found %zu pending VISION markers for %s", pending.size(), video_id.c_str());
    
    auto report_progress = [&](const std::string& phase, double progress, const std::string& message) {
        if (progress_callback) {
            AnalysisProgress p;
            p.phase = phase;
            p.progress = progress;
            p.message = message;
            progress_callback(p);
        }
    };
    
    // Prepare batch data for vision scanner
    std::vector<std::pair<std::string, int64_t>> to_analyze;
    for (const auto& marker : pending) {
        to_analyze.emplace_back(marker.thumbnail_path, marker.start_ms);
    }
    
    // Process in batches
    const int batch_size = 4;
    size_t processed = 0;
    
    report_progress("vision", 0.0, "Analyzing video frames...");
    
    while (processed < to_analyze.size()) {
        size_t end = std::min(processed + batch_size, to_analyze.size());
        std::vector<std::pair<std::string, int64_t>> batch(
            to_analyze.begin() + processed, to_analyze.begin() + end);
        
        auto batch_results = m_vision_scanner.analyze_batch(batch, batch_size);
        
        for (auto& scan_result : batch_results) {
            if (scan_result.success && !scan_result.description.empty()) {
                // Find the corresponding marker by timestamp
                auto all_markers = m_db.get_markers(video_id);
                for (const auto& marker : all_markers) {
                    if (marker.marker_type == MarkerType::VISION && 
                        marker.start_ms == scan_result.timestamp_ms) {
                        // Update this marker with vision description
                        update_marker_description(marker.id, scan_result.description, 
                                                 scan_result.confidence);
                        
                        // Add to updated markers list
                        Marker updated = marker;
                        updated.text = scan_result.description;
                        updated.confidence = scan_result.confidence;
                        result.updated_markers.push_back(updated);
                        
                        result.updated_count++;
                        result.success = true;
                        
                        PIPELINE_DEBUG("[Pass2] Updated marker %ld at %ld ms: %s",
                                      marker.id, scan_result.timestamp_ms, 
                                      scan_result.description.c_str());
                        break;
                    }
                }
            } else {
                PIPELINE_DEBUG("[Pass2] Vision analysis failed: %s", 
                             scan_result.error_message.c_str());
                result.failed_count++;
            }
        }
        
        processed = end;
        
        double progress = static_cast<double>(processed) / to_analyze.size();
        report_progress("vision", progress, 
                      "Analyzing frames... " + std::to_string(processed) + "/" + std::to_string(to_analyze.size()));
    }
    
    PIPELINE_DEBUG("[Pass2] === Pass 2 Complete ===");
    PIPELINE_DEBUG("[Pass2] Updated: %d, Failed: %d", result.updated_count, result.failed_count);
    
    return result;
}

} // namespace lucid