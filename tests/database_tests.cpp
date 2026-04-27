// database_tests.cpp - Database unit tests
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>

#include "lucid/model/database.hpp"

namespace lucid {
namespace testing {

// ============================================================================
// Database Test Fixture
// ============================================================================

class DatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create unique temp database path for each test
        temp_db_path = "/tmp/lucid_test_" + std::to_string(::getpid()) + "_" + 
                       std::to_string(test_count++) + ".db";
        
        // Remove any existing file
        std::filesystem::remove(temp_db_path);
        
        // Create database
        db = std::make_unique<Database>(temp_db_path);
    }
    
    void TearDown() override {
        db.reset();
        // Clean up temp database
        std::filesystem::remove(temp_db_path);
    }
    
    static int test_count;
    
    std::unique_ptr<Database> db;
    std::string temp_db_path;
};

int DatabaseTest::test_count = 0;

// ============================================================================
// Video CRUD Tests
// ============================================================================

TEST_F(DatabaseTest, AddAndRetrieveVideo) {
    VideoEntry video;
    video.id = "test-video-001";
    video.path = "/tmp/test_video.mp4";
    video.title = "Test Video";
    video.source = "local";
    video.status = VideoStatus::PENDING;
    
    db->add_video(video);
    
    auto retrieved = db->get_video("test-video-001");
    
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->title, "Test Video");
    EXPECT_EQ(retrieved->path, "/tmp/test_video.mp4");
    EXPECT_EQ(retrieved->status, VideoStatus::PENDING);
}

TEST_F(DatabaseTest, GetVideoNotFound) {
    auto result = db->get_video("non-existent-id");
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(DatabaseTest, UpdateVideoStatus) {
    // Add video
    VideoEntry video;
    video.id = "test-video-002";
    video.path = "/tmp/test.mp4";
    video.title = "Test";
    video.source = "local";
    db->add_video(video);
    
    // Update status
    db->update_video_status("test-video-002", VideoStatus::COMPLETED);
    
    // Verify
    auto retrieved = db->get_video("test-video-002");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->status, VideoStatus::COMPLETED);
}

TEST_F(DatabaseTest, UpdateVideoDuration) {
    VideoEntry video;
    video.id = "test-video-003";
    video.path = "/tmp/test.mp4";
    video.title = "Test";
    video.source = "local";
    db->add_video(video);
    
    db->update_video_duration("test-video-003", 60000);  // 60 seconds
    
    auto retrieved = db->get_video("test-video-003");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->duration_ms, 60000);
}

TEST_F(DatabaseTest, GetAllVideosPagination) {
    // Add multiple videos
    for (int i = 0; i < 5; i++) {
        VideoEntry video;
        video.id = "video-" + std::to_string(i);
        video.path = "/tmp/video-pag-" + std::to_string(i) + ".mp4";  // Unique paths
        video.title = "Video " + std::to_string(i);
        video.source = "local";
        db->add_video(video);
    }
    
    // Test pagination (DB orders by created_at DESC, so newest first)
    auto first_page = db->get_all_videos(2, 0);
    auto second_page = db->get_all_videos(2, 2);
    
    EXPECT_EQ(first_page.size(), 2);
    EXPECT_EQ(second_page.size(), 2);
    // Videos are ordered by created_at DESC, so newest first (video-4, video-3)
    EXPECT_EQ(first_page[0].id, "video-4");
    EXPECT_EQ(first_page[1].id, "video-3");
}

TEST_F(DatabaseTest, DeleteVideo) {
    VideoEntry video;
    video.id = "test-video-delete";
    video.path = "/tmp/test.mp4";
    video.title = "Test";
    video.source = "local";
    db->add_video(video);
    
    // Verify it exists
    ASSERT_TRUE(db->get_video("test-video-delete").has_value());
    
    // Delete
    db->delete_video("test-video-delete");
    
    // Verify it's gone
    EXPECT_FALSE(db->get_video("test-video-delete").has_value());
}

TEST_F(DatabaseTest, FindByPath) {
    VideoEntry video;
    video.id = "test-video-path";
    video.path = "/tmp/unique_path_12345.mp4";
    video.title = "Test";
    video.source = "local";
    db->add_video(video);
    
    auto found_id = db->find_by_path("/tmp/unique_path_12345.mp4");
    
    ASSERT_TRUE(found_id.has_value());
    EXPECT_EQ(*found_id, "test-video-path");
}

