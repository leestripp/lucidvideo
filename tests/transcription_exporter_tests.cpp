// transcription_exporter_tests.cpp - Unit tests for TranscriptionExporter
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

// Include from src/lucid/export/
#include "lucid/export/transcription_exporter.hpp"

using namespace lucid;

// ============================================================================
// Test Fixtures
// ============================================================================

class TranscriptionExporterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use temp directory for test files
        m_temp_dir = std::filesystem::temp_directory_path() / "lucid_test_export";
        std::filesystem::create_directory(m_temp_dir);
    }

    void TearDown() override {
        // Clean up test files
        std::filesystem::remove_all(m_temp_dir);
    }

    std::filesystem::path m_temp_dir;

    // Helper to create segments
    std::vector<ExportSegment> create_segments(const std::vector<std::tuple<int64_t, int64_t, std::string>>& data) {
        std::vector<ExportSegment> segments;
        for (const auto& [start, end, text] : data) {
            segments.push_back({start, end, text});
        }
        return segments;
    }

    // Helper to read file content
    std::string read_file(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return "";
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }
};

// ============================================================================
// SRT Format Tests
// ============================================================================

TEST_F(TranscriptionExporterTest, SRTFormatSingleSegment) {
    TranscriptionExporter exporter;
    auto segments = create_segments({
        {1000, 3500, "Hello, welcome to this video."}
    });

    std::string srt = exporter.to_srt(segments);

    EXPECT_NE(srt.find("1\n"), std::string::npos);
    EXPECT_NE(srt.find("00:00:01,000 --> 00:00:03,500\n"), std::string::npos);
    EXPECT_NE(srt.find("Hello, welcome to this video."), std::string::npos);
}

TEST_F(TranscriptionExporterTest, SRTFormatMultipleSegments) {
    TranscriptionExporter exporter;
    auto segments = create_segments({
        {1000, 3500, "First segment."},
        {4000, 7200, "Second segment."}
    });

    std::string srt = exporter.to_srt(segments);

    // Should have both segments
    EXPECT_NE(srt.find("1\n"), std::string::npos);
    EXPECT_NE(srt.find("2\n"), std::string::npos);
    EXPECT_NE(srt.find("First segment."), std::string::npos);
    EXPECT_NE(srt.find("Second segment."), std::string::npos);
}

TEST_F(TranscriptionExporterTest, SRTTimestampConversion) {
    TranscriptionExporter exporter;
    auto segments = create_segments({
        {3661001, 3661500, "Test"}  // 1:01:01.001
    });

    std::string srt = exporter.to_srt(segments);

    // Should have correct timestamp format HH:MM:SS,mmm
    EXPECT_NE(srt.find("01:01:01,001"), std::string::npos);
}

TEST_F(TranscriptionExporterTest, SRTEscapeNewlines) {
    TranscriptionExporter exporter;
    auto segments = create_segments({
        {1000, 3500, "Line one.\nLine two."}
    });

    std::string srt = exporter.to_srt(segments);

    // SRT uses \n as is for multi-line
    EXPECT_NE(srt.find("Line one.\nLine two."), std::string::npos);
}

// ============================================================================
// VTT Format Tests
// ============================================================================

TEST_F(TranscriptionExporterTest, VTTFormatSingleSegment) {
    TranscriptionExporter exporter;
    auto segments = create_segments({
        {1000, 3500, "Hello, welcome to this video."}
    });

    std::string vtt = exporter.to_vtt(segments);

    // VTT should have header
    EXPECT_NE(vtt.find("WEBVTT\n\n"), std::string::npos);
    EXPECT_NE(vtt.find("00:00:01.000 --> 00:00:03.500\n"), std::string::npos);
    EXPECT_NE(vtt.find("Hello, welcome to this video."), std::string::npos);
}

TEST_F(TranscriptionExporterTest, VTTFormatMultipleSegments) {
    TranscriptionExporter exporter;
    auto segments = create_segments({
        {1000, 3500, "First."},
        {4000, 7200, "Second."}
    });

    std::string vtt = exporter.to_vtt(segments);

    // Should have both segments
    EXPECT_NE(vtt.find("First."), std::string::npos);
    EXPECT_NE(vtt.find("Second."), std::string::npos);
}

TEST_F(TranscriptionExporterTest, VTTTimestampUsesDot) {
    TranscriptionExporter exporter;
    auto segments = create_segments({
        {1000, 3500, "Test"}
    });

    std::string vtt = exporter.to_vtt(segments);

    // VTT uses dot separator: HH:MM:SS.mmm (not comma like SRT)
    EXPECT_NE(vtt.find("00:00:01.000"), std::string::npos);
}

TEST_F(TranscriptionExporterTest, VTTEscapesHTMLEntities) {
    TranscriptionExporter exporter;
    auto segments = create_segments({
        {1000, 3500, "Less than < and greater than >"}
    });

    std::string vtt = exporter.to_vtt(segments);

    // HTML entities should be escaped
    EXPECT_NE(vtt.find("&lt;"), std::string::npos);
    EXPECT_NE(vtt.find("&gt;"), std::string::npos);
}

// ============================================================================
// Plain Text Tests
// ============================================================================

TEST_F(TranscriptionExporterTest, PlainTextSingleSegment) {
    TranscriptionExporter exporter;
    auto segments = create_segments({
        {1000, 3500, "Hello, welcome to this video."}
    });

    std::string txt = exporter.to_plain_text(segments);

    EXPECT_EQ(txt, "Hello, welcome to this video.");
}

TEST_F(TranscriptionExporterTest, PlainTextMultipleSegments) {
    TranscriptionExporter exporter;
    auto segments = create_segments({
        {1000, 3500, "First sentence."},
        {4000, 7200, "Second sentence."}
    });

    std::string txt = exporter.to_plain_text(segments);

    EXPECT_EQ(txt, "First sentence. Second sentence.");
}

