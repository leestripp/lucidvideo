# Lucid Testing Strategy

## Overview

This document outlines the testing approach for Lucid, including unit tests, integration tests, and CI/CD setup.

**Repository**: [github.com/ggml-org/lucid](https://github.com/ggml-org/lucid)

---

## Testing Framework

### GoogleTest (GTest)

We use GoogleTest for C++ unit testing:
- **Header-only**: Easy to integrate via submodule
- **Parameterized tests**: For testing multiple configurations
- **Mocking**: GoogleMock (GMock) for dependencies
- **Assertions**: Rich assertion types

### Why Not Catch2 or doctest?

| Framework | Pros | Cons |
|-----------|------|------|
| **GoogleTest** | Most widely used, excellent IDE integration | More complex setup |
| **Catch2** | Header-only, single header | Less IDE integration |
| **doctest** | Fast compile, header-only | Newer, less adoption |

**Decision**: GoogleTest - proven in production, great tooling support.

---

## Test Directory Structure

```
lucid/
├── tests/
│   ├── CMakeLists.txt
│   ├── unit/
│   │   ├── test_database.cpp
│   │   ├── test_model_downloader.cpp
│   │   ├── test_transcription_engine.cpp
│   │   ├── test_embedding_store.cpp
│   │   ├── test_audio_extractor.cpp
│   │   └── test_settings.cpp
│   ├── integration/
│   │   ├── test_video_processing.cpp
│   │   ├── test_search_pipeline.cpp
│   │   └── test_youtube_download.cpp
│   ├── fixtures/
│   │   ├── test_video.mp4           # Sample video for tests
│   │   ├── test_audio.wav            # Sample audio
│   │   └── test_data/
│   │       ├── sample.db
│   │       └── expected_results.json
│   └── helpers/
│       ├── temp_dir.hpp              # Temporary directory fixture
│       └── mock_whisper.hpp          # Mock whisper for testing
└── src/
    └── lucid/
        └── test/
            └── mocks/                 # Mock implementations
```

---

## CMake Test Setup

### tests/CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)
project(LucidTests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Test executable
add_executable(lucid_tests
    unit/test_database.cpp
    unit/test_model_downloader.cpp
    unit/test_transcription_engine.cpp
    unit/test_settings.cpp
    integration/test_video_processing.cpp
    helpers/temp_dir.hpp
)

target_include_directories(lucid_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
)

target_link_libraries(lucid_tests PRIVATE
    GTest::gtest_main
    GTest::gmock
    pthread
)

# Register tests with ctest
include(GoogleTest)
gtest_discover_tests(lucid_tests)

# Copy test fixtures
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/fixtures/)
```

---

## Unit Tests

### Test Database Module

**File**: `tests/unit/test_database.cpp`

```cpp
#include <gtest/gtest.h>
#include <database.hpp>
#include <helpers/temp_dir.hpp>

class DatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir = std::make_unique<TempDir>("lucid_test_");
        db_path = temp_dir->path() / "test.db";
        db = std::make_unique<Database>(db_path.string());
    }
    
    std::unique_ptr<TempDir> temp_dir;
    std::string db_path;
    std::unique_ptr<Database> db;
};

TEST_F(DatabaseTest, CreateVideo) {
    VideoEntry video;
    video.id = "test-uuid-123";
    video.path = "/path/to/video.mp4";
    video.title = "Test Video";
    video.duration = 120;
    video.source = "local";
    
    std::string id = db->add_video(video);
    
    EXPECT_EQ(id, "test-uuid-123");
    
    auto retrieved = db->get_video(id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->title, "Test Video");
    EXPECT_EQ(retrieved->duration, 120);
}

TEST_F(DatabaseTest, UpdateVideo) {
    // Add video
    VideoEntry video;
    video.id = "test-uuid-456";
    video.title = "Original Title";
    video.path = "/video.mp4";
    db->add_video(video);
    
    // Update
    video.title = "Updated Title";
    db->update_video(video);
    
    auto retrieved = db->get_video(video.id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->title, "Updated Title");
}