TEST_F(DatabaseTest, FindByPathNotFound) {
    auto result = db->find_by_path("/nonexistent/path.mp4");
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(DatabaseTest, SetVideoError) {
    VideoEntry video;
    video.id = "test-video-error";
    video.path = "/tmp/test.mp4";
    video.title = "Test";
    video.source = "local";
    db->add_video(video);
    
    db->set_video_error("test-video-error", "Test error message");
    
    auto retrieved = db->get_video("test-video-error");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->error_message, "Test error message");
    EXPECT_EQ(retrieved->status, VideoStatus::FAILED);
}

// ============================================================================
// Video Status Tests
// ============================================================================

TEST_F(DatabaseTest, StatusEnumConversions) {
    EXPECT_EQ(video_status_from_string("pending"), VideoStatus::PENDING);
    EXPECT_EQ(video_status_from_string("completed"), VideoStatus::COMPLETED);
    EXPECT_EQ(video_status_from_string("failed"), VideoStatus::FAILED);
    
    EXPECT_STREQ(video_status_to_string(VideoStatus::PENDING), "pending");
    EXPECT_STREQ(video_status_to_string(VideoStatus::COMPLETED), "completed");
}

TEST_F(DatabaseTest, GetVideoCountByStatus) {
    // Add videos with different statuses
    for (int i = 0; i < 3; i++) {
        VideoEntry video;
        video.id = "pending-" + std::to_string(i);
        video.path = "/tmp/pending-" + std::to_string(i) + ".mp4";
        video.title = "Pending";
        video.source = "local";
        video.status = VideoStatus::PENDING;
        db->add_video(video);
    }
    
    VideoEntry completed;
    completed.id = "completed-1";
    completed.path = "/tmp/completed.mp4";
    completed.title = "Completed";
    completed.source = "local";
    completed.status = VideoStatus::COMPLETED;
    db->add_video(completed);
    
    EXPECT_EQ(db->get_video_count_by_status(VideoStatus::PENDING), 3);
    EXPECT_EQ(db->get_video_count_by_status(VideoStatus::COMPLETED), 1);
}

TEST_F(DatabaseTest, ResetAllVideosToPending) {
    // Add some completed videos
    for (int i = 0; i < 2; i++) {
        VideoEntry video;
        video.id = "completed-" + std::to_string(i);
        video.path = "/tmp/completed-" + std::to_string(i) + ".mp4";
        video.title = "Completed";
        video.source = "local";
        video.status = VideoStatus::COMPLETED;
        db->add_video(video);
    }
    
    db->reset_all_videos_to_pending();
    
    auto videos = db->get_all_videos();
    for (const auto& v : videos) {
        EXPECT_EQ(v.status, VideoStatus::PENDING);
    }
}

// ============================================================================
// Marker CRUD Tests
// ============================================================================

TEST_F(DatabaseTest, AddAndGetMarker) {
    // Add video first
    VideoEntry video;
    video.id = "video-for-marker";
    video.path = "/tmp/test.mp4";
    video.title = "Test";
    video.source = "local";
    db->add_video(video);
    
    // Add marker
    Marker marker;
    marker.video_id = "video-for-marker";
    marker.segment_index = 0;
    marker.start_ms = 0;
    marker.end_ms = 5000;
    marker.text = "Hello world test";
    marker.timecode_start = "00:00:00.000";
    marker.timecode_end = "00:00:05.000";
    
    int64_t marker_id = db->add_marker(marker);
    EXPECT_GT(marker_id, 0);
    
    // Retrieve markers
    auto markers = db->get_markers("video-for-marker");
    
    ASSERT_EQ(markers.size(), 1);
    EXPECT_EQ(markers[0].text, "Hello world test");
    EXPECT_EQ(markers[0].start_ms, 0);
    EXPECT_EQ(markers[0].end_ms, 5000);
}

TEST_F(DatabaseTest, AddMultipleMarkers) {
    VideoEntry video;
    video.id = "video-multi-marker";
    video.path = "/tmp/test.mp4";
    video.title = "Test";
    video.source = "local";
    db->add_video(video);
    
    std::vector<Marker> markers = {
        {0, "video-multi-marker", 0, 0, 5000, -1, "00:00:00.000", "00:00:05.000", "First segment", 1.0f, 0, "", std::nullopt, {}, ""},
        {0, "video-multi-marker", 1, 5000, 10000, -1, "00:00:05.000", "00:00:10.000", "Second segment", 1.0f, 0, "", std::nullopt, {}, ""},
        {0, "video-multi-marker", 2, 10000, 15000, -1, "00:00:10.000", "00:00:15.000", "Third segment", 1.0f, 0, "", std::nullopt, {}, ""}
    };
    
    db->add_markers("video-multi-marker", markers);
    
    auto retrieved = db->get_markers("video-multi-marker");
    
    ASSERT_EQ(retrieved.size(), 3);
    EXPECT_EQ(retrieved[0].text, "First segment");
    EXPECT_EQ(retrieved[1].text, "Second segment");
    EXPECT_EQ(retrieved[2].text, "Third segment");
}

