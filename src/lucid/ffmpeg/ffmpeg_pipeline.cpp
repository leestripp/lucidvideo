// ffmpeg_pipeline.cpp - FFmpeg Video Processing Pipeline Implementation
//
// Audio extraction, thumbnail generation, metadata extraction

#include "ffmpeg_pipeline.hpp"

#include <giomm.h>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <iostream>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
}

namespace lucid {

// ============================================================================
// FFmpegPipeline Implementation
// ============================================================================

FFmpegPipeline::FFmpegPipeline() {
    m_cache_dir = get_cache_dir();
    
    // Ensure cache directory exists
    std::filesystem::create_directories(m_cache_dir);
}

FFmpegPipeline::~FFmpegPipeline() {
    // Cleanup will be called explicitly if needed
}

std::string FFmpegPipeline::get_cache_dir() {
    auto cache_base = Glib::get_user_cache_dir();
    return Glib::build_filename(cache_base, "lucid", "ffmpeg");
}

std::string FFmpegPipeline::generate_cache_path(const std::string& prefix,
                                             const std::string& extension) {
    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    auto random = std::rand();
    
    std::ostringstream oss;
    oss << m_cache_dir << "/" << prefix << "_" << timestamp << "_" << random << extension;
    return oss.str();
}

bool FFmpegPipeline::run_command(const std::string& command) {
    int result = std::system(command.c_str());
    return result == 0;
}

bool FFmpegPipeline::is_available() {
    static bool checked = false;
    static bool available = false;
    
    if (!checked) {
        available = (std::system("which ffmpeg > /dev/null 2>&1") == 0);
        checked = true;
    }
    
    return available;
}

// ============================================================================
// Metadata Extraction
// ============================================================================

std::optional<VideoMetadata> FFmpegPipeline::get_metadata(const std::string& video_path) {
    AVFormatContext* fmt_ctx = nullptr;
    
    // Open input file
    if (avformat_open_input(&fmt_ctx, video_path.c_str(), nullptr, nullptr) != 0) {
        std::cerr << "[ERROR] Failed to open video: " << video_path << std::endl;
        return std::nullopt;
    }
    
    // Get stream info
    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        std::cerr << "[ERROR] Failed to find stream info" << std::endl;
        avformat_close_input(&fmt_ctx);
        return std::nullopt;
    }
    
    VideoMetadata metadata;
    
    // Duration (convert to milliseconds)
    metadata.duration_ms = (fmt_ctx->duration * 1000) / AV_TIME_BASE;
    
    // Format
    if (fmt_ctx->iformat) {
        metadata.format = fmt_ctx->iformat->name ? fmt_ctx->iformat->name : "";
    }
    
    // Bitrate
    metadata.bitrate = fmt_ctx->bit_rate;
    
    // Find video and audio streams
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; ++i) {
        auto* stream = fmt_ctx->streams[i];
        auto* codecpar = stream->codecpar;
        
        if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            metadata.width = codecpar->width;
            metadata.height = codecpar->height;
            
            // FPS
            if (stream->avg_frame_rate.den > 0) {
                metadata.fps = av_q2d(stream->avg_frame_rate);
            }
            
            // Codec
            const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
            if (codec) {
                metadata.codec = codec->name;
            }
        }
        else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            metadata.audio_channels = codecpar->ch_layout.nb_channels;
            metadata.audio_sample_rate = codecpar->sample_rate;
        }
    }
    
    avformat_close_input(&fmt_ctx);
    
    return metadata;
}

// ============================================================================
// Audio Extraction
// ============================================================================

std::optional<std::string> FFmpegPipeline::extract_audio(const std::string& video_path) {
    auto output_path = generate_cache_path("audio", ".wav");
    
    // Extract 16kHz mono WAV using system FFmpeg
    std::ostringstream cmd;
    cmd << "ffmpeg -i \"" << video_path << "\""
        << " -ar 16000 -ac 1 -c:a pcm_s16le -f wav -y \""
        << output_path << "\" 2>/dev/null";
    
    if (run_command(cmd.str())) {
        // Verify output file exists
        if (std::filesystem::exists(output_path)) {
            return output_path;
        }
    }
    
    return std::nullopt;
}

