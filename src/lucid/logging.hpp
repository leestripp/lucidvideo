// logging.hpp - Lucid Logging System
//
// Structured logging with GLib integration

#pragma once

#include <glib.h>
#include <string_view>

namespace lucid
{

// ============================================================================
// Debug Configuration
// ============================================================================

// Set ANALYSIS_PIPELINE_DEBUG=ON in CMakeLists.txt to enable detailed debug output
// for the video analysis pipeline (thumbnail generation, transcription, vision scan)
#if !defined(ANALYSIS_PIPELINE_DEBUG)
    #define ANALYSIS_PIPELINE_DEBUG 0
#endif

// Log levels
enum class LogLevel {
    DEBUG = 0,
    INFO,
    WARNING,
    ERROR
};

// Log category
constexpr std::string_view LUCID_LOG_DOMAIN = "Lucid";

// Initialize logging system
void initialize_logging(LogLevel min_level = LogLevel::INFO);

// Get current log level
LogLevel get_log_level();

// Set log level
void set_log_level(LogLevel level);

// ============================================================================
// Macros for logging
// ============================================================================

// Format-based logging macros (using GLib's g_log)
#define LUCID_LOG(level, format, ...) \
    g_log(LUCID_LOG_DOMAIN.data(), G_LOG_LEVEL_##level, format, ##__VA_ARGS__)

// Debug logging
#define LUCID_DEBUG(format, ...) \
    LUCID_LOG(DEBUG, "[%s:%d] " format, __FILE__, __LINE__, ##__VA_ARGS__)

// Info logging
#define LUCID_INFO(format, ...) \
    LUCID_LOG(INFO, format, ##__VA_ARGS__)

// Warning logging
#define LUCID_WARN(format, ...) \
    LUCID_LOG(WARNING, format, ##__VA_ARGS__)

// Error logging
#define LUCID_ERROR(format, ...) \
    LUCID_LOG(ERROR, format, ##__VA_ARGS__)

// ============================================================================
// Analysis Pipeline Debug Macros
// ============================================================================

// Debug output for video analysis pipeline - only enabled when ANALYSIS_PIPELINE_DEBUG is set
#if ANALYSIS_PIPELINE_DEBUG

    // Thumbnail generation debug
    #define THUMB_DEBUG(fmt, ...) \
        LUCID_DEBUG("[THUMB] " fmt, ##__VA_ARGS__)

    // Transcription debug
    #define TRANSCRIBE_DEBUG(fmt, ...) \
        LUCID_DEBUG("[TRANSCRIBE] " fmt, ##__VA_ARGS__)

    // Vision scan debug
    #define VISION_DEBUG(fmt, ...) \
        LUCID_DEBUG("[VISION] " fmt, ##__VA_ARGS__)

    // Marker debug
    #define MARKER_DEBUG(fmt, ...) \
        LUCID_DEBUG("[MARKER] " fmt, ##__VA_ARGS__)

    // Pipeline phase debug
    #define PIPELINE_DEBUG(fmt, ...) \
        LUCID_DEBUG("[PIPELINE] " fmt, ##__VA_ARGS__)

#else // ANALYSIS_PIPELINE_DEBUG

    // No-op macros when debug is disabled
    #define THUMB_DEBUG(fmt, ...) do {} while(0)
    #define TRANSCRIBE_DEBUG(fmt, ...) do {} while(0)
    #define VISION_DEBUG(fmt, ...) do {} while(0)
    #define MARKER_DEBUG(fmt, ...) do {} while(0)
    #define PIPELINE_DEBUG(fmt, ...) do {} while(0)

#endif // ANALYSIS_PIPELINE_DEBUG

// ============================================================================
// Stream-based logging (for complex objects)
// ============================================================================

// Simple stream-based logger for complex object logging
class Logger {
public:
    Logger(LogLevel level) : m_level(level) {}
    
    template<typename T>
    Logger& operator<<(const T& value) {
        // Format to string and append
        // For simplicity, we use g_markup_printf_escaped
        return *this;
    }
    
    ~Logger() {
        // Flush to GLib log
    }
    
private:
    LogLevel m_level;
};

// Convenience macro for stream-based logging
#define LUCID_LOG_STREAM(level) \
    Logger(LogLevel::level)

} // namespace lucid

// Specializations for common types
#include <cstdint>
#include <string>
#include <vector>

namespace lucid::logging {

// Format helpers for common types
inline const char* format_timestamp(time_t t) {
    static char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&t));
    return buffer;
}

// Format size in human-readable form
inline const char* format_size(int64_t bytes) {
    static char buffer[64];
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_idx = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit_idx < 4) {
        size /= 1024.0;
        ++unit_idx;
    }
    
    snprintf(buffer, sizeof(buffer), "%.2f %s", size, units[unit_idx]);
    return buffer;
}

} // namespace lucid::logging