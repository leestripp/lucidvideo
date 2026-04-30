# LucidVideo - Crash Fix Task List

## Project Info
- **Language**: C++20
- **Build System**: CMake
- **Framework**: GTK4, libadwaita
- **Architecture**: GTK Application with worker threads

## Crash Analysis

### Root Cause: Dangling References from Detached Threads

The application crashes because detached threads capture `this` via lambda and continue running after `MainWindow` destruction. When the window is destroyed:

1. **Lambda threads** access destructed `this` pointer
2. **Callback objects** reference destroyed UI components  
3. **Member unique_ptr** members are destroyed while threads still hold references

### Bug Locations Found:

1. **main_window.cpp:923** - Chat message LLM generation thread
2. **main_window.cpp:1329** - Folder scan thread  
3. **chat_session.cpp:142** - Async generation thread
4. **folder_browser.cpp:65** - Async folder selection thread
5. **model_manager.cpp:729** - Model download thread (different issue)

## Tasks

### TODO 1: Fix MainWindow Destructor - Add shutdown flag for detached threads
**File**: `src/lucid/gui/main_window.cpp` + `src/lucid/gui/main_window.hpp`

**Problem**: 
- Destructor is `= default;` with no cleanup
- Detached threads continue running during destruction
- Lambda threads capture `this` by value, accessing destructed members

**Fix**:
- Add `std::atomic<bool> m_shutdown{false}` to MainWindow
- Add destructor with cleanup that sets shutdown flag
- All detached threads must check shutdown flag before accessing members
- Add mutex-protected access for any shared state

### TODO 2: Fix Chat LLM Generation Thread - Race condition at line 923
**File**: `src/lucid/gui/main_window.cpp`

**Problem**: Lambda captures `this` by value, accessing `m_chat_panel` after potential destruction

**Fix**:
- Check `m_shutdown` flag before accessing `m_chat_panel`
- Use weak_ptr pattern or add destruction guards
- Ensure UI dispatch only if window still valid

### TODO 3: Fix Folder Scan Thread - Race condition at line 1329  
**File**: `src/lucid/gui/main_window.cpp`

**Problem**: Lambda captures `this` and accesses `m_status_label`, `m_home_view` after potential destruction

**Fix**:
- Check `m_shutdown` before accessing GTK widgets
- Use `g_idle_add` with guard for widget validity
- Only update UI if widget still exists

### TODO 4: Fix FolderBrowser Async Thread - FolderBrowser destruction
**File**: `src/lucid/gui/folder_browser.cpp`

**Problem**: `shared_ptr<AsyncData>` captured in lambda but `FolderBrowser` may be destroyed before callback completes

**Fix**:
- Add shutdown flag to FolderBrowser
- Ensure async_data mutex lock survives object destruction
- The current code uses `async_data.get()` not shared_ptr, which is correct - but needs shutdown guard

### TODO 5: Fix ChatSession Async Thread
**File**: `src/lucid/inference/chat_session.cpp`

**Problem**: Lambda captures `this` and accesses `m_engine`, `m_messages` after potential destruction

**Fix**:
- Add shutdown mechanism to ChatSession
- Guard all member access in lambda

### TODO 6: Fix VideoAnalysisWorker Callbacks - Cross-thread safety
**File**: `src/lucid/model/video_analysis_worker.cpp`

**Problem**: Worker calls UI callbacks from worker thread, not main thread

**Fix**:
- All UI callbacks MUST be dispatched to main thread via `g_idle_add` or `Glib::signal_idle()`
- Currently some callbacks might be called directly from worker thread

### TODO 7: Fix ModelManager Download Thread
**File**: `src/lucid/model/model_manager.cpp`

**Problem**: Download thread detached, captures `this` (singleton pattern issue)

**Fix**:
- ModelManager is a singleton, should be alive for app lifetime
- But callbacks might be invoked during shutdown
- Add proper shutdown sequence

### TODO 8: Verify Settings Page Thread Safety
**File**: `src/lucid/gui/settings_page.cpp`

**Problem**: Settings page has combo boxes, callbacks that might race

**Fix**: Check for similar detached thread patterns

### TODO 9: Build and Test - Verify no crashes
**Command**: 
```bash
rm -rf build && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
```

**Test**: Run app multiple times, check for crashes

## Implementation Order

1. First add shutdown flag and destructor to MainWindow
2. Fix all detached lambda threads to check shutdown flag
3. Ensure all UI updates are dispatched to main thread
4. Build and verify

## Files to Modify

1. `src/lucid/gui/main_window.hpp` - Add shutdown flag
2. `src/lucid/gui/main_window.cpp` - Fix destructor, all lambda threads
3. `src/lucid/inference/chat_session.cpp` - Add shutdown guard
4. `src/lucid/gui/folder_browser.cpp` - Add shutdown guard  
5. `src/lucid/model/video_analysis_worker.cpp` - Ensure main thread dispatch

## Notes

- All UI updates from background threads MUST go through `g_idle_add` or `Glib::signal_idle()`
- Detached threads should capture weak references or check shutdown before accessing members
- The `m_shutdown` flag pattern is simplest for this codebase