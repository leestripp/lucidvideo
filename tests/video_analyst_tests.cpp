// video_analyst_tests.cpp - Unit tests for Video Analyst components

#include <gtest/gtest.h>
#include "lucid/model/video_library.hpp"
#include "lucid/model/video_analyst.hpp"
#include "lucid/ffmpeg/thumbnail_generator.hpp"

using namespace lucid;

// ============================================================================
// Timecode Format Tests
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
// VideoStatus Enum Tests
// ============================================================================

TEST(VideoStatusTest, StringConversions) {
    EXPECT_STREQ(video_status_to_string(VideoStatus::PENDING), "pending");
    EXPECT_STREQ(video_status_to_string(VideoStatus::ANALYZING), "analyzing");
    EXPECT_STREQ(video_status_to_string(VideoStatus::TRANSCRIBING), "transcribing");
    EXPECT_STREQ(video_status_to_string(VideoStatus::THUMBNAILING), "thumbnailing");
    EXPECT_STREQ(video_status_to_string(VideoStatus::COMPLETED), "completed");
    EXPECT_STREQ(video_status_to_string(VideoStatus::FAILED), "failed");
    EXPECT_STREQ(video_status_to_string(VideoStatus::STALE), "stale");
}

TEST(VideoStatusTest, ParsingFromString) {
    EXPECT_EQ(video_status_from_string("pending"), VideoStatus::PENDING);
    EXPECT_EQ(video_status_from_string("analyzing"), VideoStatus::ANALYZING);
    EXPECT_EQ(video_status_from_string("transcribing"), VideoStatus::TRANSCRIBING);
    EXPECT_EQ(video_status_from_string("thumbnailing"), VideoStatus::THUMBNAILING);
    EXPECT_EQ(video_status_from_string("completed"), VideoStatus::COMPLETED);
    EXPECT_EQ(video_status_from_string("failed"), VideoStatus::FAILED);
    EXPECT_EQ(video_status_from_string("stale"), VideoStatus::STALE);
}

TEST(VideoStatusTest, UnknownStringDefaultsToPending) {
    EXPECT_EQ(video_status_from_string("unknown"), VideoStatus::PENDING);
    EXPECT_EQ(video_status_from_string(""), VideoStatus::PENDING);
}

// ============================================================================
// Marker Structure Tests
// ============================================================================

TEST(MarkerTest, DefaultValues) {
    Marker marker;
    EXPECT_EQ(marker.id, 0);
    EXPECT_EQ(marker.video_id, "");
    EXPECT_EQ(marker.segment_index, 0);
    EXPECT_EQ(marker.start_ms, 0);
    EXPECT_EQ(marker.end_ms, 0);
    EXPECT_EQ(marker.text, "");
    EXPECT_EQ(marker.confidence, 1.0f);
    EXPECT_EQ(marker.word_count, 0);
}

TEST(MarkerTest, CanSetAllFields) {
    Marker marker;
    marker.id = 42;
    marker.video_id = "test-video-123";
    marker.segment_index = 5;
    marker.start_ms = 3000;
    marker.end_ms = 6500;
    marker.timecode_start = "00:00:03.000";
    marker.timecode_end = "00:00:06.500";
    marker.text = "Hello world this is a test.";
    marker.confidence = 0.95f;
    marker.word_count = 7;
    
    EXPECT_EQ(marker.id, 42);
    EXPECT_EQ(marker.video_id, "test-video-123");
    EXPECT_EQ(marker.segment_index, 5);
    EXPECT_EQ(marker.start_ms, 3000);
    EXPECT_EQ(marker.end_ms, 6500);
    EXPECT_EQ(marker.text, "Hello world this is a test.");
    EXPECT_EQ(marker.confidence, 0.95f);
    EXPECT_EQ(marker.word_count, 7);
}

// ============================================================================
// ThumbnailEntry Structure Tests
// ============================================================================

TEST(ThumbnailEntryTest, DefaultValues) {
    ThumbnailEntry entry;
    EXPECT_EQ(entry.id, 0);
    EXPECT_EQ(entry.video_id, "");
    EXPECT_EQ(entry.path, "");
    EXPECT_EQ(entry.timestamp_ms, 0);
    EXPECT_EQ(entry.width, 0);
    EXPECT_EQ(entry.height, 0);
}

TEST(ThumbnailEntryTest, CanSetAllFields) {
    ThumbnailEntry entry;
    entry.id = 1;
    entry.video_id = "video-abc";
    entry.path = "/path/to/thumbnail.jpg";
    entry.timestamp_ms = 15000;
    entry.timecode = "00:00:15.000";
    entry.width = 512;
    entry.height = 288;
    
    EXPECT_EQ(entry.id, 1);
    EXPECT_EQ(entry.video_id, "video-abc");
    EXPECT_EQ(entry.timestamp_ms, 15000);
    EXPECT_EQ(entry.timecode, "00:00:15.000");
    EXPECT_EQ(entry.width, 512);
    EXPECT_EQ(entry.height, 288);
}

