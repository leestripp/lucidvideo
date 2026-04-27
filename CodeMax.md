# LucidVideo - Intelligent Video Library Application

## System Information
| Component | Value |
|-----------|-------|
| Kernel | Linux 6.18.22 |
| OS | Arch Linux |
| Architecture | x86_64 |
| Container | docker.io/library/archlinux:latest (Podman) |
| Desktop | GNOME, Wayland |
| Session | local, uid 1000 |
| Date/Time | 2026-04-27 19:58:44 AEST |

## Project Info
- **Build System**: CMake 3.20+ with Ninja
- **Language**: C++20
- **Entry Point**: `src/main.cpp` → `LucidApp`
- **Binary Name**: `lucidvideo` (renamed from `lucid` in v0.4.1)
- **UI Framework**: GTK4 with Gtkmm-4.0, libadwaita-1
- **Database**: SQLite3 with sqlite-vec extension
- **Executable**: `./build/lucidvideo`
- **API Server**: HTTP control server on port 8765

## Directory Structure
```
.
├── src/
│   ├── main.cpp                     # Application entry point
│   └── lucid/
│       ├── app.cpp/hpp              # Main application (GTK Application, HTTP server)
│       ├── logging.cpp/hpp          # Logging utilities
│       ├── buffer/                  # Byte buffer utilities
│       ├── chat/                    # Chat context management
│       ├── control/                 # HTTP control server (port 8765)
│       ├── export/                  # Transcription export
│       ├── ffmpeg/                  # FFmpeg pipeline, thumbnail generation
│       ├── gui/                     # GTK UI widgets
│       │   ├── main_window.cpp/hpp  # Main application window
│       │   ├── settings_page.cpp/hpp # Settings UI
│       │   ├── video_list.cpp/hpp   # Video list view
│       │   ├── video_player_widget.cpp/hpp # Video playback
│       │   └── ...
│       ├── inference/               # LLM engine, embeddings, chat sessions
│       ├── model/                  # Core data models
│       │   ├── database.cpp/hpp     # SQLite database operations
│       │   ├── model_manager.cpp/hpp # Model download/management
│       │   ├── video_analyst.cpp/hpp # Video analysis orchestration
│       │   ├── video_analysis_worker.cpp/hpp # Background worker
│       │   └── ...
│       ├── search/                  # Semantic search, query parsing
│       ├── settings/                # AppSettings singleton
│       ├── vision/                  # Vision model (Qwen2-VL) scanning
│       └── workers/                 # Thread pool, background jobs
├── tests/                          # Unit tests (gtest)
├── third_party/                    # External dependencies
│   ├── llama.cpp                   # LLM inference
│   ├── whisper.cpp                 # Speech transcription
│   ├── ggml                        # Tensor computation
│   └── sqlite-vec                  # Vector search
├── docs/                           # Documentation
├── data/                           # Desktop entry, icons
├── resources/                      # App resources
├── scripts/                         # Build scripts
├── packaging/
│   ├── aur/                        # AUR package files
│   ├── appimage/                   # AppImage build script
│   └── flatpak/                    # Flatpak manifest
├── models/                         # Model cache directory
├── lucidvideo/                      # AUR package source (separate repo)
├── build/                          # Build output
│   └── lucidvideo                  # Compiled executable
├── CMakeLists.txt
├── README.md
├── PKGBUILD                        # AUR package (builds from source)
└── CodeMax.md                      # This file
```

## Build Commands
```bash
# Configure (from project root)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build with parallel jobs
cmake --build build -j$(nproc)

# Run the application
./build/lucidvideo

# Run tests
ctest --test-dir build

# Run cppcheck static analysis
cmake --build build --target cppcheck
```

## Key Components

### 1. Application Core (app.cpp)
- **LucidApp**: Main GTK4 application class extending `Gtk::Application`
- **Control Server**: HTTP API on port 8765 for external access/testing
- **Global g_app pointer**: For static callback access

### 2. Video Analysis Pipeline
- **VideoAnalysisWorker**: Background worker for video processing
- **VideoAnalyst**: Orchestrates analysis passes
- **Pass 1**: Thumbnail extraction, transcription
- **Pass 2**: Vision scanning (Qwen2-VL)
- **Adaptive Thumbnails**: Percentage-based distribution with logarithmic scaling

### 3. AI/ML Components
- **LLM Engine**: llama.cpp inference
- **Transcription Engine**: whisper.cpp
- **Vision Scanner**: Qwen2-VL 2B for image analysis
- **Embedding Store**: Semantic search with sqlite-vec
- **Semantic Search**: Natural language video search

### 4. UI Pages (MainWindow)
- **HOME**: Dashboard
- **VIDEO_LIST**: Browse videos
- **SEARCH**: Semantic search
- **VIDEO_DETAIL**: Single video view with player
- **ANALYSIS_QUEUE**: Processing queue
- **SETTINGS**: Configuration
- **MODEL_HUB**: Model management
- **CHAT**: AI chat interface

### 5. Settings System (AppSettings)
- **Singleton pattern** for global access
- **Signal-based** change notifications
- **Key settings**:
  - Whisper model selection
  - LLM/embedding models
  - GPU acceleration toggle
  - Thumbnail size
  - Vision scan interval
  - Watched folders

### 6. Database Schema
- **videos**: Video metadata, status, timestamps
- **markers**: Transcribed text + vision descriptions with timestamps
- **thumbnails**: Generated thumbnails
- **chat_messages**: Chat history
- **embeddings**: Vector embeddings for search

## Dependencies

### Runtime
- gtk4
- libadwaita
- sqlite3
- curl
- json-glib
- gst-libav
- gst-plugins-base
- gst-plugins-good

### Build
- cmake
- ninja
- gcc
- pkg-config

## Configuration
- **Config dir**: `~/.config/lucid/`
- **Data dir**: `~/.local/share/lucid/`
- **Database**: `~/.local/share/lucid/lucid.db`
- **Log file**: Application logs to stdout

## Testing Notes
- Test videos: `~/Videos/AI/` folder
- API endpoint: `http://127.0.0.1:8765`
- Control server starts automatically with app

## Git Workflow
- **Main development**: `devel` branch
- **Release**: `main` branch
- **Tags**: vX.Y.Z format for releases

## Documentation
See `docs/` directory:
- `lucid_api.md` - Control server API reference
- `configuration.md` - Configuration options
- `gtkmm4.md` - GTK4 UI patterns
- `video-analyst.md` - Video analysis pipeline
- `whisper.cpp.md` - Transcription documentation
- `llama.cpp.md` - LLM inference documentation
- `vision_scan.md` - Vision scanning documentation
- `testing.md` - Testing guide
- `packaging.md` - Packaging documentation
- `aur.md` - AUR package management

## Completed Features
- ✅ GTK4 migration (no deprecation warnings)
- ✅ llama.cpp/wisper.cpp API updates
- ✅ Adaptive thumbnail distribution
- ✅ Vision scan integration
- ✅ Semantic search with sqlite-vec
- ✅ HTTP control server API
- ✅ AUR package (lucidvideo)
- ✅ Build scripts for AppImage/Flatpak

## Tasks
<!-- Add tasks here as they are identified -->