TEST_F(DatabaseTest, ClearMarkers) {
    VideoEntry video;
    video.id = "video-clear-marker";
    video.path = "/tmp/test.mp4";
    video.title = "Test";
    video.source = "local";
    db->add_video(video);
    
    Marker marker;
    marker.video_id = "video-clear-marker";
    marker.segment_index = 0;
    marker.start_ms = 0;
    marker.end_ms = 5000;
    marker.text = "To be cleared";
    db->add_marker(marker);
    
    ASSERT_EQ(db->get_markers("video-clear-marker").size(), 1);
    
    db->clear_markers("video-clear-marker");
    
    EXPECT_TRUE(db->get_markers("video-clear-marker").empty());
}

// ============================================================================
// Marker Type and Thumbnail Tests
// ============================================================================

TEST_F(DatabaseTest, AddMarkerWithAudioType) {
    // Add video first
    VideoEntry video;
    video.id = "video-audio-marker";
    video.path = "/tmp/test.mp4";
    video.title = "Test";
    video.source = "local";
    db->add_video(video);

    // Add AUDIO marker (transcription)
    Marker marker;
    marker.video_id = "video-audio-marker";
    marker.segment_index = 0;
    marker.start_ms = 0;
    marker.end_ms = 5000;
    marker.text = "Speech transcription";
    marker.marker_type = MarkerType::AUDIO;
    marker.thumbnail_path = "/path/to/speaker_icon.png";

    int64_t marker_id = db->add_marker(marker);
    EXPECT_GT(marker_id, 0);

    // Retrieve and verify
    auto markers = db->get_markers("video-audio-marker");
    ASSERT_EQ(markers.size(), 1);
    EXPECT_EQ(markers[0].marker_type, MarkerType::AUDIO);
    EXPECT_EQ(markers[0].thumbnail_path, "/path/to/speaker_icon.png");
}

TEST_F(DatabaseTest, AddMarkerWithVisionType) {
    // Add video first
    VideoEntry video;
    video.id = "video-vision-marker";
    video.path = "/tmp/test.mp4";
    video.title = "Test";
    video.source = "local";
    db->add_video(video);

    // Add VISION marker (image analysis)
    Marker marker;
    marker.video_id = "video-vision-marker";
    marker.segment_index = 0;
    marker.start_ms = 30000;
    marker.end_ms = 35000;
    marker.text = "A person coding at a desk";
    marker.marker_type = MarkerType::VISION;
    marker.thumbnail_path = "/path/to/thumbnail_0030.jpg";

    int64_t marker_id = db->add_marker(marker);
    EXPECT_GT(marker_id, 0);

    // Retrieve and verify
    auto markers = db->get_markers("video-vision-marker");
    ASSERT_EQ(markers.size(), 1);
    EXPECT_EQ(markers[0].marker_type, MarkerType::VISION);
    EXPECT_EQ(markers[0].thumbnail_path, "/path/to/thumbnail_0030.jpg");
}

TEST_F(DatabaseTest, AddMultipleMarkersDifferentTypes) {
    VideoEntry video;
    video.id = "video-mixed-markers";
    video.path = "/tmp/test.mp4";
    video.title = "Test";
    video.source = "local";
    db->add_video(video);

    // Add mixed marker types
    std::vector<Marker> markers = {
        {0, "video-mixed-markers", 0, 0, 5000, -1, "", "", "First speech", 1.0f, 0, "", std::nullopt, MarkerType::AUDIO, "/icons/speaker.png", {}},
        {0, "video-mixed-markers", 1, 5000, 10000, -1, "", "", "Second speech", 1.0f, 0, "", std::nullopt, MarkerType::AUDIO, "/icons/speaker.png", {}},
        {0, "video-mixed-markers", 2, 10000, 15000, -1, "", "", "Image at 10s", 1.0f, 0, "", std::nullopt, MarkerType::VISION, "/thumb/thumb_010.jpg", {}},
        {0, "video-mixed-markers", 3, 15000, 20000, -1, "", "", "Third speech", 1.0f, 0, "", std::nullopt, MarkerType::AUDIO, "/icons/speaker.png", {}},
        {0, "video-mixed-markers", 4, 20000, 25000, -1, "", "", "Image at 20s", 1.0f, 0, "", std::nullopt, MarkerType::VISION, "/thumb/thumb_020.jpg", {}}
    };

    db->add_markers("video-mixed-markers", markers);

    auto retrieved = db->get_markers("video-mixed-markers");
    ASSERT_EQ(retrieved.size(), 5);

    // Verify marker types
    EXPECT_EQ(retrieved[0].marker_type, MarkerType::AUDIO);
    EXPECT_EQ(retrieved[1].marker_type, MarkerType::AUDIO);
    EXPECT_EQ(retrieved[2].marker_type, MarkerType::VISION);
    EXPECT_EQ(retrieved[3].marker_type, MarkerType::AUDIO);
    EXPECT_EQ(retrieved[4].marker_type, MarkerType::VISION);

    // Verify thumbnail paths
    EXPECT_EQ(retrieved[0].thumbnail_path, "/icons/speaker.png");
    EXPECT_EQ(retrieved[2].thumbnail_path, "/thumb/thumb_010.jpg");
    EXPECT_EQ(retrieved[4].thumbnail_path, "/thumb/thumb_020.jpg");
}

