// ffmpeg_pipeline_tests.cpp - Integration tests for FFmpeg Pipeline (ThumbnailGenerator)
//
// These tests verify ThumbnailGenerator functionality which uses FFmpeg under the hood.
// Tests cover timecode formatting, thumbnail extraction, and error handling.

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <cstdio>

#include "lucid/ffmpeg/thumbnail_generator.hpp"

using namespace lucid;

// ============================================================================
// Test Fixtures
// ============================================================================

class ThumbnailGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Check if FFmpeg is available
        if (!ThumbnailGenerator::is_available()) {
            GTEST_SKIP() << "FFmpeg not available, skipping integration tests";
        }
        
        // Create temporary test directory
        m_test_dir = std::filesystem::temp_directory_path() / "lucid_thumb_test";
        std::filesystem::create_directories(m_test_dir);
    }
    
    void TearDown() override {
        // Cleanup test directory
        try {
            std::filesystem::remove_all(m_test_dir);
        } catch (...) {}
    }
    
    std::filesystem::path m_test_dir;
};

// ============================================================================
// Availability Tests
// ============================================================================

TEST(ThumbnailGeneratorAvailabilityTest, FFmpegIsAvailable) {
    EXPECT_TRUE(ThumbnailGenerator::is_available());
}

// ============================================================================
// Timecode Format Tests (pure unit tests)
// ============================================================================

TEST(TimecodeFormatTest, FormatsMillisecondsCorrectly) {
    // Test basic formatting
    EXPECT_EQ(ThumbnailGenerator::format_timecode(0), "00:00:00.000");
    EXPECT_EQ(ThumbnailGenerator::format_timecode(1000), "00:00:01.000");
    EXPECT_EQ(ThumbnailGenerator::format_timecode(60000), "00:01:00.000");
    EXPECT_EQ(ThumbnailGenerator::format_timecode(3661000), "01:01:01.000");
}

TEST(TimecodeFormatTest, HandlesHoursCorrectly) {
    EXPECT_EQ(ThumbnailGenerator::format_timecode(3600000), "01:00:00.000");
    EXPECT_EQ(ThumbnailGenerator::format_timecode(7200000), "02:00:00.000");
    EXPECT_EQ(ThumbnailGenerator::format_timecode(3600000 + 60000 + 1000 + 500), "01:01:01.500");
}

TEST(TimecodeFormatTest, ParsesTimecodeString) {
    auto ms = ThumbnailGenerator::parse_timecode("00:00:00.000");
    ASSERT_TRUE(ms.has_value());
    EXPECT_EQ(*ms, 0);
    
    ms = ThumbnailGenerator::parse_timecode("01:30:45.500");
    ASSERT_TRUE(ms.has_value());
    EXPECT_EQ(*ms, 1 * 3600000 + 30 * 60000 + 45 * 1000 + 500);
}

TEST(TimecodeFormatTest, ParsesTimecodeWithoutMillis) {
    auto ms = ThumbnailGenerator::parse_timecode("01:30:45");
    ASSERT_TRUE(ms.has_value());
    EXPECT_EQ(*ms, 1 * 3600000 + 30 * 60000 + 45 * 1000);
}

TEST(TimecodeFormatTest, InvalidTimecodeReturnsNullopt) {
    auto ms = ThumbnailGenerator::parse_timecode("invalid");
    EXPECT_FALSE(ms.has_value());
    
    ms = ThumbnailGenerator::parse_timecode("");
    EXPECT_FALSE(ms.has_value());
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(ThumbnailGeneratorTest, GetDurationMsReturnsNulloptForNonexistentFile) {
    ThumbnailGenerator gen;
    auto result = gen.get_duration_ms("/nonexistent/file.mp4");
    EXPECT_FALSE(result.has_value());
}

TEST_F(ThumbnailGeneratorTest, ExtractAtFailsForNonexistentFile) {
    ThumbnailGenerator gen;
    auto output_path = (m_test_dir / "single_thumb.jpg").string();
    
    auto result = gen.extract_at("/nonexistent/file.mp4", output_path, 0, 512);
    EXPECT_FALSE(result.has_value());
}

TEST_F(ThumbnailGeneratorTest, GenerateThumbnailsFailsForNonexistentFile) {
    ThumbnailGenerator gen;
    auto output_dir = m_test_dir / "thumbnails";
    std::filesystem::create_directories(output_dir);
    
    auto thumbnails = gen.generate("/nonexistent/file.mp4", output_dir.string(), 10, 512);
    EXPECT_TRUE(thumbnails.empty());
}

TEST_F(ThumbnailGeneratorTest, ExtractAtFailsForInvalidFile) {
    // Create a temporary file that is not a valid video
    auto not_video = m_test_dir / "not_a_video.txt";
    std::ofstream ofs(not_video);
    ofs << "This is not a video file";
    ofs.close();
    
    ThumbnailGenerator gen;
    auto output_path = (m_test_dir / "thumb.jpg").string();
    
    auto result = gen.extract_at(not_video.string(), output_path, 0, 512);
    EXPECT_FALSE(result.has_value());
}

TEST_F(ThumbnailGeneratorTest, GenerateFailsForInvalidFile) {
    // Create a temporary file that is not a valid video
    auto not_video = m_test_dir / "not_video.txt";
    std::ofstream ofs(not_video);
    ofs << "Not a video";
    ofs.close();
    
    ThumbnailGenerator gen;
    auto output_dir = m_test_dir / "out";
    std::filesystem::create_directories(output_dir);
    
    auto thumbnails = gen.generate(not_video.string(), output_dir.string(), 10, 512);
    EXPECT_TRUE(thumbnails.empty());
}

// ============================================================================
// Roundtrip Tests
// ============================================================================

TEST(TimecodeRoundtripTest, ParseAndFormatAreInverse) {
    ThumbnailGenerator gen;
    
    // Test several timestamps
    std::vector<int64_t> test_timestamps = {
        0,
        1000,
        60000,
        3661000,
        7200000
    };
    
    for (auto ms : test_timestamps) {
        std::string formatted = gen.format_timecode(ms);
        auto parsed = gen.parse_timecode(formatted);
        ASSERT_TRUE(parsed.has_value()) << "Failed to parse: " << formatted;
        EXPECT_EQ(*parsed, ms) << "Roundtrip failed for " << formatted;
    }
}

TEST(TimecodeRoundtripTest, ParseHandlesVariousFormats) {
    ThumbnailGenerator gen;
    
    std::vector<std::string> test_cases = {
        "00:00:00.000",
        "00:00:01.000",
        "01:30:45.500",
        "02:00:00.000",
    };
    
    for (const auto& tc : test_cases) {
        auto parsed = gen.parse_timecode(tc);
        ASSERT_TRUE(parsed.has_value()) << "Failed to parse: " << tc;
        
        // Format the parsed value back
        std::string formatted = gen.format_timecode(*parsed);
        EXPECT_EQ(formatted, tc) << "Roundtrip failed for " << tc;
    }
}