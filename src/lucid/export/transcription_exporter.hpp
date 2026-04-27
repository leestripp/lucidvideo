// transcription_exporter.hpp - Lucid Transcription Exporter
// 
// Exports transcription segments to standard subtitle formats:
// - SRT (SubRip)
// - VTT (WebVTT)
// - TXT (Plain text)

#pragma once

#include <string>
#include <vector>
#include <optional>

namespace lucid {

// Transcription segment with timestamps
struct ExportSegment {
    int64_t start_ms;
    int64_t end_ms;
    std::string text;
};

// Export format types
enum class ExportFormat {
    SRT,  // SubRip format
    VTT,  // WebVTT format
    TXT   // Plain text (no timestamps)
};

// TranscriptionExporter - converts transcription segments to subtitle formats
class TranscriptionExporter {
public:
    TranscriptionExporter() = default;
    ~TranscriptionExporter() = default;

    // Export transcription to a file in the specified format
    // Returns true on success, false on failure
    bool export_to_file(const std::string& filepath,
                        ExportFormat format,
                        const std::vector<ExportSegment>& segments,
                        const std::string& title = "Transcription");

    // Convert transcription to string (for in-memory use)
    std::string to_srt(const std::vector<ExportSegment>& segments);
    std::string to_vtt(const std::vector<ExportSegment>& segments);
    std::string to_plain_text(const std::vector<ExportSegment>& segments);

    // Format a single segment
    static std::string format_srt_segment(int index, const ExportSegment& segment);
    static std::string format_vtt_segment(const ExportSegment& segment);

private:
    // Helper: convert milliseconds to SRT time format (HH:MM:SS,mmm)
    static std::string ms_to_srt_time(int64_t ms);
    
    // Helper: convert milliseconds to VTT time format (HH:MM:SS.mmm)
    static std::string ms_to_vtt_time(int64_t ms);

    // Helper: escape HTML entities for VTT
    static std::string escape_vtt_text(const std::string& text);
    
    // Helper: write string to file
    bool write_file(const std::string& filepath, const std::string& content);
};

} // namespace lucid