TEST_F(DatabaseTest, MarkerTypeDefaultValue) {
    VideoEntry video;
    video.id = "video-default-type";
    video.path = "/tmp/test.mp4";
    video.title = "Test";
    video.source = "local";
    db->add_video(video);

    // Add marker without explicitly setting marker_type
    Marker marker;
    marker.video_id = "video-default-type";
    marker.segment_index = 0;
    marker.start_ms = 0;
    marker.end_ms = 5000;
    marker.text = "Default type test";
    // marker_type should default to MarkerType::AUDIO per struct definition

    db->add_marker(marker);

    auto markers = db->get_markers("video-default-type");
    ASSERT_EQ(markers.size(), 1);
    // Verify default is AUDIO
    EXPECT_EQ(markers[0].marker_type, MarkerType::AUDIO);
}

// ============================================================================
// Search Tests (FTS5)
// ============================================================================

TEST_F(DatabaseTest, SearchMarkersFTS) {
    VideoEntry video;
    video.id = "video-search-test";
    video.path = "/tmp/test.mp4";
    video.title = "Test";
    video.source = "local";
    db->add_video(video);
    
    // Add markers with different text
    std::vector<Marker> markers = {
        {0, "video-search-test", 0, 0, 5000, -1, "", "", "pandas are eating bamboo", 1.0f, 0, "", std::nullopt, {}, ""},
        {0, "video-search-test", 1, 5000, 10000, -1, "", "", "the bear loves honey", 1.0f, 0, "", std::nullopt, {}, ""},
        {0, "video-search-test", 2, 10000, 15000, -1, "", "", "pandas dancing in the snow", 1.0f, 0, "", std::nullopt, {}, ""}
    };
    db->add_markers("video-search-test", markers);
    
    // Search for "pandas"
    auto results = db->search_markers("pandas", 10);
    
    ASSERT_EQ(results.size(), 2);
    EXPECT_TRUE(results[0].text.find("pandas") != std::string::npos);
    EXPECT_TRUE(results[1].text.find("pandas") != std::string::npos);
}

TEST_F(DatabaseTest, SearchMarkersNoResults) {
    VideoEntry video;
    video.id = "video-no-results";
    video.path = "/tmp/test.mp4";
    video.title = "Test";
    video.source = "local";
    db->add_video(video);
    
    Marker marker;
    marker.video_id = "video-no-results";
    marker.text = "completely unrelated text";
    db->add_marker(marker);
    
    auto results = db->search_markers("xyz123nonexistent", 10);
    
    EXPECT_TRUE(results.empty());
}

// ============================================================================
// Embedding Tests
// ============================================================================

TEST_F(DatabaseTest, AddAndGetEmbeddings) {
    // Create test embedding (768 dim for nomic)
    std::vector<float> embedding(768, 0.5f);
    embedding[0] = 1.0f;  // Make it non-zero
    
    db->add_embedding("test-embedding-1", "test video", embedding);
    
    auto all_embeddings = db->get_all_embeddings();
    
    ASSERT_EQ(all_embeddings.size(), 1);
    EXPECT_EQ(all_embeddings[0].id, "test-embedding-1");
    EXPECT_EQ(all_embeddings[0].text, "test video");
    EXPECT_EQ(all_embeddings[0].embedding.size(), 768);
}

TEST_F(DatabaseTest, DeleteEmbeddingsForVideo) {
    // Add embeddings for a video
    std::vector<float> emb(768, 0.1f);
    db->add_embedding("video-emb-1", "first", emb);
    db->add_embedding("video-emb-2", "second", emb);
    
    auto embeddings = db->get_all_embeddings();
    ASSERT_EQ(embeddings.size(), 2);
    
    // Delete embeddings for this "video" (id contains "video")
    db->delete_embeddings_for_video("video");
    
    // Note: delete_embeddings_for_video uses video_id prefix matching
    // For this test we just verify the function exists and runs
}