TEST_F(DatabaseTest, DeleteVideo) {
    VideoEntry video;
    video.id = "test-uuid-789";
    video.title = "To Delete";
    video.path = "/delete.mp4";
    db->add_video(video);
    
    db->delete_video(video.id);
    
    auto retrieved = db->get_video(video.id);
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(DatabaseTest, AddTranscription) {
    // Add video first
    VideoEntry video;
    video.id = "vid-123";
    video.title = "With Transcript";
    video.path = "/transcript.mp4";
    db->add_video(video);
    
    // Add transcription segments
    std::vector<TranscriptionSegment> segments = {
        {0, "vid-123", 0, 3000, "Hello world", 0.95f, std::nullopt},
        {0, "vid-123", 3000, 6000, "This is a test", 0.92f, std::nullopt}
    };
    
    db->add_transcription("vid-123", segments);
    
    // Verify
    auto retrieved = db->get_transcription("vid-123");
    EXPECT_EQ(retrieved.size(), 2);
    EXPECT_EQ(retrieved[0].text, "Hello world");
}

TEST_F(DatabaseTest, KeywordSearch) {
    // Setup
    VideoEntry video;
    video.id = "vid-search";
    video.title = "Search Test Video";
    video.path = "/search.mp4";
    db->add_video(video);
    
    TranscriptionSegment seg;
    seg.video_id = "vid-search";
    seg.start_ms = 0;
    seg.end_ms = 5000;
    seg.text = "The quick brown fox jumps";
    db->add_transcription("vid-search", {seg});
    
    // Search
    auto results = db->keyword_search("quick");
    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0].video_id, "vid-search");
}
```

### Test Model Downloader

**File**: `tests/unit/test_model_downloader.cpp`

```cpp
#include <gtest/gtest.h>
#include <model_downloader.hpp>

class ModelDownloaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir = std::make_unique<TempDir>("lucid_model_test_");
        downloader = std::make_unique<ModelDownloader>(temp_dir->path().string());
    }
    
    std::unique_ptr<TempDir> temp_dir;
    std::unique_ptr<ModelDownloader> downloader;
};

TEST_F(ModelDownloaderTest, GetAvailableModels) {
    auto models = downloader->get_available_models();
    
    ASSERT_FALSE(models.empty());
    
    // Check Whisper models
    auto whisper_models = filter_by_type(models, "whisper");
    ASSERT_FALSE(whisper_models.empty());
    
    // Check Llama models
    auto llama_models = filter_by_type(models, "llama");
    ASSERT_FALSE(llama_models.empty());
}

TEST_F(ModelDownloaderTest, ModelCatalog) {
    auto models = downloader->get_available_models();
    
    for (const auto& model : models) {
        EXPECT_FALSE(model.id.empty());
        EXPECT_FALSE(model.name.empty());
        EXPECT_FALSE(model.url.empty());
        EXPECT_GT(model.size_bytes, 0);
        
        // URL should be valid
        EXPECT_TRUE(model.url.starts_with("https://"));
    }
}

TEST_F(ModelDownloaderTest, IsDownloaded_NotDownloaded) {
    // No models downloaded yet
    EXPECT_FALSE(downloader->is_downloaded("whisper-tiny"));
    EXPECT_FALSE(downloader->is_downloaded("nomic-embed-text"));
}

TEST_F(ModelDownloaderTest, LocalPathGeneration) {
    auto path = downloader->get_local_path("whisper-tiny");
    ASSERT_TRUE(path.has_value());
    EXPECT_TRUE(path->find("ggml-tiny.bin") != std::string::npos);
}
```

### Test Fixtures

**File**: `tests/helpers/temp_dir.hpp`

```cpp
#pragma once

#include <filesystem>
#include <string>
#include <random>

class TempDir {
public:
    TempDir(const std::string& prefix = "test_") {
        // Create unique temporary directory
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(10000, 99999);
        
        auto temp = std::filesystem::temp_directory_path();
        m_path = temp / (prefix + std::to_string(dis(gen)));
        
        std::filesystem::create_directories(m_path);
    }
    
    ~TempDir() {
        // Clean up
        std::error_code ec;
        std::filesystem::remove_all(m_path, ec);
    }
    
