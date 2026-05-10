# LucidVideo - AI-Powered Video Library

## System Information
| Component | Value |
|-----------|-------|
| Kernel | Linux 7.0.3-6.stable |
| OS | Arch Linux x86_64 |
| Container | docker.io/library/archlinux:latest (podman) |
| Display | Wayland, GNOME |
| Architecture | x86_64 |
| Session | Local terminal (xterm-256color) |
| Date/Time | 2026-05-10 21:33:39 AEST |

## Project Info

### Overview
**LucidVideo** is an intelligent video library application with AI-powered search and semantic understanding. It is a GTK4-based desktop application built with C++20 that leverages AI models for video content analysis and natural language search.

### Build System
- **Build Tool**: CMake 3.20+
- **Languages**: C++20, C
- **Build Commands**:
  ```bash
  cmake -S . -B build
  cmake --build build -j$(nproc)
  cmake --build build --target test  # Run tests
  cmake --build build --target cppcheck  # Static analysis
  ```

### Project Structure
```
lucidvideo/
├── src/lucid/              # Main application source
│   ├── app.cpp/hpp         # Main application class (GTK::Application)
│   ├── buffer/             # Byte buffer utilities
│   ├── chat/               # Chat context handling
│   ├── control/            # HTTP control server API
│   ├── export/             # Transcription export (SRT/ VTT)
│   ├── ffmpeg/             # FFmpeg pipeline & thumbnails
│   ├── gui/                # GTK4 UI components
│   ├── inference/           # LLM engine & embeddings
│   ├── model/              # Database, video analysis, indexing
│   ├── search/             # Semantic search implementation
│   ├── settings/           # App configuration
│   ├── vision/             # Vision scanner (Qwen2-VL)
│   ├── workers/            # Background job & thread pool
│   └── utils/              # Hardware detection utilities
├── tests/                  # GoogleTest unit tests
├── third_party/            # Dependencies
│   ├── llama.cpp/          # LLM inference & embeddings
│   ├── whisper.cpp/        # Speech-to-text transcription
│   ├── ggml/               # ML tensor library
│   └── sqlite-vec/         # Vector database extension
├── resources/icons/        # App icons (16-512px PNG + SVG)
├── docs/                   # Documentation (markdown)
├── models/                 # AI model files (GGUF format)
├── build/                  # CMake build output
├── CMakeLists.txt          # Main build configuration
└── README.md               # Project documentation
```

### Entry Points
- **Main**: `src/main.cpp` - Creates LucidApp instance
- **App Class**: `src/lucid/app.cpp/hpp` - GTK4 Application with video library management

### Key Dependencies
| Dependency | Purpose | Header Location |
|------------|---------|-----------------|
| GTK4 | UI framework | `/usr/include/gtk-4.0` |
| libadwaita | GNOME design patterns | `/usr/include/libadwaita-1` |
| sqlite3 | Database | System |
| FFmpeg (libav*) | Video processing | `/usr/include/ffmpeg*` |
| curl | HTTP downloads | System |
| whisper.cpp | Speech-to-text | `third_party/whisper.cpp/include` |
| llama.cpp | LLM inference | `third_party/llama.cpp/include` |
| ggml | Tensor operations | `third_party/ggml/include` |

### Module Architecture

| Module | Description | Key Files |
|--------|-------------|-----------|
| **GUI** | GTK4 widgets & main window | `gui/main_window.cpp`, `gui/video_player_widget.cpp` |
| **Video Analysis** | AI-powered video analysis pipeline | `model/video_analyst.cpp`, `vision/vision_scanner.cpp` |
| **Search** | Semantic search with embeddings | `search/semantic_search.cpp`, `search/llm_query.cpp` |
| **Database** | SQLite + sqlite-vec for vectors | `model/database.cpp` |
| **Transcription** | Whisper-based speech-to-text | `model/transcription_engine.cpp` |
| **Inference** | LLM engine wrapper | `inference/llm_engine.cpp`, `inference/embedding_store.cpp` |
| **FFmpeg** | Video decoding & thumbnails | `ffmpeg/ffmpeg_pipeline.cpp`, `ffmpeg/thumbnail_generator.cpp` |
| **Settings** | App configuration persistence | `settings/app_settings.cpp` |

### Testing
- **Framework**: GoogleTest (gtest)
- **Test Executables**:
  - `lucid_tests` - Standalone utility tests
  - `lucid_buffer_tests` - ByteBuffer tests
  - `lucid_database_tests` - Database operations
  - `lucid_semantic_search_tests` - Search algorithms
  - `lucid_video_analyst_tests` - Video analysis
  - `lucid_transcription_exporter_tests` - Export formats
  - `lucid_chat_session_tests` - Chat message handling
  - `lucid_app_settings_tests` - Settings serialization
  - `lucid_vision_scanner_tests` - Vision model scanning
  - `lucid_ffmpeg_pipeline_tests` - FFmpeg pipeline
- **Run Tests**: `ctest --test-dir build` or `cd build && ctest`

### Build Options (CMake)
| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TESTS` | ON | Build unit tests |
| `ENABLE_VULKAN` | ON | GPU acceleration via Vulkan |
| `BUILD_MTMD` | ON | Build mtmd for vision support |
| `ANALYSIS_PIPELINE_DEBUG` | OFF | Debug output for video analysis |
| `ENABLE_CPPCHECK` | OFF | Run static analysis with build |

### Version
**0.4.1** (defined in CMakeLists.txt)

## Tasks

This section documents the project analysis completed. The following areas were examined:

1. **Project Identification** - C++20 GTK4 application with AI/ML capabilities
2. **Build System** - CMake with multiple test executables and static analysis
3. **Project Structure** - Modular design with GUI, Model, Search, and Inference layers
4. **Dependencies** - llama.cpp, whisper.cpp, FFmpeg, sqlite-vec
5. **Entry Points** - main.cpp -> LucidApp -> MainWindow
6. **Testing** - GoogleTest suite with 10+ test executables

## Todos

No tasks have been added yet. Use this section to track planned development work.