std::vector<AudioChunk> FFmpegPipeline::extract_audio_chunks(const std::string& video_path,
                                                           int chunk_duration_ms) {
    std::vector<AudioChunk> chunks;
    
    // Get duration first
    auto metadata = get_metadata(video_path);
    if (!metadata) {
        std::cerr << "[ERROR] Cannot get video duration for chunking" << std::endl;
        return chunks;
    }
    
    int64_t duration_ms = metadata->duration_ms;
    int64_t current_ms = 0;
    int chunk_index = 0;
    
    while (current_ms < duration_ms) {
        int64_t chunk_start = current_ms;
        int64_t chunk_end = std::min(current_ms + chunk_duration_ms, duration_ms);
        
        // Generate output path
        auto output_path = generate_cache_path("chunk", ".wav");
        
        // Build FFmpeg command for segment
        std::ostringstream cmd;
        cmd << "ffmpeg -i \"" << video_path << "\""
            << " -ss " << (chunk_start / 1000.0)
            << " -t " << ((chunk_end - chunk_start) / 1000.0)
            << " -ar 16000 -ac 1 -c:a pcm_s16le -f wav -y \""
            << output_path << "\" 2>/dev/null";
        
        if (run_command(cmd.str()) && std::filesystem::exists(output_path)) {
            AudioChunk chunk;
            chunk.index = chunk_index;
            chunk.path = output_path;
            chunk.start_ms = chunk_start;
            chunk.end_ms = chunk_end;
            chunks.push_back(chunk);
        }
        
        current_ms = chunk_end;
        chunk_index++;
    }
    
    return chunks;
}

// ============================================================================
// Thumbnail Extraction
// ============================================================================

std::vector<Thumbnail> FFmpegPipeline::extract_thumbnails(const std::string& video_path,
                                                           int count) {
    std::vector<Thumbnail> thumbnails;
    
    auto metadata = get_metadata(video_path);
    if (!metadata || metadata->duration_ms == 0) {
        std::cerr << "[ERROR] Cannot get video duration for thumbnails" << std::endl;
        return thumbnails;
    }
    
    int64_t duration_ms = metadata->duration_ms;
    
    // Generate timestamps at evenly spaced intervals
    for (int i = 0; i < count; ++i) {
        double fraction = (i + 1.0) / (count + 1.0);
        int64_t timestamp_ms = static_cast<int64_t>(duration_ms * fraction);
        
        auto output_path = generate_cache_path("thumb", ".jpg");
        
        if (extract_frame(video_path, timestamp_ms, output_path)) {
            Thumbnail thumb;
            thumb.path = output_path;
            thumb.timestamp_ms = timestamp_ms;
            thumbnails.push_back(thumb);
        }
    }
    
    return thumbnails;
}

std::optional<std::string> FFmpegPipeline::extract_frame(const std::string& video_path,
                                                        int64_t timestamp_ms,
                                                        const std::string& output_path) {
    // Use system FFmpeg for frame extraction
    std::ostringstream cmd;
    cmd << "ffmpeg -ss " << (timestamp_ms / 1000.0)
        << " -i \"" << video_path << "\""
        << " -vframes 1 -q:v 2 -y \"" << output_path << "\" 2>/dev/null";
    
    if (run_command(cmd.str()) && std::filesystem::exists(output_path)) {
        return output_path;
    }
    
    return std::nullopt;
}

// ============================================================================
// Cleanup
// ============================================================================

void FFmpegPipeline::cleanup() {
    try {
        std::filesystem::remove_all(m_cache_dir);
    } catch (const std::exception& e) {
        std::cerr << "[WARN] Failed to cleanup cache: " << e.what() << std::endl;
    }
}

} // namespace lucid