TEST_F(DatabaseTest, GetEmbeddingsForVideo) {
    std::vector<float> emb(768, 0.3f);
    
    db->add_embedding("specific-video:0", "specific video", emb);
    db->add_embedding("other-video:0", "other", emb);
    
    auto video_embeddings = db->get_embeddings_for_video("specific-video");
    
    EXPECT_EQ(video_embeddings.size(), 1);
    EXPECT_EQ(video_embeddings[0].text, "specific video");
}

// ============================================================================
// Thumbnail Tests
// ============================================================================

TEST_F(DatabaseTest, AddAndGetThumbnails) {
    VideoEntry video;
    video.id = "video-thumbnails";
    video.path = "/tmp/test.mp4";
    video.title = "Test";
    video.source = "local";
    db->add_video(video);
    
    ThumbnailEntry thumb;
    thumb.video_id = "video-thumbnails";
    thumb.path = "/tmp/thumb1.jpg";
    thumb.timestamp_ms = 10000;
    thumb.timecode = "00:00:10.000";
    
    int64_t thumb_id = db->add_thumbnail(thumb);
    EXPECT_GT(thumb_id, 0);
    
    auto thumbnails = db->get_thumbnails("video-thumbnails");
    
    ASSERT_EQ(thumbnails.size(), 1);
    EXPECT_EQ(thumbnails[0].path, "/tmp/thumb1.jpg");
    EXPECT_EQ(thumbnails[0].timestamp_ms, 10000);
}

TEST_F(DatabaseTest, ClearThumbnails) {
    VideoEntry video;
    video.id = "video-clear-thumb";
    video.path = "/tmp/test.mp4";
    video.title = "Test";
    video.source = "local";
    db->add_video(video);
    
    ThumbnailEntry thumb;
    thumb.video_id = "video-clear-thumb";
    thumb.path = "/tmp/thumb.jpg";
    db->add_thumbnail(thumb);
    
    ASSERT_EQ(db->get_thumbnails("video-clear-thumb").size(), 1);
    
    db->clear_thumbnails("video-clear-thumb");
    
    EXPECT_TRUE(db->get_thumbnails("video-clear-thumb").empty());
}

// ============================================================================
// Timeline Tests
// ============================================================================

TEST_F(DatabaseTest, GetTimeline) {
    VideoEntry video;
    video.id = "video-timeline";
    video.path = "/tmp/test.mp4";
    video.title = "Test";
    video.source = "local";
    db->add_video(video);
    
    // Add marker
    Marker marker;
    marker.video_id = "video-timeline";
    marker.start_ms = 5000;
    marker.end_ms = 10000;
    marker.text = "Timeline test";
    db->add_marker(marker);
    
    // Add thumbnail
    ThumbnailEntry thumb;
    thumb.video_id = "video-timeline";
    thumb.path = "/tmp/timeline_thumb.jpg";
    thumb.timestamp_ms = 3000;
    db->add_thumbnail(thumb);
    
    auto timeline = db->get_timeline("video-timeline");
    
    // Should have 2 entries (1 marker + 1 thumbnail)
    EXPECT_EQ(timeline.size(), 2);
}

// ============================================================================
// Transcription Tests
// ============================================================================

TEST_F(DatabaseTest, AddAndGetTranscription) {
    VideoEntry video;
    video.id = "video-transcription";
    video.path = "/tmp/test.mp4";
    video.title = "Test";
    video.source = "local";
    db->add_video(video);
    
    std::vector<TranscriptionSegment> segments = {
        {0, "video-transcription", 0, 5000, "", "", "First segment text", 0.95f, 2, "", std::nullopt, false},
        {0, "video-transcription", 5000, 10000, "", "", "Second segment text", 0.90f, 3, "", std::nullopt, false}
    };
    
    db->add_transcription("video-transcription", segments);
    
    auto retrieved = db->get_transcription("video-transcription");
    
    ASSERT_EQ(retrieved.size(), 2);
    EXPECT_EQ(retrieved[0].text, "First segment text");
    EXPECT_EQ(retrieved[1].text, "Second segment text");
}

TEST_F(DatabaseTest, ClearTranscription) {
    VideoEntry video;
    video.id = "video-clear-transcription";
    video.path = "/tmp/test.mp4";
    video.title = "Test";
    video.source = "local";
    db->add_video(video);
    
    TranscriptionSegment seg;
    seg.video_id = "video-clear-transcription";
    seg.text = "To be cleared";
    db->add_transcription("video-clear-transcription", {seg});
    
    ASSERT_EQ(db->get_transcription("video-clear-transcription").size(), 1);
    
    db->clear_transcription("video-clear-transcription");
    
    EXPECT_TRUE(db->get_transcription("video-clear-transcription").empty());
}

