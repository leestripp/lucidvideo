# Lucid FFmpeg Pipeline

## Overview

Lucid uses FFmpeg for video processing including:
- Audio extraction (16kHz mono WAV for Whisper)
- Thumbnail generation
- Video duration detection
- YouTube metadata extraction via yt-dlp

**Repository**: [github.com/ggml-org/lucid](https://github.com/ggml-org/lucid)

---

## FFmpeg Audio Extraction

### Why 16kHz Mono WAV?

Whisper expects audio in this format:
- **Sample rate**: 16000 Hz (required)
- **Channels**: 1 (mono)
- **Format**: 16-bit signed integer PCM or 32-bit float

```cpp
// src/lucid/ffmpeg/audio_extractor.hpp
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <functional>

namespace lucid {

struct AudioChunk {
    int index;
    std::string path;      // Path to extracted WAV file
    double start_sec;
    double end_sec;
    bool is_last;
};

struct AudioExtractionResult {
    std::string video_id;
    bool success;
    std::string error_message;
    std::vector<AudioChunk> chunks;
    double duration_sec;
};

class AudioExtractor {
public:
    AudioExtractor();
    ~AudioExtractor();
    
    // Extract audio from video file
    AudioExtractionResult extract(const std::string& video_path,
                                   const std::string& output_dir,
                                   int chunk_duration_sec = 30,
                                   std::function<void(double)> progress_callback = nullptr);
    
    // Extract single chunk (for streaming)
    std::optional<std::string> extract_chunk(const std::string& video_path,
                                              int chunk_index,
                                              double start_sec,
                                              double end_sec,
                                              const std::string& output_path);
    
    // Get video duration
    std::optional<double> get_duration(const std::string& video_path);
    
    // Get audio info (sample rate, channels, etc.)
    struct AudioInfo {
        int sample_rate;
        int channels;
        std::string codec;
        double duration_sec;
        int64_t bitrate;
    };
    std::optional<AudioInfo> get_audio_info(const std::string& video_path);
    
    // Check if file is valid video
    bool is_valid_video(const std::string& path);
    
    // Clean up temp files
    void cleanup_chunks(const std::vector<AudioChunk>& chunks);

private:
    std::string build_extraction_command(const std::string& input,
                                           const std::string& output,
                                           double start_sec = 0,
                                           double duration_sec = 0);
    
    std::string build_chunk_command(const std::string& input,
                                      const std::string& output,
                                      double start_sec,
                                      double end_sec);
    
    bool run_command(const std::string& cmd);
    
    int execute_system(const std::string& cmd,
                        std::string& output,
                        std::string& error);
};
```

### Implementation

**File**: `src/lucid/ffmpeg/audio_extractor.cpp`

```cpp
#include "audio_extractor.hpp"
#include <cstdlib>
#include <sstream>
#include <iomanip>

namespace lucid {

AudioExtractor::AudioExtractor() = default;
AudioExtractor::~AudioExtractor() = default;

AudioExtractionResult AudioExtractor::extract(
    const std::string& video_path,
    const std::string& output_dir,
    int chunk_duration_sec,
    std::function<void(double)> progress_callback) {
    
    AudioExtractionResult result;
    result.video_id = video_path;
    
    // Get video duration
    auto duration = get_duration(video_path);
    if (!duration) {
        result.success = false;
        result.error_message = "Failed to get video duration";
        return result;
    }
    result.duration_sec = *duration;
    
    // Create output directory
    Glib::Dir::make_dir(output_dir);
    
    // Calculate number of chunks
    int n_chunks = static_cast<int>(*duration / chunk_duration_sec) + 1;
    
    for (int i = 0; i < n_chunks; ++i) {
        double start = i * chunk_duration_sec;
        double end = std::min((i + 1) * chunk_duration_sec, *duration);
        
        // Generate output filename
        std::ostringstream filename;
        filename << "chunk_" << std::setfill('0') << std::setw(5) << i << ".wav";
        std::string output_path = Glib::build_filename(output_dir, filename.str());
        
        // Extract chunk
        if (extract_chunk(video_path, i, start, end, output_path)) {
            AudioChunk chunk;
            chunk.index = i;
            chunk.path = output_path;
            chunk.start_sec = start;
            chunk.end_sec = end;
            chunk.is_last = (i == n_chunks - 1);
            result.chunks.push_back(chunk);
        } else {
            result.success = false;
            result.error_message = "Failed to extract chunk " + std::to_string(i);
            cleanup_chunks(result.chunks);
            return result;
        }
        
        // Report progress
        if (progress_callback) {
            progress_callback((i + 1) * 100.0 / n_chunks);
        }
    }
    
    result.success = true;
    return result;
}

std::optional<std::string> AudioExtractor::extract_chunk(
    const std::string& video_path,
    int chunk_index,
    double start_sec,
    double end_sec,
    const std::string& output_path) {
    
    std::string cmd = build_chunk_command(video_path, output_path, start_sec, end_sec);
    
    std::string stderr_output;
    if (!execute_system(cmd, stderr_output)) {
        g_warning("Audio extraction failed: %s", stderr_output.c_str());
        return std::nullopt;
    }
    
    // Verify output file exists
    if (!Glib::File::create_for_path(output_path)->query_exists()) {
        return std::nullopt;
    }
    
    return output_path;
}

std::optional<double> AudioExtractor::get_duration(const std::string& video_path) {
    std::string cmd = "ffprobe -v error -show_entries format=duration "
                      "-of default=noprint_wrappers=1:nokey=1 \"" + video_path + "\"";
    
    std::string output;
    std::string error;
    
    if (!execute_system(cmd, output, error)) {
        return std::nullopt;
    }
    
    try {
        return std::stod(output);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

AudioExtractor::AudioInfo AudioExtractor::get_audio_info(const std::string& video_path) {
    // Use ffprobe to get audio information
    // Implementation similar to get_duration
    // Parse JSON output with nlohmann/json or manually
}

bool AudioExtractor::is_valid_video(const std::string& path) {
    // Check file exists and has video stream
    std::string cmd = "ffprobe -v error -select_streams v:0 -show_entries "
                      "stream=codec_name -of csv=p=0 \"" + path + "\"";
    
    std::string output;
    std::string error;
    
    return execute_system(cmd, output, error) && !output.empty();
}

std::string AudioExtractor::build_chunk_command(
    const std::string& input,
    const std::string& output,
    double start_sec,
    double end_sec) {
    
    std::ostringstream cmd;
    cmd << "ffmpeg -y -ss " << std::fixed << std::setprecision(3) << start_sec
        << " -t " << (end_sec - start_sec)
        << " -i \"" << input << "\""
        << " -ar 16000"          // 16kHz sample rate
        << " -ac 1"              // Mono channel
        << " -c:a pcm_s16le"     // 16-bit signed integer PCM
        << " -f wav"             // WAV format
        << " \"" << output << "\"";
    
    return cmd.str();
}

int AudioExtractor::execute_system(const std::string& cmd,
                                   std::string& stdout_output,
                                   std::string& stderr_output) {
    FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
    if (!pipe) return -1;
    
    char buffer[256];
    stdout_output.clear();
    stderr_output.clear();
    
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        stdout_output += buffer;
    }
    
    int result = pclose(pipe);
    return result;
}

void AudioExtractor::cleanup_chunks(const std::vector<AudioChunk>& chunks) {
    for (const auto& chunk : chunks) {
        try {
            std::filesystem::remove(chunk.path);
        } catch (const std::exception&) {
            // Ignore cleanup errors
        }
    }
}

} // namespace lucid
```

---

## Thumbnail Generation

### Extract Keyframes

```cpp
// src/lucid/ffmpeg/thumbnail_generator.hpp
#pragma once

#include <string>
#include <vector>
#include <optional>

namespace lucid {

struct Thumbnail {
    int index;
    std::string path;      // Path to JPEG/PNG
    double timestamp_sec;   // When in video
};

class ThumbnailGenerator {
public:
    ThumbnailGenerator();
    ~ThumbnailGenerator();
    
    // Generate thumbnails at specific intervals
    std::vector<Thumbnail> generate(const std::string& video_path,
                                   const std::string& output_dir,
                                   int n_thumbnails = 3);
    
    // Generate single thumbnail at timestamp
    std::optional<std::string> extract_frame(const std::string& video_path,
                                             double timestamp_sec,
                                             const std::string& output_path,
                                             int quality = 2);  // 1-31, lower = better
    
    // Get video resolution
    struct VideoInfo {
        int width;
        int height;
        double duration;
        double fps;
    };
    std::optional<VideoInfo> get_video_info(const std::string& video_path);

private:
    std::string build_frame_command(const std::string& input,
                                      const std::string& output,
                                      double timestamp,
                                      int quality);
    
    bool run_command(const std::string& cmd);
};

} // namespace lucid
```

**File**: `src/lucid/ffmpeg/thumbnail_generator.cpp`

```cpp
#include "thumbnail_generator.hpp"
#include <sstream>
#include <iomanip>

namespace lucid {

ThumbnailGenerator::ThumbnailGenerator() = default;
ThumbnailGenerator::~ThumbnailGenerator() = default;

std::vector<Thumbnail> ThumbnailGenerator::generate(
    const std::string& video_path,
    const std::string& output_dir,
    int n_thumbnails) {
    
    std::vector<Thumbnail> thumbnails;
    
    // Get video info
    auto info = get_video_info(video_path);
    if (!info) {
        g_warning("Failed to get video info for %s", video_path.c_str());
        return thumbnails;
    }
    
    // Calculate positions (15%, 50%, 85%)
    std::vector<double> positions;
    for (int i = 0; i < n_thumbnails; ++i) {
        double pos = info->duration * (0.15 + 0.35 * i);
        if (pos > info->duration - 1) {
            pos = info->duration - 1;  // Avoid very end
        }
        positions.push_back(pos);
    }
    
    // Create output directory
    Glib::Dir::make_dir(output_dir);
    
    // Generate thumbnail for each position
    for (int i = 0; i < n_thumbnails; ++i) {
        // Generate output path
        std::ostringstream filename;
        filename << "thumb_" << std::setfill('0') << std::setw(3) << i << ".jpg";
        std::string output_path = Glib::build_filename(output_dir, filename.str());
        
        // Extract frame
        auto result = extract_frame(video_path, positions[i], output_path);
        if (result) {
            Thumbnail thumb;
            thumb.index = i;
            thumb.path = *result;
            thumb.timestamp_sec = positions[i];
            thumbnails.push_back(thumb);
        }
    }
    
    return thumbnails;
}

std::optional<std::string> ThumbnailGenerator::extract_frame(
    const std::string& video_path,
    double timestamp_sec,
    const std::string& output_path,
    int quality) {
    
    std::string cmd = build_frame_command(video_path, output_path, timestamp_sec, quality);
    
    if (std::system(cmd.c_str()) != 0) {
        return std::nullopt;
    }
    
    // Verify file exists
    if (!Glib::File::create_for_path(output_path)->query_exists()) {
        return std::nullopt;
    }
    
    return output_path;
}

ThumbnailGenerator::VideoInfo ThumbnailGenerator::get_video_info(
    const std::string& video_path) {
    
    std::string cmd = "ffprobe -v error -select_streams v:0 "
                      "-show_entries stream=width,height,r_frame_rate,duration "
                      "-show_entries format=duration "
                      "-of json \"" + video_path + "\"";
    
    // Parse JSON output with json library
    // For simplicity, returning placeholder
    // Real implementation would parse ffprobe JSON output
    
    VideoInfo info = {1920, 1080, 0.0, 30.0};  // Placeholder
    
    return info;
}

std::string ThumbnailGenerator::build_frame_command(
    const std::string& input,
    const std::string& output,
    double timestamp,
    int quality) {
    
    std::ostringstream cmd;
    cmd << "ffmpeg -y -ss " << std::fixed << std::setprecision(3) << timestamp
        << " -i \"" << input << "\""
        << " -vframes 1"           // Single frame
        << " -q:v " << quality     // Quality (1-31, lower = better)
        << " -vf scale=320:-1"     // Scale to 320px width
        << " \"" << output << "\"";
    
    return cmd.str();
}

} // namespace lucid
```

---

## YouTube Integration (yt-dlp)

### Download and Metadata

```cpp
// src/lucid/ffmpeg/youtube_downloader.hpp
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <functional>

namespace lucid {

struct YouTubeVideo {
    std::string id;
    std::string title;
    std::string channel;
    std::string channel_id;
    std::string thumbnail_url;
    int64_t duration_sec;
    std::string description;
    std::string upload_date;  // YYYYMMDD
};

struct YouTubeDownloadResult {
    bool success;
    std::string video_path;
    std::string thumbnail_path;
    YouTubeVideo metadata;
    std::string error_message;
};

struct YouTubeChannel {
    std::string id;
    std::string name;
    std::string url;
    std::vector<std::string> video_ids;
};

class YouTubeDownloader {
public:
    YouTubeDownloader(const std::string& output_dir);
    ~YouTubeDownloader();
    
    // Download video
    YouTubeDownloadResult download(const std::string& url,
                                    std::function<void(double)> progress_callback = nullptr);
    
    // Get video metadata without downloading
    std::optional<YouTubeVideo> get_video_info(const std::string& url);
    
    // Get channel info
    std::optional<YouTubeChannel> get_channel_info(const std::string& channel_url);
    
    // Get new videos from channel
    std::vector<YouTubeVideo> get_channel_videos(const std::string& channel_url);
    
    // Check for new videos (for subscribed channels)
    std::vector<YouTubeVideo> check_for_new_videos(const std::string& channel_id,
                                                    const std::string& last_check_date);
    
private:
    std::string m_output_dir;
    
    std::string run_ytdlp(const std::vector<std::string>& args,
                          std::string& output,
                          std::string& error);
    
    YouTubeVideo parse_video_json(const std::string& json);
    YouTubeChannel parse_channel_json(const std::string& json);
};

} // namespace lucid
```

**File**: `src/lucid/ffmpeg/youtube_downloader.cpp`

```cpp
#include "youtube_downloader.hpp"
#include <sstream>
#include <iomanip>

namespace lucid {

YouTubeDownloader::YouTubeDownloader(const std::string& output_dir)
    : m_output_dir(output_dir) {
    
    // Ensure output directory exists
    Glib::Dir::make_dir(output_dir);
}

YouTubeDownloader::~YouTubeDownloader() = default;

YouTubeDownloadResult YouTubeDownloader::download(
    const std::string& url,
    std::function<void(double)> progress_callback) {
    
    YouTubeDownloadResult result;
    
    // Get metadata first
    auto info = get_video_info(url);
    if (!info) {
        result.success = false;
        result.error_message = "Failed to get video info";
        return result;
    }
    result.metadata = *info;
    
    // Sanitize filename
    std::string filename = sanitize_filename(info->title) + ".mp4";
    std::string output_path = Glib::build_filename(m_output_dir, filename);
    
    // Build yt-dlp command
    std::vector<std::string> args = {
        "--output", output_path,
        "--write-thumbnail",
        "--embed-thumbnail",
        "--format", "bestvideo[height<=1080]+bestaudio/best[height<=1080]",
        "--merge-output-format", "mp4",
        url
    };
    
    std::string stdout, stderr;
    std::string cmd = run_ytdlp(args, stdout, stderr);
    
    if (std::system(cmd.c_str()) != 0) {
        result.success = false;
        result.error_message = stderr;
        return result;
    }
    
    result.success = true;
    result.video_path = output_path;
    result.thumbnail_path = output_path + ".jpg";  // yt-dlp naming
    
    return result;
}

std::optional<YouTubeVideo> YouTubeDownloader::get_video_info(const std::string& url) {
    // Get JSON metadata without downloading
    std::vector<std::string> args = {
        "--dump-json",
        "--no-download",
        url
    };
    
    std::string stdout, stderr;
    std::string cmd = run_ytdlp(args, stdout, stderr);
    
    if (std::system(cmd.c_str()) != 0) {
        return std::nullopt;
    }
    
    return parse_video_json(stdout);
}

YouTubeVideo YouTubeDownloader::parse_video_json(const std::string& json) {
    // Parse yt-dlp JSON output
    // Use nlohmann/json or similar
    
    YouTubeVideo video;
    // Parse JSON and populate video struct
    // ...
    return video;
}

std::string YouTubeDownloader::run_ytdlp(
    const std::vector<std::string>& args,
    std::string& output,
    std::string& error) {
    
    std::ostringstream cmd;
    cmd << "yt-dlp";
    
    for (const auto& arg : args) {
        cmd << " " << arg;
    }
    
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) return "";
    
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    
    int result = pclose(pipe);
    
    // Note: In real implementation, handle stderr separately
    return cmd.str();
}

std::string sanitize_filename(const std::string& name) {
    std::string result = name;
    // Remove invalid characters
    for (char& c : result) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    return result;
}

} // namespace lucid
```

---

## FFmpeg Pipeline Integration

### Video Processing Service

```cpp
// src/lucid/ffmpeg/video_processor.hpp
#pragma once

#include "audio_extractor.hpp"
#include "thumbnail_generator.hpp"
#include "youtube_downloader.hpp"

namespace lucid {

class VideoProcessor {
public:
    VideoProcessor(const std::string& cache_dir);
    ~VideoProcessor();
    
    // Process video: extract audio, generate thumbnails
    void process_video(const std::string& video_path,
                       std::function<void(const std::string&)> status_callback,
                       std::function<void(double)> progress_callback);
    
    // Get extracted audio chunks
    std::vector<AudioChunk> get_audio_chunks(const std::string& video_id);
    
    // Get thumbnails
    std::vector<Thumbnail> get_thumbnails(const std::string& video_id);
    
    // Clean up temporary files for a video
    void cleanup(const std::string& video_id);
    
    // Download YouTube video
    YouTubeDownloadResult download_youtube(const std::string& url);

private:
    AudioExtractor m_audio_extractor;
    ThumbnailGenerator m_thumbnail_generator;
    YouTubeDownloader m_youtube_downloader;
    
    std::string m_cache_dir;
    
    std::string get_video_cache_dir(const std::string& video_id);
};

} // namespace lucid
```

---

## System Dependencies

```bash
# Arch Linux
sudo pacman -S ffmpeg yt-dlp
```

### CMake Integration

```cmake
# Find FFmpeg
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil)
pkg_check_modules(FFMPEGDEVICE REQUIRED libavdevice)

# Note: yt-dlp is a Python script, no CMake integration needed
# Just ensure it's installed on the system
```

---

## Error Handling

```cpp
// Common FFmpeg errors and handling
enum class FFmpegError {
    FILE_NOT_FOUND,
    INVALID_FORMAT,
    CODEC_NOT_FOUND,
    DECODE_ERROR,
    ENCODE_ERROR,
    TIMEOUT,
    DISK_FULL,
    UNKNOWN
};

FFmpegError parse_ffmpeg_error(const std::string& stderr) {
    if (stderr.find("No such file") != std::string::npos) {
        return FFmpegError::FILE_NOT_FOUND;
    }
    if (stderr.find("Invalid data found") != std::string::npos) {
        return FFmpegError::INVALID_FORMAT;
    }
    if (stderr.find("Unknown encoder") != std::string::npos ||
        stderr.find("codec not found") != std::string::npos) {
        return FFmpegError::CODEC_NOT_FOUND;
    }
    if (stderr.find("Error during decoding") != std::string::npos) {
        return FFmpegError::DECODE_ERROR;
    }
    if (stderr.find("No space left") != std::string::npos) {
        return FFmpegError::DISK_FULL;
    }
    return FFmpegError::UNKNOWN;
}
```

---

## Performance Tips

1. **Parallel Processing**: Extract audio chunks in parallel using thread pool
2. **Skip Transcoded Audio**: If video already has 16kHz mono stream, use `-codec copy`
3. **Hardware Acceleration**: Use VA-API or NVDEC for decoding
   ```bash
   ffmpeg -hwaccel vaapi -i input.mp4 ...
   ```
4. **Memory-Mapped I/O**: For large files, consider memory-mapped access

---

## API Documentation Links

| Resource | URL |
|----------|-----|
| FFmpeg | [ffmpeg.org/documentation](https://ffmpeg.org/documentation.html) |
| FFprobe | [ffmpeg.org/ffprobe.html](https://ffmpeg.org/ffprobe.html) |
| yt-dlp | [github.com/yt-dlp/yt-dlp](https://github.com/yt-dlp/yt-dlp) |
| FFmpeg Codecs | [ffmpeg.org/general.html#Audio-Codecs](https://ffmpeg.org/general.html#Audio-Codecs) |

---

*Document generated for Lucid project*