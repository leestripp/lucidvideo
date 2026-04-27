cmake_minimum_required(VERSION 3.20)
project(Lucid VERSION 0.4.0 LANGUAGES C CXX)

# ============================================================================
# Force static libraries for all third-party deps (no LD_LIBRARY_PATH needed)
# ============================================================================
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries")
set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# ============================================================================
# Compiler Setup - C++20 with native optimization
# ============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Native optimization for current architecture
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -mtune=native")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
# Warning flags - be strict but allow common patterns
add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
    -Wno-conversion
)

# ============================================================================
# Options
# ============================================================================
option(BUILD_TESTS "Build unit tests" ON)
option(ENABLE_VULKAN "Enable Vulkan GPU acceleration (requires Vulkan SDK)" ON)
option(BUILD_MTMD "Build mtmd library for vision support" ON)
option(ANALYSIS_PIPELINE_DEBUG "Enable detailed debug output for video analysis pipeline" OFF)
option(ENABLE_CPPCHECK "Run cppcheck static analysis as part of the build" OFF)

# ============================================================================
# Dependencies
# ============================================================================
find_package(PkgConfig REQUIRED)

# GTK4 and UI libraries
pkg_check_modules(GTK4 REQUIRED gtk4)
pkg_check_modules(GTKMM4 REQUIRED gtkmm-4.0)
pkg_check_modules(ADWAITA REQUIRED libadwaita-1)
pkg_check_modules(GIOUNIX REQUIRED gio-unix-2.0)

# Database and utilities
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
pkg_check_modules(CURL REQUIRED libcurl)

# FFmpeg for video processing
find_path(FFMPEG_INCLUDE_DIR libavformat/avformat.h REQUIRED)
set(FFMPEG_INCLUDE_DIRS ${FFMPEG_INCLUDE_DIR})
set(FFMPEG_LIBRARIES avformat avcodec avutil swscale)

# GStreamer for video playback
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMER_VIDEO REQUIRED gstreamer-video-1.0)
pkg_check_modules(GLIB glib-2.0)

