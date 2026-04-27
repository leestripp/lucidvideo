// ffmpeg_pipeline.hpp - FFmpeg Video Processing Pipeline
//
// Audio extraction, thumbnail generation, metadata extraction

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <chrono>

namespace lucid {

// ============================================================================
// Data Structures
// ============================================================================

struct VideoMetadata {
    int64_t duration_ms = 0;           // Duration in milliseconds
    int width = 0;
    int height = 0;
    double fps = 0.0;
    std::string codec;
    std::string format;
    int bitrate = 0;
    int audio_channels = 0;
    int audio_sample_rate = 0;
};

struct Thumbnail {
    std::string path;                  // Path to thumbnail image
    int64_t timestamp_ms = 0;           // Timestamp in video
};

struct AudioChunk {
    int index;
    std::string path;                  // Path to extracted audio
    int64_t start_ms = 0;
    int64_t end_ms = 0;
};

// ============================================================================
// FFmpeg Pipeline
// ============================================================================

class FFmpegPipeline {
public:
    FFmpegPipeline();
    ~FFmpegPipeline();
    
    // Check if FFmpeg is available
    static bool is_available();
    
    // Video metadata extraction
    std::optional<VideoMetadata> get_metadata(const std::string& video_path);
    
    // Audio extraction (16kHz mono WAV for Whisper)
    std::optional<std::string> extract_audio(const std::string& video_path);
    
    // Chunked audio extraction for parallel processing
    std::vector<AudioChunk> extract_audio_chunks(const std::string& video_path, 
                                                 int chunk_duration_ms = 30000);
    
    // Thumbnail generation
    std::vector<Thumbnail> extract_thumbnails(const std::string& video_path, 
                                              int count = 3);
    
    // Extract single frame at timestamp
    std::optional<std::string> extract_frame(const std::string& video_path,
                                            int64_t timestamp_ms,
                                            const std::string& output_path);
    
    // Cleanup temporary files
    void cleanup();

private:
    std::string m_cache_dir;
    
    // Internal helpers
    std::string get_cache_dir();
    std::string generate_cache_path(const std::string& prefix, 
                                    const std::string& extension);
    bool run_command(const std::string& command);
};

} // namespace lucid