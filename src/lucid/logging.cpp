// logging.cpp - Lucid Logging Implementation
//
// Structured logging with GLib integration

#include "logging.hpp"

#include <glib.h>
#include <glibmm.h>
#include <ctime>
#include <cstdio>
#include <chrono>

namespace lucid {

// Current log level
static LogLevel s_min_level = LogLevel::INFO;

// GLib log handler
static void glib_log_handler(const gchar* log_domain,
                              GLogLevelFlags log_level,
                              const gchar* message,
                              gpointer /* user_data */) {
    // Filter by level
    LogLevel level;
    switch (log_level & G_LOG_LEVEL_MASK) {
        case G_LOG_LEVEL_DEBUG: level = LogLevel::DEBUG; break;
        case G_LOG_LEVEL_INFO:  level = LogLevel::INFO; break;
        case G_LOG_LEVEL_WARNING: level = LogLevel::WARNING; break;
        case G_LOG_LEVEL_ERROR:
        case G_LOG_LEVEL_CRITICAL: level = LogLevel::ERROR; break;
        default: level = LogLevel::INFO; break;
    }
    
    if (level < s_min_level) {
        return;
    }
    
    // Output to stderr with timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    // Format timestamp
    char time_buffer[64];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", localtime(&time_t_now));
    
    fprintf(stderr, "[%s] %s: %s\n",
            time_buffer,
            log_domain,
            message);
}

void initialize_logging(LogLevel min_level) {
    s_min_level = min_level;
    
    // Install GLib log handler
    g_log_set_handler(LUCID_LOG_DOMAIN.data(),
                      static_cast<GLogLevelFlags>(G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION),
                      glib_log_handler,
                      nullptr);
    
    g_message("Logging initialized at level %d", static_cast<int>(min_level));
}

LogLevel get_log_level() {
    return s_min_level;
}

void set_log_level(LogLevel level) {
    s_min_level = level;
}

} // namespace lucid