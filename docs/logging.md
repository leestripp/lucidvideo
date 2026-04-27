# Lucid Logging and Error Handling

## Overview

Lucid uses a centralized logging system based on GLib's logging infrastructure with additional spdlog integration for performance-critical components. This ensures consistency with GNOME conventions while providing modern C++ features.

**Repository**: [github.com/ggml-org/lucid](https://github.com/ggml-org/lucid)

---

## Logging Strategy

### Why GLib Logging + spdlog?

| Approach | Pros | Cons |
|----------|------|------|
| **GLib (g_warning, g_error)** | GNOME standard, journald integration, IDE support | Less flexible formatting |
| **spdlog** | Fast, async, rich features | External dependency |
| **Both combined** | Best of both worlds | More complex setup |

**Decision**: Use GLib logging for GTK/GLib components, spdlog for inference-heavy operations (whisper.cpp, llama.cpp).

### Log Levels

| Level | GLib | spdlog | Use Case |
|-------|------|--------|----------|
| **DEBUG** | `g_debug()` | `spdlog::debug()` | Development, verbose info |
| **INFO** | `g_info()` | `spdlog::info()` | Normal operation |
| **WARNING** | `g_warning()` | `spdlog::warn()` | Recoverable issues |
| **ERROR** | `g_error()` | `spdlog::error()` | Operation failed |
| **CRITICAL** | `g_critical()` | `spdlog::critical()` | Fatal, crash likely |

### Log Destinations

```
~/.local/share/lucid/logs/
├── lucid.log          # Current session
├── lucid-2026-04-19.log  # Rotated daily
└── errors.log         # Errors only
```

---

## Logger Implementation

### File: `src/lucid/utils/logger.hpp`

```cpp
#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>
#include <string>

namespace lucid {

class Logger {
public:
    static Logger& instance();
    
    // Initialize logger with log directory
    void initialize(const std::string& log_dir = get_default_log_dir());
    
    // Get spdlog logger for inference components
    std::shared_ptr<spdlog::logger> get_inference_logger();
    
    // Get name for specific component
    std::shared_ptr<spdlog::logger> get_logger(const std::string& name);
    
    // Set log level
    void set_level(spdlog::level::level_enum level);
    
    // Flush all logs
    void flush();
    
    // Default log directory
    static std::string get_default_log_dir();

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    std::shared_ptr<spdlog::logger> m_main_logger;
    std::shared_ptr<spdlog::logger> m_inference_logger;
};

} // namespace lucid
```

### File: `src/lucid/utils/logger.cpp`

```cpp
#include "logger.hpp"
#include <glib.h>
#include <glibmm.h>
#include <filesystem>

namespace lucid {

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

std::string Logger::get_default_log_dir() {
    return Glib::build_filename(
        Glib::get_user_data_dir(),
        "lucid",
        "logs"
    );
}

void Logger::initialize(const std::string& log_dir) {
    // Create log directory
    std::filesystem::create_directories(log_dir);
    
    // Create rotating file sink (10MB max, 5 files)
    auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        (std::filesystem::path(log_dir) / "lucid.log").string(),
        1024 * 1024 * 10,  // 10MB
        5
    );
    
    // Create stdout sink for debugging
    auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    
    // Format: [2026-04-19 12:30:45.123] [INFO] [component] message
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");
    
    // Main logger (file + console)
    m_main_logger = std::make_shared<spdlog::logger>("main", 
        spdlog::sinks_init_list{rotating_sink, stdout_sink});
    spdlog::register_logger(m_main_logger);
    
    // Inference logger (separate file for whisper/llama logs)
    auto inference_file = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        (std::filesystem::path(log_dir) / "inference.log").string(),
        1024 * 1024 * 10,
        3
    );
    m_inference_logger = std::make_shared<spdlog::logger>("inference", inference_file);
    spdlog::register_logger(m_inference_logger);
    
    SPDLOG_LOGGER_INFO(m_main_logger, "Logger initialized at {}", log_dir);
}

std::shared_ptr<spdlog::logger> Logger::get_inference_logger() {
    return m_inference_logger;
}

std::shared_ptr<spdlog::logger> Logger::get_logger(const std::string& name) {
    return spdlog::get(name);
}

void Logger::set_level(spdlog::level::level_enum level) {
    spdlog::set_level(level);
    if (m_main_logger) m_main_logger->set_level(level);
    if (m_inference_logger) m_inference_logger->set_level(level);
}

void Logger::flush() {
    spdlog::flush_all();
}

} // namespace lucid
```

---

## GLib Logging Integration

### File: `src/lucid/utils/glib_log_handler.hpp`

```cpp
#pragma once

#include <glib.h>
#include <spdlog/spdlog.h>

// Redirect GLib logging to spdlog
void setup_glib_log_handler() {
    // Set custom log handler
    g_set_print_handler([](const gchar* message) {
        spdlog::info("[GLib] {}", message);
    });
    
    g_set_printerr_handler([](const gchar* message) {
        spdlog::error("[GLib] {}", message);
    });
}

// Example: Using GLib log levels in C++
// g_debug("Video indexed: %s", path.c_str());
// g_info("Download complete: %s", title.c_str());
// g_warning("Retrying: %s (attempt %d)", url.c_str(), attempt);
// g_error("Failed to load model: %s", path.c_str());
// g_critical("Database corruption detected!");
```

---

## Error Handling Patterns

### 1. Result Type for Operations

```cpp
// File: src/lucid/utils/result.hpp
#pragma once

#include <expected>
#include <string>

namespace lucid {

template<typename T>
using Result = std::expected<T, std::string>;

template<>
using Result<void> = std::expected<std::monostate, std::string>;

} // namespace lucid

// Usage:
lucid::Result<VideoEntry> load_video(const std::string& path) {
    if (!file_exists(path)) {
        return std::unexpected("File not found: " + path);
    }
    return VideoEntry{};
}
```

### 2. Exception Handling for Non-Critical Operations

```cpp
// Catch and log, don't throw
void process_video(const std::string& path) {
    try {
        transcribe_video(path);
    } catch (const std::exception& e) {
        g_warning("Transcription failed for %s: %s", path.c_str(), e.what());
        // Continue with next video, don't crash
    }
}
```

### 3. Cancellable Operations

```cpp
// File: src/lucid/utils/cancellable_task.hpp
#pragma once

#include <atomic>
#include <functional>

namespace lucid {

class CancellableTask {
public:
    using TaskFunc = std::function<bool()>;  // Return false to cancel
    
    CancellableTask(TaskFunc func) : m_func(std::move(func)) {}
    
    void cancel() { m_cancelled.store(true); }
    bool is_cancelled() const { return m_cancelled.load(); }
    
    bool execute() {
        return m_func();
    }

private:
    TaskFunc m_func;
    std::atomic<bool> m_cancelled{false};
};

} // namespace lucid

// Usage:
void transcribe_with_cancel(const std::string& path) {
    CancellableTask task([]() -> bool {
        for (int i = 0; i < 100; ++i) {
            if (task.is_cancelled()) {
                return false;  // Stop processing
            }
            process_chunk(i);
        }
        return true;  // Completed
    });
    
    // Run in thread
    std::thread([&task]() { task.execute(); }).detach();
    
    // Cancel after timeout
    std::this_thread::sleep_for(30s);
    task.cancel();
}
```

---

## Crash Handling

### 1. SIGSEGV Handler

```cpp
// File: src/lucid/utils/crash_handler.cpp
#include <csignal>
#include <cstdlib>
#include <execinfo.h>
#include <filesystem>
#include <fstream>

void setup_crash_handler() {
    std::signal(SIGSEGV, [](int sig) {
        // Log backtrace
        void* buffer[32];
        int n = backtrace(buffer, 32);
        char** strings = backtrace_symbols(buffer, n);
        
        std::ofstream crash_log(
            std::filesystem::path(lucid::Logger::get_default_log_dir()) 
                / "crash.log"
        );
        
        crash_log << "Crash: " << strsignal(sig) << "\n";
        for (int i = 0; i < n; ++i) {
            crash_log << "  " << strings[i] << "\n";
        }
        
        free(strings);
        
        // Exit with crash code
        std::_Exit(128 + sig);
    });
    
    // Also handle SIGABRT
    std::signal(SIGABRT, [](int sig) {
        // Similar handling
        std::_Exit(128 + sig);
    });
}
```

### 2. GTK Critical Logs

```cpp
// Catch GTK warnings without crashing
g_log_set_fatal_mask("Gtk", G_LOG_LEVEL_WARNING);
g_log_set_fatal_mask("Gdk", G_LOG_LEVEL_WARNING);

// Or make specific warnings non-fatal
g_log_set_always_fatal(G_LOG_LEVEL_ERROR | G_LOG_FLAG_FATAL);
```

---

## Logging Best Practices

### Do's and Don'ts

**DO:**
```cpp
// Use component prefix in logs
g_info("TranscriptionEngine: Processing %s", path.c_str());
SPDLOG_INFO(logger, "WhisperEngine: Model loaded in {:.2f}s", load_time);

// Include context
g_warning("ModelDownloader: Download failed for %s, retry %d/%d", 
          model_id.c_str(), attempt, max_attempts);

// Log before operation
g_debug("Database: Opening %s", db_path.c_str());

// Log after operation
g_info("VideoIndexer: Indexed %d videos", count);
```

**DON'T:**
```cpp
// Don't log sensitive info
// BAD: g_info("User: %s, Password: %s", user, password);

// Don't log too verbosely in hot paths
// BAD: g_debug("Processing chunk %d", i); // Called millions of times

// Don't forget to flush critical logs
// GOOD: if (critical_operation) logger->flush();
```

### Performance Considerations

```cpp
// Use lazy evaluation for expensive strings
// BAD: g_debug("Vector size: " + std::to_string(vec.size()));  // Always formats

// GOOD: g_debug("Vector size: %zu", vec.size());  // Only formats if enabled

// Disable debug logging in release builds
#ifndef NDEBUG
    spdlog::set_level(spdlog::level::debug);
#else
    spdlog::set_level(spdlog::level::info);
#endif
```

---

## Log File Management

### Rotation Policy

- **Daily rotation**: New file each day
- **Size limit**: 10MB per file, keep 7 days
- **Compression**: Compress old logs with gzip
- **Cleanup**: Remove logs older than 30 days

```cpp
// File: src/lucid/utils/log_manager.hpp
class LogManager {
public:
    static void rotate_if_needed();
    static void cleanup_old_logs(int days_to_keep = 30);
    static void compress_old_logs();
};
```

```cpp
void LogManager::cleanup_old_logs(int days_to_keep) {
    auto log_dir = Logger::get_default_log_dir();
    auto now = std::chrono::system_clock::now();
    
    for (const auto& entry : std::filesystem::directory_iterator(log_dir)) {
        if (entry.is_regular_file()) {
            auto age = now - std::filesystem::last_write_time(entry);
            auto days = std::chrono::duration_cast<std::chrono::days>(age).count();
            
            if (days > days_to_keep) {
                std::filesystem::remove(entry.path());
                SPDLOG_INFO("Cleaned up old log: {}", entry.path().string());
            }
        }
    }
}
```

---

## Log Levels by Component

| Component | Default Level | Hot Path |
|-----------|--------------|----------|
| **App** | INFO | Startup, shutdown |
| **Database** | WARNING | All queries |
| **VideoIndexer** | INFO | File events |
| **TranscriptionEngine** | INFO | Chunk boundaries |
| **WhisperEngine** | DEBUG | Token processing |
| **LlamaEngine** | DEBUG | Token generation |
| **ModelDownloader** | INFO | Download progress |
| **GUI** | WARNING | User interactions |

---

## Monitoring

### systemd Journal Integration

Lucid logs to journald via GLib's default handler:

```bash
# View Lucid logs
journalctl -u lucid.service -f

# View only errors
journalctl -u lucid.service PRIORITY=3

# Filter by component
journalctl -u lucid.service SYSLOG_IDENTIFIER=lucid
```

### Startup Verification

```cpp
void verify_logging() {
    SPDLOG_INFO("Logger test");
    g_info("GLib handler test");
    
    // Verify file was created
    auto log_path = std::filesystem::path(Logger::get_default_log_dir()) / "lucid.log";
    if (!std::filesystem::exists(log_path)) {
        g_critical("Failed to create log file!");
    }
}
```

---

## Dependencies

```cmake
# CMakeLists.txt - spdlog for inference components
find_package(spdlog REQUIRED)
target_link_libraries(lucid PRIVATE spdlog::spdlog)
```

---

## API Documentation Links

| Resource | URL |
|----------|-----|
| spdlog GitHub | [github.com/gabime/spdlog](https://github.com/gabime/spdlog) |
| GLib Logging | [docs.gtk.org/glib/glib-Message-Logging.html](https://docs.gtk.org/glib/glib-Message-Logging.html) |
| systemd Journal | [man7.org/linux/man-pages/man7/systemd.journal-fields.7.html](https://man7.org/linux/man-pages/man7/systemd.journal-fields.7.html) |

---

*Document generated for Lucid project*