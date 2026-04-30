# Tests CMakeLists.txt
# Unit tests for Lucid application

include(CTest)

# Enable testing
enable_testing()

# ============================================================================
# Standalone Tests (no external dependencies)
# ============================================================================
add_executable(lucid_tests 
    standalone_tests.cpp
    model_downloader_tests.cpp
)

target_link_libraries(lucid_tests
    GTest::gtest
    GTest::gtest_main
    pthread
)

# ============================================================================
# Buffer Tests (needs ByteBuffer source)
# ============================================================================
add_executable(lucid_buffer_tests
    byte_buffer_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/lucid/buffer/byte_buffer.cpp
)

target_link_libraries(lucid_buffer_tests
    GTest::gtest
    GTest::gtest_main
    ${CURL_LIBRARIES}
    pthread
)

target_include_directories(lucid_buffer_tests PRIVATE
    ${CURL_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src
)

# ============================================================================
# Video Analyst Tests
# ============================================================================
add_executable(lucid_video_analyst_tests
    video_analyst_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/lucid/model/database.cpp
    ${CMAKE_SOURCE_DIR}/src/lucid/ffmpeg/thumbnail_generator.cpp
)

target_link_libraries(lucid_video_analyst_tests
    GTest::gtest
    GTest::gtest_main
    ${SQLITE3_LIBRARIES}
    ${GLIB_LIBRARIES}
    pthread
)

target_include_directories(lucid_video_analyst_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${SQLITE3_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/third_party/llama.cpp/include
    ${CMAKE_SOURCE_DIR}/third_party/llama.cpp/tools/mtmd
    ${CMAKE_SOURCE_DIR}/third_party/ggml/include
)

add_test(NAME lucid_tests COMMAND lucid_tests)
add_test(NAME lucid_buffer_tests COMMAND lucid_buffer_tests)
add_test(NAME lucid_video_analyst_tests COMMAND lucid_video_analyst_tests)

# ============================================================================
# Database Tests
# ============================================================================
add_executable(lucid_database_tests
    database_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/lucid/model/database.cpp
)

target_link_libraries(lucid_database_tests
    GTest::gtest
    GTest::gtest_main
    ${SQLITE3_LIBRARIES}
    ${GLIB_LIBRARIES}
    pthread
)

target_include_directories(lucid_database_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${SQLITE3_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
)

add_test(NAME lucid_database_tests COMMAND lucid_database_tests)

# ============================================================================
# Semantic Search Tests (pure math, no external dependencies)
# ============================================================================
add_executable(lucid_semantic_search_tests
    semantic_search_tests.cpp
)

target_link_libraries(lucid_semantic_search_tests
    GTest::gtest
    GTest::gtest_main
    pthread
)

add_test(NAME lucid_semantic_search_tests COMMAND lucid_semantic_search_tests)

# ============================================================================
# Transcription Exporter Tests
# ============================================================================
add_executable(lucid_transcription_exporter_tests
    transcription_exporter_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/lucid/export/transcription_exporter.cpp
)

target_link_libraries(lucid_transcription_exporter_tests
    GTest::gtest
    GTest::gtest_main
    pthread
)

target_include_directories(lucid_transcription_exporter_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME lucid_transcription_exporter_tests COMMAND lucid_transcription_exporter_tests)

# ============================================================================
# Chat Session Tests (message handling logic, standalone)
# ============================================================================
add_executable(lucid_chat_session_tests
    chat_session_tests.cpp
)

target_link_libraries(lucid_chat_session_tests
    GTest::gtest
    GTest::gtest_main
    pthread
)

target_include_directories(lucid_chat_session_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME lucid_chat_session_tests COMMAND lucid_chat_session_tests)

# ============================================================================
# FFmpeg Pipeline Integration Tests (ThumbnailGenerator only)
# ============================================================================
# Note: FFmpegPipeline requires giomm which needs special linking.
# We test ThumbnailGenerator instead which covers most FFmpeg functionality.

add_executable(lucid_ffmpeg_pipeline_tests
    ffmpeg_pipeline_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/lucid/ffmpeg/thumbnail_generator.cpp
)

target_link_libraries(lucid_ffmpeg_pipeline_tests
    GTest::gtest
    GTest::gtest_main
    pthread
)

target_include_directories(lucid_ffmpeg_pipeline_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${GLIB_INCLUDE_DIRS}
)

add_test(NAME lucid_ffmpeg_pipeline_tests COMMAND lucid_ffmpeg_pipeline_tests)

# ============================================================================
# Vision Scanner Tests
# Note: Full model tests require actual model files. These tests focus on
# structure validation, failure paths, and parse logic.
# ============================================================================
add_executable(lucid_vision_scanner_tests
    vision_scanner_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/lucid/vision/vision_scanner.cpp
    ${CMAKE_SOURCE_DIR}/src/lucid/utils/hardware.cpp
)

target_link_libraries(lucid_vision_scanner_tests
    GTest::gtest
    GTest::gtest_main
    ${GLIB_LIBRARIES}
    llama
    mtmd
    pthread
)

target_include_directories(lucid_vision_scanner_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/llama.cpp/include
    ${CMAKE_SOURCE_DIR}/third_party/llama.cpp/tools/mtmd
    ${CMAKE_SOURCE_DIR}/third_party/llama.cpp/ggml/include
    ${GLIB_INCLUDE_DIRS}
)

add_test(NAME lucid_vision_scanner_tests COMMAND lucid_vision_scanner_tests)

target_include_directories(lucid_vision_scanner_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/llama.cpp/include
    ${CMAKE_SOURCE_DIR}/third_party/llama.cpp/tools/mtmd
    ${CMAKE_SOURCE_DIR}/third_party/llama.cpp/ggml/include
    ${GLIB_INCLUDE_DIRS}
)

# ============================================================================
# AppSettings Tests
# ============================================================================
add_executable(lucid_app_settings_tests
    app_settings_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/lucid/settings/app_settings.cpp
)

target_link_libraries(lucid_app_settings_tests
    GTest::gtest
    GTest::gtest_main
    ${GLIB_LIBRARIES}
    ${GTKMM4_LIBRARIES}
    ${GIOUNIX_LIBRARIES}
    pthread
)

target_include_directories(lucid_app_settings_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${GTKMM4_INCLUDE_DIRS}
    ${GIOUNIX_INCLUDE_DIRS}
)

add_test(NAME lucid_app_settings_tests COMMAND lucid_app_settings_tests)