    const std::filesystem::path& path() const { return m_path; }
    const std::string& string() const { return m_path.string(); }
    
    // Disable copy
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    
    // Allow move
    TempDir(TempDir&&) = default;
    TempDir& operator=(TempDir&&) = default;

private:
    std::filesystem::path m_path;
};
```

---

## Integration Tests

### Test Video Processing Pipeline

**File**: `tests/integration/test_video_processing.cpp`

```cpp
#include <gtest/gtest.h>
#include <video_processor.hpp>
#include <database.hpp>

class VideoProcessingIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir = std::make_unique<TempDir>("lucid_integration_");
        
        // Setup database
        auto db_path = temp_dir->path() / "test.db";
        db = std::make_unique<Database>(db_path.string());
        
        // Setup processor
        processor = std::make_unique<VideoProcessor>(
            (temp_dir->path() / "cache").string()
        );
    }
    
    std::unique_ptr<TempDir> temp_dir;
    std::unique_ptr<Database> db;
    std::unique_ptr<VideoProcessor> processor;
};

TEST_F(VideoProcessingIntegrationTest, ProcessSampleVideo) {
    std::string video_path = get_fixture_path("test_video.mp4");
    
    if (video_path.empty()) {
        GTEST_SKIP() << "Test video not available";
    }
    
    // Add to database
    VideoEntry video;
    video.id = "integration-test-1";
    video.path = video_path;
    video.title = "Integration Test Video";
    video.source = "local";
    db->add_video(video);
    
    // Process
    std::string last_status;
    processor->process_video(video_path,
        [&](const std::string& status) { last_status = status; },
        [](double progress) {}
    );
    
    // Verify database entries
    auto retrieved = db->get_video("integration-test-1");
    ASSERT_TRUE(retrieved.has_value());
    
    auto transcription = db->get_transcription("integration-test-1");
    // Transcription may or may not succeed depending on audio
    
    // Verify thumbnails exist
    auto thumbs_dir = temp_dir->path() / "thumbnails" / "integration-test-1";
    if (std::filesystem::exists(thumbs_dir)) {
        int thumb_count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(thumbs_dir)) {
            if (entry.is_regular_file()) {
                thumb_count++;
            }
        }
        EXPECT_GE(thumb_count, 3);
    }
}

std::string get_fixture_path(const std::string& name) {
    auto fixtures_dir = std::filesystem::path(__FILE__).parent_path() / "fixtures";
    auto path = fixtures_dir / name;
    
    if (std::filesystem::exists(path)) {
        return path.string();
    }
    return {};
}
```

---

## Running Tests

### Build and Run

```bash
# Configure with tests
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTS=ON

# Build
cmake --build build -j

# Run all tests
ctest --test-dir build

# Run specific test
./build/tests/lucid_tests --gtest_filter=DatabaseTest.CreateVideo

# Run with verbose output
./build/tests/lucid_tests --gtest_also_run_disabled_tests --gtest_print_time=1
```

### Test Filters

```bash
# Run only unit tests
./build/tests/lucid_tests --gtest_filter='*Test.*'

# Run only integration tests
./build/tests/lucid_tests --gtest_filter='*Integration*'

# Run specific pattern
./build/tests/lucid_tests --gtest_filter='Database*:-*Delete*'
```

---

## CI/CD Configuration

### GitHub Actions

**File**: `.github/workflows/test.yml`

```yaml
name: Tests

on:
  push:
    branches: [main, devel]
  pull_request:
    branches: [main]

jobs:
  test:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v4
      with:
        submodules: recursive
        
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y \
            cmake \
            ninja-build \
            libgtk-4-dev \
            libadwaita-1-dev \
            libsqlite3-dev \
            ffmpeg \
            yt-dlp
            
    - name: Configure
      run: cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTS=ON
      
    - name: Build
      run: cmake --build build -j$(nproc)
      
    - name: Run tests
      run: ctest --test-dir build --output-on-failure
      
    - name: Upload coverage
      if: matrix.coverage == true
      uses: codecov/codecov-action@v4
      with:
        files: build/coverage.info
