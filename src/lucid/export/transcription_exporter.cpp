// transcription_exporter.cpp - Lucid Transcription Exporter Implementation
//
// Converts transcription segments to standard subtitle formats:
// - SRT (SubRip): Most widely supported, used by video players
// - VTT (WebVTT): HTML5 standard, supports styling
// - TXT (Plain text): Just the transcript without timestamps

#include "transcription_exporter.hpp"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cmath>

namespace lucid {

// ============================================================================
// Public Methods
// ============================================================================

bool TranscriptionExporter::export_to_file(const std::string& filepath,
                                            ExportFormat format,
                                            const std::vector<ExportSegment>& segments,
                                            const std::string& title) {
    if (segments.empty()) {
        std::cerr << "[TranscriptionExporter] No segments to export" << std::endl;
        return false;
    }

    std::string content;
    switch (format) {
        case ExportFormat::SRT:
            content = to_srt(segments);
            break;
        case ExportFormat::VTT:
            content = to_vtt(segments);
            break;
        case ExportFormat::TXT:
            content = to_plain_text(segments);
            break;
    }

    bool success = write_file(filepath, content);
    if (success) {
        std::cout << "[TranscriptionExporter] Exported " << segments.size() << " segments to " << filepath << std::endl;
    }

    return success;
}

std::string TranscriptionExporter::to_srt(const std::vector<ExportSegment>& segments) {
    std::stringstream ss;
    
    for (size_t i = 0; i < segments.size(); ++i) {
        const auto& segment = segments[i];
        
        // SRT segment with 1-based index
        ss << format_srt_segment(static_cast<int>(i + 1), segment);
        
        // Blank line between segments
        if (i < segments.size() - 1) {
            ss << "\n";
        }
    }
    
    return ss.str();
}

std::string TranscriptionExporter::to_vtt(const std::vector<ExportSegment>& segments) {
    std::stringstream ss;
    
    // WebVTT header
    ss << "WEBVTT\n\n";
    
    for (size_t i = 0; i < segments.size(); ++i) {
        const auto& segment = segments[i];
        
        // VTT segment (no index, just timestamps and text)
        ss << format_vtt_segment(segment);
        
        // Blank line between segments
        if (i < segments.size() - 1) {
            ss << "\n";
        }
    }
    
    return ss.str();
}

std::string TranscriptionExporter::to_plain_text(const std::vector<ExportSegment>& segments) {
    std::stringstream ss;
    
    for (const auto& segment : segments) {
        // Trim whitespace and add sentence
        std::string text = segment.text;
        
        // Remove leading/trailing whitespace
        while (!text.empty() && std::isspace(text.front())) {
            text.erase(text.begin());
        }
        while (!text.empty() && std::isspace(text.back())) {
            text.pop_back();
        }
        
        // Skip empty segments
        if (text.empty()) {
            continue;
        }
        
        // Add sentence with space
        ss << text << " ";
    }
    
    std::string result = ss.str();
    
    // Trim trailing space
    while (!result.empty() && std::isspace(result.back())) {
        result.pop_back();
    }
    
    return result;
}

// ============================================================================
// Format Helpers
// ============================================================================

std::string TranscriptionExporter::format_srt_segment(int index, const ExportSegment& segment) {
    std::stringstream ss;
    
    // SRT index (1-based)
    ss << index << "\n";
    
    // SRT timestamps: HH:MM:SS,mmm --> HH:MM:SS,mmm
    ss << ms_to_srt_time(segment.start_ms) << " --> " << ms_to_srt_time(segment.end_ms) << "\n";
    
    // Text content (trimmed)
    std::string text = segment.text;
    while (!text.empty() && std::isspace(text.front())) {
        text.erase(text.begin());
    }
    while (!text.empty() && std::isspace(text.back())) {
        text.pop_back();
    }
    ss << text;
    
    return ss.str();
}

std::string TranscriptionExporter::format_vtt_segment(const ExportSegment& segment) {
    std::stringstream ss;
    
    // VTT timestamps: HH:MM:SS.mmm --> HH:MM:SS.mmm
    ss << ms_to_vtt_time(segment.start_ms) << " --> " << ms_to_vtt_time(segment.end_ms) << "\n";
    
    // Text content (trimmed, with HTML entities escaped)
    std::string text = segment.text;
    while (!text.empty() && std::isspace(text.front())) {
        text.erase(text.begin());
    }
    while (!text.empty() && std::isspace(text.back())) {
        text.pop_back();
    }
    ss << escape_vtt_text(text);
    
    return ss.str();
}

// ============================================================================
// Private Helpers
// ============================================================================

std::string TranscriptionExporter::ms_to_srt_time(int64_t ms) {
    int64_t total_seconds = ms / 1000;
    int64_t hours = total_seconds / 3600;
    int64_t minutes = (total_seconds % 3600) / 60;
    int64_t seconds = total_seconds % 60;
    int64_t millis = ms % 1000;
    
    std::stringstream ss;
    ss << std::setfill('0')
       << std::setw(2) << hours << ":"
       << std::setw(2) << minutes << ":"
       << std::setw(2) << seconds << ","
       << std::setw(3) << millis;
    
    return ss.str();
}

std::string TranscriptionExporter::ms_to_vtt_time(int64_t ms) {
    int64_t total_seconds = ms / 1000;
    int64_t hours = total_seconds / 3600;
    int64_t minutes = (total_seconds % 3600) / 60;
    int64_t seconds = total_seconds % 60;
    int64_t millis = ms % 1000;
    
    std::stringstream ss;
    ss << std::setfill('0')
       << std::setw(2) << hours << ":"
       << std::setw(2) << minutes << ":"
       << std::setw(2) << seconds << "."
       << std::setw(3) << millis;
    
    return ss.str();
}

std::string TranscriptionExporter::escape_vtt_text(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    
    for (char c : text) {
        switch (c) {
            case '&':
                result += "&amp;";
                break;
            case '<':
                result += "&lt;";
                break;
            case '>':
                result += "&gt;";
                break;
            case '"':
                result += "&quot;";
                break;
            case '\n':
                result += "<br>";
                break;
            default:
                result += c;
                break;
        }
    }
    
    return result;
}

bool TranscriptionExporter::write_file(const std::string& filepath, const std::string& content) {
    std::ofstream file(filepath);
    
    if (!file.is_open()) {
        std::cerr << "[TranscriptionExporter] Failed to open file for writing: " << filepath << std::endl;
        return false;
    }
    
    file << content;
    
    if (file.fail()) {
        std::cerr << "[TranscriptionExporter] Failed to write to file: " << filepath << std::endl;
        return false;
    }
    
    file.close();
    return true;
}

} // namespace lucid
