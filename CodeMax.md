# LucidVideo - Video Integrity Checker Implementation Plan

## System Information
| Component | Value |
|-----------|-------|
| Kernel | Linux 6.8.0 |
| OS | Arch Linux |
| Architecture | x86_64 |

## Project Overview
- **Build System**: CMake with C++20
- **Database**: SQLite3 with sqlite-vec
- **UI Framework**: GTK4/libadwaita

## Problem Statement
Videos that fail during analysis (due to transient errors, corrupted files, etc.) are left in FAILED state and never retried. We need a background checker that:
1. Periodically scans for failed videos
2. Re-queues them for retry (up to 3 attempts)
3. Marks them as BROKEN after 3 failures

---

## Implementation Plan

### Phase 1: Database Schema Updates

#### 1.1 Add BROKEN status to VideoStatus enum
**File**: `src/lucid/model/database.hpp`
```cpp
enum class VideoStatus {
    PENDING,
    ANALYZING,
    TRANSCRIBING,
    THUMBNAILING,
    VISION_SCANNING,
    COMPLETED,
    FAILED,
    STALE,
    BROKEN  // NEW: Failed more than 3 times
};
```

**File**: `src/lucid/model/database.cpp`
- Update `video_status_to_string()` to return "broken"
- Update `video_status_from_string()` to parse "broken"

#### 1.2 Add retry_count column to videos table
**File**: `src/lucid/model/database.cpp`
Add migration in `initialize_schema()`:
```cpp
// Migration: Add retry_count column to videos
const char* migrate_retry = R"(
    ALTER TABLE videos ADD COLUMN retry_count INTEGER DEFAULT 0;
)";
sqlite3_exec(m_db, migrate_retry, nullptr, nullptr, nullptr);
```

#### 1.3 Update VideoEntry struct
**File**: `src/lucid/model/database.hpp`
```cpp
struct VideoEntry {
    // ... existing fields ...
    std::string error_message;
    int retry_count = 0;  // NEW: Number of analysis retry attempts
    // ... rest of fields ...
};
```

#### 1.4 Add new database methods
**File**: `src/lucid/model/database.hpp`
```cpp
// Retry/broken video handling
void reset_failed_to_pending(int max_retries = 3);
void increment_retry_count(const std::string& id);
int get_retry_count(const std::string& id);
void mark_video_broken(const std::string& id);
```

---

### Phase 2: VideoIntegrityChecker Class

#### 2.1 Create header file
**File**: `src/lucid/model/video_integrity_checker.hpp`

```cpp
#pragma once

#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <condition_variable>

namespace lucid {

class Database;

class VideoIntegrityChecker {
public:
    using BrokenVideoCallback = std::function<void(const std::string& video_id, const std::string& error)>;
    
    VideoIntegrityChecker(Database& db, int check_interval_sec = 10);
    ~VideoIntegrityChecker();
    
    void start();
    void stop();
    
    void set_broken_video_callback(BrokenVideoCallback cb);
    
    int get_check_interval() const { return m_check_interval_sec; }
    void set_check_interval(int seconds) { m_check_interval_sec = seconds; }
    
    bool is_running() const { return m_running.load(); }

private:
    void worker_loop();
    void check_and_retry_failed_videos();

    Database& m_db;
    int m_check_interval_sec;
    
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stop_requested{false};
    std::thread m_worker_thread;
    
    std::mutex m_callback_mutex;
    BrokenVideoCallback m_broken_callback;
};

} // namespace lucid
```

#### 2.2 Create implementation file
**File**: `src/lucid/model/video_integrity_checker.cpp`

Key logic:
```cpp
void VideoIntegrityChecker::check_and_retry_failed_videos() {
    auto failed_videos = m_db.get_failed_videos();
    
    for (const auto& video : failed_videos) {
        int retry_count = m_db.get_retry_count(video.id);
        
        if (retry_count < 3) {
            // Increment retry count and reset to pending
            m_db.increment_retry_count(video.id);
            m_db.update_video_status(video.id, VideoStatus::PENDING);
            LUCID_INFO("VideoIntegrityChecker: Re-queuing %s (attempt %d/3)", 
                       video.title.c_str(), retry_count + 1);
        } else {
            // Mark as broken
            m_db.mark_video_broken(video.id);
            LUCID_WARN("VideoIntegrityChecker: Marking %s as BROKEN after 3 failed attempts", 
                       video.title.c_str());
            
            if (m_broken_callback) {
                m_broken_callback(video.id, video.error_message);
            }
        }
    }
}
```

---

### Phase 3: Integration

#### 3.1 Wire up in App
**File**: `src/lucid/app.cpp`

```cpp
// In App constructor or initialization
m_integrity_checker = std::make_unique<VideoIntegrityChecker>(*m_database, 10);
m_integrity_checker->set_broken_video_callback([this](const std::string& id, const std::string& err) {
    // Notify UI about broken video
    notify_broken_video(id, err);
});
m_integrity_checker->start();

// In shutdown
m_integrity_checker->stop();
```

#### 3.2 Add to CMakeLists.txt
**File**: `CMakeLists.txt`
```cmake
file(GLOB LUCID_SOURCES
    "src/*.cpp"
    "src/**/*.cpp"
    # Don't double-add video_integrity_checker.cpp
)
# Alternative: Add explicitly
set(LUCID_SOURCES
    ${LUCID_SOURCES}
    "src/lucid/model/video_integrity_checker.cpp"
)
```

---

### Phase 4: Testing

1. Create a test video that fails (simulate error)
2. Verify it goes from FAILED → PENDING → FAILED → PENDING → FAILED → BROKEN
3. Check logs show the retry attempts
4. Verify BROKEN videos don't get re-queued

---

## File Changes Summary

| File | Change Type |
|------|-------------|
| `src/lucid/model/database.hpp` | Add BROKEN status, retry_count field, new method signatures |
| `src/lucid/model/database.cpp` | Implement BROKEN status, migrations, new methods |
| `src/lucid/model/video_integrity_checker.hpp` | NEW - Header file |
| `src/lucid/model/video_integrity_checker.cpp` | NEW - Implementation |
| `src/lucid/app.cpp` | Integrate checker |
| `src/lucid/app.hpp` | Add member variable |
| `CMakeLists.txt` | Add new source file |

---

## Implementation Order

1. ✅ DONE - Update `database.hpp` - Add BROKEN status, retry_count field, method signatures
2. ✅ DONE - Update `database.cpp` - Implement status handling, migrations, methods
3. Create `video_integrity_checker.hpp` - Class header
4. Create `video_integrity_checker.cpp` - Class implementation
5. Update `app.cpp` / `app.hpp` - Wire up integration
6. Update `CMakeLists.txt` - Add source file
7. Build and test