TEST_F(TranscriptionExporterTest, PlainTextSkipsEmptySegments) {
    TranscriptionExporter exporter;
    auto segments = create_segments({
        {1000, 2000, "Valid text."},
        {3000, 4000, ""},  // Empty segment
        {5000, 6000, "More text."}
    });

    std::string txt = exporter.to_plain_text(segments);

    EXPECT_EQ(txt, "Valid text. More text.");
}

TEST_F(TranscriptionExporterTest, PlainTextTrimsWhitespace) {
    TranscriptionExporter exporter;
    auto segments = create_segments({
        {1000, 3500, "  Leading spaces."},
        {4000, 7200, "Trailing spaces.  "}
    });

    std::string txt = exporter.to_plain_text(segments);

    EXPECT_EQ(txt, "Leading spaces. Trailing spaces.");
}

// ============================================================================
// File Export Tests
// ============================================================================

TEST_F(TranscriptionExporterTest, ExportToFileSRT) {
    TranscriptionExporter exporter;
    auto segments = create_segments({
        {1000, 3500, "Test export."}
    });

    auto filepath = m_temp_dir / "test.srt";
    bool result = exporter.export_to_file(filepath.string(), ExportFormat::SRT, segments, "Test");

    EXPECT_TRUE(result);
    EXPECT_TRUE(std::filesystem::exists(filepath));

    std::string content = read_file(filepath.string());
    EXPECT_NE(content.find("Test export."), std::string::npos);
}

TEST_F(TranscriptionExporterTest, ExportToFileVTT) {
    TranscriptionExporter exporter;
    auto segments = create_segments({
        {1000, 3500, "Test export."}
    });

    auto filepath = m_temp_dir / "test.vtt";
    bool result = exporter.export_to_file(filepath.string(), ExportFormat::VTT, segments, "Test");

    EXPECT_TRUE(result);
    EXPECT_TRUE(std::filesystem::exists(filepath));

    std::string content = read_file(filepath.string());
    EXPECT_NE(content.find("WEBVTT"), std::string::npos);
    EXPECT_NE(content.find("Test export."), std::string::npos);
}

TEST_F(TranscriptionExporterTest, ExportToFileTXT) {
    TranscriptionExporter exporter;
    auto segments = create_segments({
        {1000, 3500, "Test export."}
    });

    auto filepath = m_temp_dir / "test.txt";
    bool result = exporter.export_to_file(filepath.string(), ExportFormat::TXT, segments, "Test");

    EXPECT_TRUE(result);
    EXPECT_TRUE(std::filesystem::exists(filepath));

    std::string content = read_file(filepath.string());
    EXPECT_EQ(content, "Test export.");
}

TEST_F(TranscriptionExporterTest, ExportEmptySegmentsFails) {
    TranscriptionExporter exporter;
    auto segments = create_segments({});  // Empty

    auto filepath = m_temp_dir / "empty.srt";
    bool result = exporter.export_to_file(filepath.string(), ExportFormat::SRT, segments, "Test");

    EXPECT_FALSE(result);
}

TEST_F(TranscriptionExporterTest, ExportNonexistentDirectoryFails) {
    TranscriptionExporter exporter;
    auto segments = create_segments({
        {1000, 3500, "Test."}
    });

    auto filepath = "/nonexistent/directory/test.srt";
    bool result = exporter.export_to_file(filepath, ExportFormat::SRT, segments, "Test");

    EXPECT_FALSE(result);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(TranscriptionExporterTest, VeryLongSegment) {
    TranscriptionExporter exporter;
    std::string long_text(1000, 'x');  // 1000 character text
    auto segments = create_segments({
        {0, 100000, long_text}
    });

    std::string srt = exporter.to_srt(segments);
    EXPECT_NE(srt.find(long_text), std::string::npos);
}

TEST_F(TranscriptionExporterTest, SegmentAtZero) {
    TranscriptionExporter exporter;
    auto segments = create_segments({
        {0, 1000, "Starts at beginning."}
    });

    std::string vtt = exporter.to_vtt(segments);
    EXPECT_NE(vtt.find("00:00:00.000"), std::string::npos);
}

TEST_F(TranscriptionExporterTest, SegmentAtEnd) {
    TranscriptionExporter exporter;
    // 2 hours, 59 minutes, 59 seconds
    auto segments = create_segments({
        {10799000, 10800000, "Near end."}
    });

    std::string srt = exporter.to_srt(segments);
    EXPECT_NE(srt.find("02:59:59,000"), std::string::npos);
}

TEST_F(TranscriptionExporterTest, SRTAndVTTProduceDifferentOutput) {
    TranscriptionExporter exporter;
    auto segments = create_segments({
        {1000, 3500, "Test"}
    });

    std::string srt = exporter.to_srt(segments);
    std::string vtt = exporter.to_vtt(segments);

    // Should be different (different headers, different timestamp formats)
    EXPECT_NE(srt, vtt);
    EXPECT_NE(srt.find("1\n"), std::string::npos);
    EXPECT_EQ(vtt.find("\n1\n"), std::string::npos);  // VTT has no index
}

TEST_F(TranscriptionExporterTest, SpecialCharactersInText) {
    TranscriptionExporter exporter;
    auto segments = create_segments({
        {1000, 3500, "Quote: \"test\" and ampersand: &"}
    });

    std::string srt = exporter.to_srt(segments);
    std::string vtt = exporter.to_vtt(segments);

    // SRT keeps quotes, VTT escapes ampersand
    EXPECT_NE(srt.find("Quote: \"test\""), std::string::npos);
    EXPECT_NE(vtt.find("&amp;"), std::string::npos);
}