```

### CMake Presets

**File**: `CMakePresets.json`

```json
{
    "version": 4,
    "configurePresets": [
        {
            "name": "dev",
            "binaryDir": "build",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Debug",
                "ENABLE_TESTS": "ON",
                "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
            }
        },
        {
            "name": "release",
            "binaryDir": "build-release",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Release"
            }
        },
        {
            "name": "coverage",
            "binaryDir": "build-coverage",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Debug",
                "ENABLE_COVERAGE": "ON",
                "ENABLE_TESTS": "ON"
            }
        }
    ]
}
```

---

## Coverage Report

### CMake Coverage Setup

```cmake
if(ENABLE_COVERAGE)
    include(CodeCoverage)
    
    append_coverage_compiler_flags()
    
    setup_target_for_coverage(
        NAME coverage
        EXECUTABLE lucid_tests
        DEPENDENCIES lucid
        FILTER "tests/*"
    )
endif()
```

### Run with Coverage

```bash
cmake --preset coverage
cmake --build build-coverage -j
ctest --test-dir build-coverage
gcovr --xml-pretty --output coverage.xml
```

---

## Test Data Management

### Test Fixtures

```
tests/fixtures/
├── videos/
│   ├── short_test.mp4      # 10 second test video
│   └── audio_only.wav       # Audio-only test
├── expected/
│   ├── transcription.json   # Expected Whisper output
│   └── embeddings.json      # Expected embedding values
└── models/
    └── tiny-model.gguf      # Tiny model for fast tests
```

### Downloading Test Fixtures

```bash
# In tests/ directory
./download_fixtures.sh

# Or via CMake
include(FetchContent)
FetchContent_Declare(
    test_fixtures
    URL "https://github.com/lucid/test-fixtures/releases/download/v1.0/fixtures.tar.gz"
)
FetchContent_MakeAvailable(test_fixtures)
```

---

## Mocking External Dependencies

### Mock whisper.cpp

**File**: `src/lucid/test/mocks/mock_whisper.hpp`

```cpp
#pragma once

#include <whisper.h>

// Mock whisper context for testing
struct MockWhisperContext {
    std::vector<std::string> segments;
    std::string detected_lang = "en";
    bool should_fail = false;
};

whisper_context* mock_whisper_init_from_file(const char* path) {
    static int call_count = 0;
    call_count++;
    
    if (call_count > 100) {
        // Simulate memory exhaustion after many loads
        return nullptr;
    }
    
    return reinterpret_cast<whisper_context*>(new MockWhisperContext());
}

int mock_whisper_full(whisper_context* ctx, 
                      whisper_full_params params,
                      const float* samples,
                      int n_samples) {
    auto* mock = reinterpret_cast<MockWhisperContext*>(ctx);
    
    if (mock->should_fail) {
        return -1;
    }
    
    // Return mock segments
    return 0;
}

// Override at link time
#ifdef MOCK_WHISPER
#define whisper_init_from_file mock_whisper_init_from_file
#define whisper_full mock_whisper_full
#endif
```

---

## Performance Testing

```cpp
TEST(PerformanceTest, TranscriptionSpeed) {
    auto start = std::chrono::high_resolution_clock::now();
    
    TranscriptionEngine engine(MODEL_PATH);
    auto result = engine.transcribe(TEST_AUDIO);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should transcribe 1 minute of audio in under 30 seconds (with tiny model)
    EXPECT_LT(duration.count(), 30000);
    
    // Report RTF (Real-Time Factor)
    double rtf = duration.count() / 1000.0 / 60.0;  // For 1-minute audio
    SPDLOG_INFO("Transcription RTF: {:.2f}x", 1.0 / rtf);
}
```

---

## API Documentation Links

| Resource | URL |
|----------|-----|
| GoogleTest | [github.com/google/googletest](https://github.com/google/googletest) |
| GoogleMock | [github.com/google/googletest/tree/main/googlemock](https://github.com/google/googletest/tree/main/googlemock) |
| CMake Testing | [cmake.org/cmake/help/latest/module/CTest.html](https://cmake.org/cmake/help/latest/module/CTest.html) |
| CodeCoverage | [github.com/bilke/cmake-modules](https://github.com/bilke/cmake-modules#code-coverage) |

---

*Document generated for Lucid project*