# GoogleTest for unit tests
if(BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# ============================================================================
# Source Files
# ============================================================================
file(GLOB_RECURSE LUCID_SOURCES
    "src/*.cpp"
    "src/*.c"
    "src/**/*.cpp"
    "src/**/*.c"
)

# Create an object library for shared sources (for tests and executable)
add_library(lucid_objects OBJECT ${LUCID_SOURCES})
target_include_directories(lucid_objects PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# Include directories for objects
target_include_directories(lucid_objects PRIVATE
    ${GTK4_INCLUDE_DIRS}
    ${GTKMM4_INCLUDE_DIRS}
    ${ADWAITA_INCLUDE_DIRS}
    ${GIOUNIX_INCLUDE_DIRS}
    ${SQLITE3_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_VIDEO_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    # Llama.cpp includes for mtmd/vision support
    ${CMAKE_SOURCE_DIR}/third_party/llama.cpp/include
    ${CMAKE_SOURCE_DIR}/third_party/llama.cpp/tools/mtmd
    ${CMAKE_SOURCE_DIR}/third_party/ggml/include
)

# Create static library from objects (for tests)
add_library(lucid_core STATIC $<TARGET_OBJECTS:lucid_objects>)
target_link_libraries(lucid_core PRIVATE
    ${GTK4_LIBRARIES}
    ${GTKMM4_LIBRARIES}
    ${ADWAITA_LIBRARIES}
    ${GIOUNIX_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    ${CURL_LIBRARIES}
    ${FFMPEG_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_VIDEO_LIBRARIES}
    pthread
    m
    dl
    jsoncpp
)

# ============================================================================
# Whisper.cpp for transcription
# ============================================================================
set(WHISPER_USE_SYSTEM_GGML OFF CACHE BOOL "Use system GGML")
set(GGML_BUILD_TESTS OFF CACHE BOOL "Build GGML tests")
set(GGML_BUILD_EXAMPLES OFF CACHE BOOL "Build GGML examples")
# Enable Vulkan for GPU acceleration
if(ENABLE_VULKAN)
    set(GGML_VULKAN ON CACHE BOOL "Enable Vulkan support" FORCE)
    message(STATUS "Vulkan GPU acceleration enabled")
endif()
add_subdirectory(third_party/whisper.cpp)

# Add whisper and ggml include paths and define
target_include_directories(lucid_objects PRIVATE 
    ${CMAKE_SOURCE_DIR}/third_party/whisper.cpp/include
    ${CMAKE_SOURCE_DIR}/third_party/ggml/include
)
target_compile_definitions(lucid_objects PRIVATE
    LUCID_HAVE_WHISPER=1
)

# Link whisper library to lucid_core (built before executable)
target_link_libraries(lucid_core PRIVATE whisper ggml jsoncpp)

# Llama.cpp for embeddings and vision support
# ============================================================================
# Enable tools (includes mtmd) before adding the subdirectory
set(LLAMA_BUILD_TOOLS ON CACHE BOOL "" FORCE)
set(LLAMA_BUILD_COMMON ON CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

if (NOT TARGET llama)
    add_subdirectory(third_party/llama.cpp)
    
    # Add llama include paths
    target_include_directories(lucid_objects PRIVATE 
        ${CMAKE_SOURCE_DIR}/third_party/llama.cpp/include
    )
    
    # Define LUCID_HAVE_LLAMA to enable embedding code
    target_compile_definitions(lucid_objects PRIVATE
        LUCID_HAVE_LLAMA=1
    )
    
    # Define ANALYSIS_PIPELINE_DEBUG when enabled (for video analysis debug output)
    if(ANALYSIS_PIPELINE_DEBUG)
        target_compile_definitions(lucid_objects PRIVATE ANALYSIS_PIPELINE_DEBUG=1)
        message(STATUS "Video analysis pipeline debug output ENABLED")
    endif()
    
    # Link llama library (shares ggml with whisper)
    target_link_libraries(lucid_core PRIVATE llama)
endif()

# Link mtmd library for vision support (Qwen2-VL)
if(TARGET mtmd)
    target_link_libraries(lucid_core PRIVATE mtmd)
    target_link_libraries(lucid_objects PRIVATE mtmd)
    message(STATUS "MTMD library linked for vision support")
endif()

# Create the main executable
add_executable(lucidvideo $<TARGET_OBJECTS:lucid_objects>)
target_link_libraries(lucidvideo PRIVATE
    ${GTK4_LIBRARIES}
    ${GTKMM4_LIBRARIES}
    ${ADWAITA_LIBRARIES}
    ${GIOUNIX_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    ${CURL_LIBRARIES}
    ${FFMPEG_LIBRARIES}
    whisper
    llama
    ggml
    mtmd
    pthread
    m
    dl
    jsoncpp
)

# Compile options for lucidvideo executable
target_compile_options(lucidvideo PRIVATE
    $<$<CONFIG:Release>:-O3>
)

# ============================================================================
# Installation
# ============================================================================
install(TARGETS lucidvideo lucid_core
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

# ============================================================================
# Testing
# ============================================================================
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Build Info
# ============================================================================
message(STATUS "===========================================")
message(STATUS "  Lucid Build Configuration")
message(STATUS "===========================================")
message(STATUS "  Version:        ${PROJECT_VERSION}")
message(STATUS "  Build Type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "  Vulkan:         ${ENABLE_VULKAN}")
message(STATUS "  Tests:          ${BUILD_TESTS}")
message(STATUS "===========================================")

# ============================================================================
# Icon Installation (hicolor theme for GTK compatibility)
# ============================================================================
set(ICON_SIZES 16 24 32 48 64 128 256 512)

foreach(size IN LISTS ICON_SIZES)
    install(FILES resources/icons/lucid-${size}.png
            DESTINATION share/icons/hicolor/${size}x${size}/apps
            RENAME lucid.png
    )
endforeach()

# Also install the SVG source
install(FILES resources/icons/lucid-app.svg
        DESTINATION share/icons/hicolor/scalable/apps
        RENAME lucid.svg
)

# Install index.theme for hicolor (optional but helps GTK discover icons)
install(CODE "
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_directory
                ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor
        RESULT_VARIABLE result
    )
")

# ============================================================================
# Cppcheck Static Analysis
# ============================================================================
find_program(CPPCHECK cppcheck)
if(CPPCHECK)
    message(STATUS "  Cppcheck:       ${CPPCHECK} (version $(shell ${CPPCHECK} --version | head -1))")
    
    # Create cppcheck target (always available)
    add_custom_target(
        cppcheck
        COMMAND ${CPPCHECK}
            --enable=all
            --std=c++20
            --platform=unix64
            --library=gtk
            --library=libcurl
            --library=sqlite3
            --library=openssl
            --inconclusive
            --force
            --inline-suppr
            --suppress=missingIncludeSystem
            --suppress=unmatchedSuppression
            --suppress=preprocessorErrorOccurred
            --suppress=cannotOpenInlineSuppressionsFile
            --suppress=unusedFunction
            --suppress=syntaxError
            --suppress=noValidConfiguration
            --suppress=useStlAlgorithm
            --suppress=noExplicitConstructor
            --suppress=functionStatic
            --suppress=returnByReference
            --suppress=uninitMemberVar
            --suppress=variableScope
            --suppress=constVariableReference
            --suppress=constParameterPointer
            --suppress=unusedStructMember
            --suppress=redundantAssignment
            --suppress=danglingTemporaryLifetime
            --suppress=unknownMacro
            --error-exitcode=1
            -I ${CMAKE_SOURCE_DIR}/src
            -I /usr/include/gtk-4.0
            -I /usr/include/gtkmm-4.0
            -I /usr/include
            -i /usr/include/glib-2.0
            -i ${CMAKE_SOURCE_DIR}/third_party
            -i ${CMAKE_SOURCE_DIR}/vendor
            -i ${CMAKE_SOURCE_DIR}/deps
            -i ${CMAKE_SOURCE_DIR}/build
            ${CMAKE_SOURCE_DIR}/src
        COMMENT "Running cppcheck static analysis..."
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    
    # Create cppcheck-summary target (non-blocking, for CI/reports)
    add_custom_target(
        cppcheck-summary
        COMMAND ${CPPCHECK}
            --enable=warning,portability,performance
            --std=c++20
            --platform=unix64
            --quiet
            --inline-suppr
            --suppress=missingIncludeSystem
            --suppress=preprocessorErrorOccurred
            --suppress=cannotOpenInlineSuppressionsFile
            --suppress=unusedFunction
            --suppress=unmatchedSuppression
            -I ${CMAKE_SOURCE_DIR}/src
            -I /usr/include/gtk-4.0
            -I /usr/include/gtkmm-4.0
            -I /usr/include
            -i /usr/include/glib-2.0
            -i ${CMAKE_SOURCE_DIR}/third_party
            -i ${CMAKE_SOURCE_DIR}/vendor
            -i ${CMAKE_SOURCE_DIR}/deps
            -i ${CMAKE_SOURCE_DIR}/build
            ${CMAKE_SOURCE_DIR}/src
        COMMENT "Running cppcheck (quiet mode)..."
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    
    # Add cppcheck to build when ENABLE_CPPCHECK is ON
    if(ENABLE_CPPCHECK)
        message(STATUS "  Cppcheck:       ENABLED - will run with build")
        add_custom_target(cppcheck-all)
        add_dependencies(cppcheck-all cppcheck lucid)
        add_dependencies(all cppcheck-all)
    else()
        message(STATUS "  Cppcheck:       Available as 'make cppcheck' target (use -DENABLE_CPPCHECK=ON to enable)")
    endif()
else()
    message(STATUS "  Cppcheck:       NOT FOUND (install with: pacman -S cppcheck)")
endif()

# Install desktop entry
install(FILES data/lucid.desktop
        DESTINATION share/applications
)