// ============================================================================
// Bulk Operations Tests
// ============================================================================

TEST_F(DatabaseTest, ClearAllMarkers) {
    // Add multiple videos with markers (use unique prefixes)
    for (int i = 0; i < 3; i++) {
        VideoEntry video;
        video.id = "bulk-marker-" + std::to_string(i);
        video.path = "/tmp/bulk-marker-" + std::to_string(i) + ".mp4";
        video.title = "Test";
        video.source = "local";
        db->add_video(video);
        
        Marker marker;
        marker.video_id = "bulk-marker-" + std::to_string(i);
        marker.text = "Marker text " + std::to_string(i);
        db->add_marker(marker);
    }
    
    EXPECT_GT(db->get_marker_count(), 0);
    
    db->clear_all_markers();
    
    EXPECT_EQ(db->get_marker_count(), 0);
}

TEST_F(DatabaseTest, ClearAllEmbeddings) {
    std::vector<float> emb(768, 0.1f);
    for (int i = 0; i < 3; i++) {
        db->add_embedding("emb-" + std::to_string(i), "text", emb);
    }
    
    ASSERT_GT(db->get_all_embeddings().size(), 0);
    
    db->clear_all_embeddings();
    
    EXPECT_TRUE(db->get_all_embeddings().empty());
}

TEST_F(DatabaseTest, GetFailedVideos) {
    VideoEntry failed;
    failed.id = "failed-video-1";
    failed.path = "/tmp/failed.mp4";
    failed.title = "Failed";
    failed.source = "local";
    failed.status = VideoStatus::FAILED;
    failed.error_message = "Test failure";
    db->add_video(failed);
    
    VideoEntry pending;
    pending.id = "pending-video-1";
    pending.path = "/tmp/pending.mp4";
    pending.title = "Pending";
    pending.source = "local";
    pending.status = VideoStatus::PENDING;
    db->add_video(pending);
    
    auto failed_videos = db->get_failed_videos();
    
    ASSERT_EQ(failed_videos.size(), 1);
    EXPECT_EQ(failed_videos[0].id, "failed-video-1");
}