// ============================================================================
// DuplicateFound Structure Tests
// ============================================================================

TEST(DuplicateFoundTest, DefaultValues) {
    DuplicateFound dup;
    EXPECT_EQ(dup.existing_id, "");
    EXPECT_EQ(dup.reason, "");
}

TEST(DuplicateFoundTest, CanSetFields) {
    DuplicateFound dup;
    dup.existing_id = "video-123";
    dup.reason = "path";
    
    EXPECT_EQ(dup.existing_id, "video-123");
    EXPECT_EQ(dup.reason, "path");
}

// ============================================================================
// Pass1Result Tests
// ============================================================================

TEST(Pass1ResultTest, DefaultValues) {
    Pass1Result result;
    EXPECT_EQ(result.video_id, "");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_message, "");
    EXPECT_EQ(result.thumbnail_count, 0);
    EXPECT_EQ(result.audio_marker_count, 0);
    EXPECT_EQ(result.vision_marker_count, 0);
    EXPECT_TRUE(result.audio_markers.empty());
    EXPECT_TRUE(result.vision_markers.empty());
    EXPECT_TRUE(result.thumbnails.empty());
}

TEST(Pass1ResultTest, SuccessfulResult) {
    Pass1Result result;
    result.video_id = "test-video-123";
    result.success = true;
    result.thumbnail_count = 10;
    result.audio_marker_count = 5;
    result.vision_marker_count = 2;
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.thumbnail_count, 10);
    EXPECT_EQ(result.audio_marker_count, 5);
    EXPECT_EQ(result.vision_marker_count, 2);
}

TEST(Pass1ResultTest, IncludesThumbnails) {
    Pass1Result result;
    ThumbnailEntry thumb;
    thumb.path = "/tmp/thumb1.jpg";
    thumb.timestamp_ms = 1000;
    result.thumbnails.push_back(thumb);
    
    EXPECT_EQ(result.thumbnails.size(), 1);
    EXPECT_EQ(result.thumbnails[0].path, "/tmp/thumb1.jpg");
}

// ============================================================================
// ImportResult Variant Tests
// ============================================================================

TEST(ImportResultTest, SuccessfulImport) {
    ImportResult result = std::string("new-video-123");
    
    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_EQ(std::get<std::string>(result), "new-video-123");
}

TEST(ImportResultTest, DuplicateImport) {
    DuplicateFound dup;
    dup.existing_id = "existing-video-456";
    dup.reason = "path";
    
    ImportResult result = dup;
    
    ASSERT_TRUE(std::holds_alternative<DuplicateFound>(result));
    const auto& dup_result = std::get<DuplicateFound>(result);
    EXPECT_EQ(dup_result.existing_id, "existing-video-456");
    EXPECT_EQ(dup_result.reason, "path");
}

TEST(ImportResultTest, FailedImport) {
    ImportResult result = std::string("Import failed: error message");
    
    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    std::string msg = std::get<std::string>(result);
    EXPECT_TRUE(msg.find("failed") != std::string::npos);
}

// ============================================================================
// VideoAnalysisResult Tests
// ============================================================================

TEST(VideoAnalysisResultTest, DefaultValues) {
    VideoAnalysisResult result;
    EXPECT_EQ(result.video_id, "");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_message, "");
    EXPECT_EQ(result.duration_ms, 0);
    EXPECT_EQ(result.thumbnail_count, 0);
    EXPECT_EQ(result.marker_count, 0);
    EXPECT_TRUE(result.thumbnails.empty());
    EXPECT_TRUE(result.markers.empty());
}

TEST(VideoAnalysisResultTest, SuccessfulResult) {
    VideoAnalysisResult result;
    result.video_id = "test-video";
    result.success = true;
    result.duration_ms = 180000;
    result.thumbnail_count = 30;
    result.marker_count = 45;
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.duration_ms, 180000);
    EXPECT_EQ(result.thumbnail_count, 30);
    EXPECT_EQ(result.marker_count, 45);
}

// ============================================================================
// AnalysisProgress Tests
// ============================================================================

TEST(AnalysisProgressTest, DefaultValues) {
    AnalysisProgress progress;
    EXPECT_EQ(progress.phase, "");
    EXPECT_EQ(progress.progress, 0.0);
    EXPECT_EQ(progress.message, "");
}

TEST(AnalysisProgressTest, CanSetAllFields) {
    AnalysisProgress progress;
    progress.phase = "transcription";
    progress.progress = 0.75;
    progress.message = "Processing segment 15 of 20";
    
    EXPECT_EQ(progress.phase, "transcription");
    EXPECT_EQ(progress.progress, 0.75);
    EXPECT_EQ(progress.message, "Processing segment 15 of 20");
}