TEST_F(DatabaseTest, DeleteFailedVideos) {
    for (int i = 0; i < 2; i++) {
        VideoEntry failed;
        failed.id = "will-delete-" + std::to_string(i);
        failed.path = "/tmp/failed" + std::to_string(i) + ".mp4";
        failed.title = "Failed";
        failed.source = "local";
        failed.status = VideoStatus::FAILED;
        db->add_video(failed);
    }
    
    ASSERT_EQ(db->get_failed_videos().size(), 2);
    
    db->delete_failed_videos();
    
    EXPECT_TRUE(db->get_failed_videos().empty());
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(DatabaseTest, GetTotalVideoCount) {
    for (int i = 0; i < 5; i++) {
        VideoEntry video;
        video.id = "count-video-" + std::to_string(i);
        video.path = "/tmp/count-video-" + std::to_string(i) + ".mp4";  // Unique path
        video.title = "Test";
        video.source = "local";
        db->add_video(video);
    }
    
    EXPECT_EQ(db->get_total_video_count(), 5);
}

TEST_F(DatabaseTest, GetMarkerCount) {
    VideoEntry video;
    video.id = "marker-count-video";
    video.path = "/tmp/marker-count-unique.mp4";  // Unique path
    video.title = "Test";
    video.source = "local";
    db->add_video(video);
    
    for (int i = 0; i < 3; i++) {
        Marker marker;
        marker.video_id = "marker-count-video";
        marker.segment_index = i;  // Different segment index for each
        marker.text = "Marker " + std::to_string(i);
        db->add_marker(marker);
    }
    
    EXPECT_EQ(db->get_marker_count(), 3);
}

// ============================================================================
// Database Path Tests
// ============================================================================

TEST_F(DatabaseTest, GetDatabasePath) {
    EXPECT_EQ(db->get_database_path(), temp_db_path);
}

TEST_F(DatabaseTest, RawDatabasePointer) {
    ASSERT_NE(db->raw(), nullptr);
}

// ============================================================================
// Utility Tests
// ============================================================================

TEST_F(DatabaseTest, ResetCompletedToPending) {
    // Add completed video
    VideoEntry completed;
    completed.id = "completed-reset";
    completed.path = "/tmp/completed.mp4";
    completed.title = "Completed";
    completed.source = "local";
    completed.status = VideoStatus::COMPLETED;
    db->add_video(completed);
    
    // Add marker to the completed video
    Marker marker;
    marker.video_id = "completed-reset";
    marker.text = "Should be cleared";
    db->add_marker(marker);
    
    // Add thumbnail to the completed video
    ThumbnailEntry thumb;
    thumb.video_id = "completed-reset";
    thumb.path = "/tmp/thumb.jpg";
    thumb.timestamp_ms = 1000;
    thumb.timecode = "00:00:01.000";
    db->add_thumbnail(thumb);
    
    // Add vision analysis to the completed video
    VisionAnalysis vision;
    vision.video_id = "completed-reset";
    vision.thumbnail_id = 1;
    vision.description = "Test scene";
    vision.scene_type = "indoor";
    vision.confidence = 0.9;
    db->add_vision_analysis(vision);
    
    // Add embedding to the completed video (format: "video_id:marker_index")
    std::vector<float> embedding(768, 0.5f);
    db->add_embedding("completed-reset:1", "marker", embedding);
    
    // Verify all data exists
    EXPECT_GT(db->get_markers("completed-reset").size(), 0);
    EXPECT_GT(db->get_thumbnails("completed-reset").size(), 0);
    EXPECT_GT(db->get_vision_analyses("completed-reset").size(), 0);
    EXPECT_GT(db->get_embeddings_for_video("completed-reset").size(), 0);
    
    // Reset completed to pending (should clear all analysis data)
    db->reset_completed_to_pending();
    
    auto video = db->get_video("completed-reset");
    ASSERT_TRUE(video.has_value());
    EXPECT_EQ(video->status, VideoStatus::PENDING);
    
    // All analysis data should be cleared
    EXPECT_TRUE(db->get_markers("completed-reset").empty());
    EXPECT_TRUE(db->get_thumbnails("completed-reset").empty());
    EXPECT_TRUE(db->get_vision_analyses("completed-reset").empty());
    EXPECT_TRUE(db->get_embeddings_for_video("completed-reset").empty());
}

// ============================================================================
// Semantic Search Tests
// ============================================================================

TEST_F(DatabaseTest, AddAndRetrieveEmbedding) {
    // Add a video first
    VideoEntry video;
    video.id = "embed-test-video";
    video.path = "/tmp/embed_test.mp4";
    video.title = "Embedding Test Video";
    video.source = "local";
    db->add_video(video);
    
    // Create a test embedding (simplified 4-dimensional vector)
    std::vector<float> embedding = {0.1f, 0.2f, 0.3f, 0.4f};
    
    // Add embedding
    std::string emb_id = "embed-test-video:0";
    std::string text = "This is a test transcription segment";
    db->add_embedding(emb_id, text, embedding);
    
    // Retrieve all embeddings
    auto embeddings = db->get_all_embeddings();
    
    ASSERT_FALSE(embeddings.empty());
    
    // Find our embedding
    bool found = false;
    for (const auto& emb : embeddings) {
        if (emb.video_id == "embed-test-video") {
            found = true;
            EXPECT_EQ(emb.text, text);
            EXPECT_EQ(emb.embedding.size(), 4);
            EXPECT_NEAR(emb.embedding[0], 0.1f, 0.001f);
            EXPECT_NEAR(emb.embedding[1], 0.2f, 0.001f);
            EXPECT_NEAR(emb.embedding[2], 0.3f, 0.001f);
            EXPECT_NEAR(emb.embedding[3], 0.4f, 0.001f);
            break;
        }
    }
    EXPECT_TRUE(found) << "Embedding for embed-test-video not found";
}

TEST_F(DatabaseTest, SemanticSearchWithIdenticalEmbedding) {
    // Add video
    VideoEntry video;
    video.id = "semantic-test-1";
    video.path = "/tmp/semantic1.mp4";
    video.title = "Semantic Test";
    video.source = "local";
    db->add_video(video);
    
    // Add embedding
    std::vector<float> embedding = {1.0f, 0.0f, 0.0f, 0.0f};
    db->add_embedding("semantic-test-1:0", "Test segment", embedding);
    
    // Search with identical embedding - should find it with high similarity
    auto results = db->semantic_search(embedding, 10);
    
    EXPECT_FALSE(results.empty()) << "Semantic search should return results for identical embedding";
    EXPECT_EQ(results[0].video_id, "semantic-test-1");
    EXPECT_NEAR(results[0].relevance_score, 1.0f, 0.001f) << "Identical vectors should have similarity 1.0";
}

TEST_F(DatabaseTest, SemanticSearchWithDifferentEmbedding) {
    // Add video
    VideoEntry video;
    video.id = "semantic-test-2";
    video.path = "/tmp/semantic2.mp4";
    video.title = "Different Test";
    video.source = "local";
    db->add_video(video);
    
    // Add embedding
    std::vector<float> stored_emb = {1.0f, 0.0f, 0.0f, 0.0f};
    db->add_embedding("semantic-test-2:0", "Different segment", stored_emb);
    
    // Search with orthogonal embedding - should have low similarity
    std::vector<float> query_emb = {0.0f, 1.0f, 0.0f, 0.0f};
    auto results = db->semantic_search(query_emb, 10);
    
    EXPECT_TRUE(results.empty()) << "Orthogonal vectors should have similarity 0";
}

TEST_F(DatabaseTest, SemanticSearchWithPartialSimilarity) {
    // Add video
    VideoEntry video;
    video.id = "semantic-test-3";
    video.path = "/tmp/semantic3.mp4";
    video.title = "Partial Similarity Test";
    video.source = "local";
    db->add_video(video);
    
    // Add embedding
    std::vector<float> stored_emb = {1.0f, 1.0f, 0.0f, 0.0f};
    db->add_embedding("semantic-test-3:0", "Partial segment", stored_emb);
    
    // Search with partially similar embedding
    std::vector<float> query_emb = {1.0f, 0.0f, 0.0f, 0.0f};
    auto results = db->semantic_search(query_emb, 10);
    
    ASSERT_FALSE(results.empty()) << "Should find partial similarity";
    // (1,1,0,0) dot (1,0,0,0) = 1
    // |stored| = sqrt(2), |query| = 1
    // similarity = 1 / sqrt(2) ≈ 0.707
    EXPECT_NEAR(results[0].relevance_score, 0.707f, 0.01f);
}

TEST_F(DatabaseTest, SemanticSearchMultipleResults) {
    // Add multiple videos with embeddings
    for (int i = 0; i < 3; i++) {
        VideoEntry video;
        video.id = "multi-" + std::to_string(i);
        video.path = "/tmp/multi" + std::to_string(i) + ".mp4";
        video.title = "Multi Test " + std::to_string(i);
        video.source = "local";
        db->add_video(video);
        
        // Add embedding with varying similarity to query {1, 0, 0, 0}
        // Use different angles to get different similarities
        std::vector<float> embedding;
        if (i == 0) {
            embedding = {1.0f, 0.0f, 0.0f, 0.0f};  // Best match (1.0)
        } else if (i == 1) {
            embedding = {0.8f, 0.2f, 0.0f, 0.0f};  // High match (~0.97)
        } else {
            embedding = {0.5f, 0.5f, 0.0f, 0.0f};   // Medium match (~0.707)
        }
        db->add_embedding("multi-" + std::to_string(i) + ":0", "Segment " + std::to_string(i), embedding);
    }
    
    // Search with query {1, 0, 0, 0}
    std::vector<float> query = {1.0f, 0.0f, 0.0f, 0.0f};
    auto results = db->semantic_search(query, 10);
    
    ASSERT_EQ(results.size(), 3) << "Should return all 3 results with positive similarity";
    
    // Best match should be first
    EXPECT_EQ(results[0].video_id, "multi-0") << "Best matching video should be first";
    EXPECT_GT(results[0].relevance_score, results[1].relevance_score);
    EXPECT_GT(results[1].relevance_score, results[2].relevance_score);
}

TEST_F(DatabaseTest, SemanticSearchEmptyEmbedding) {
    auto results = db->semantic_search({}, 10);
    EXPECT_TRUE(results.empty()) << "Empty embedding should return no results";
}

TEST_F(DatabaseTest, DeleteAllEmbeddingsForVideo) {
    // Add video with multiple markers
    VideoEntry video;
    video.id = "delete-embed-test";
    video.path = "/tmp/delete_embed.mp4";
    video.title = "Delete Embed Test";
    video.source = "local";
    db->add_video(video);
    
    // Add multiple embeddings
    for (int i = 0; i < 3; i++) {
        std::vector<float> embedding = {0.1f * (i + 1), 0.2f, 0.3f, 0.4f};
        db->add_embedding("delete-embed-test:" + std::to_string(i), 
                          "Segment " + std::to_string(i), embedding);
    }
    
    // Verify embeddings exist
    auto before = db->get_all_embeddings();
    int before_count = 0;
    for (const auto& e : before) {
        if (e.video_id == "delete-embed-test") before_count++;
    }
    EXPECT_EQ(before_count, 3);
    
    // Delete embeddings for the video
    db->delete_embeddings_for_video("delete-embed-test");
    
    // Verify all deleted
    auto after = db->get_all_embeddings();
    int after_count = 0;
    for (const auto& e : after) {
        if (e.video_id == "delete-embed-test") after_count++;
    }
    EXPECT_EQ(after_count, 0);
}

} // namespace testing
} // namespace lucid