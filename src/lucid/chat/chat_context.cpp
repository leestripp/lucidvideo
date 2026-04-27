// chat_context.cpp - Chat Context Helper Implementation

#include "chat_context.hpp"
#include "../model/database.hpp"
#include "../search/semantic_search.hpp"
#include "../logging.hpp"

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <map>

namespace lucid {

ChatContext::ChatContext(Database* db, SemanticSearch* search)
    : m_db(db)
    , m_search(search)
{
}

ChatContext::~ChatContext() = default;

std::string ChatContext::get_library_stats() const {
    if (!m_db) return "";

    auto videos = m_db->get_all_videos();

    std::ostringstream oss;
    oss << "Your video library contains:\n";
    oss << "- " << videos.size() << " video" << (videos.size() == 1 ? "" : "s") << "\n";

    // Count videos by status (using actual enum values)
    int completed = 0, analyzing = 0, failed = 0, pending = 0;
    for (const auto& v : videos) {
        switch (v.status) {
            case VideoStatus::COMPLETED: completed++; break;
            case VideoStatus::ANALYZING:
            case VideoStatus::TRANSCRIBING:
            case VideoStatus::THUMBNAILING:
            case VideoStatus::VISION_SCANNING:
                analyzing++; break;
            case VideoStatus::FAILED: failed++; break;
            case VideoStatus::PENDING:
            case VideoStatus::STALE:
            default: pending++; break;
        }
    }
    
    if (completed > 0) {
        oss << "- " << completed << " video" << (completed == 1 ? "" : "s") << " indexed\n";
    }
    if (analyzing > 0) {
        oss << "- " << analyzing << " video" << (analyzing == 1 ? "" : "s") << " being analyzed\n";
    }
    if (pending > 0) {
        oss << "- " << pending << " video" << (pending == 1 ? "" : "s") << " pending\n";
    }

    return oss.str();
}

std::string ChatContext::get_recent_videos(int max_count) const {
    if (!m_db) return "";

    auto videos = m_db->get_all_videos();
    if (videos.empty()) return "";

    // Sort by updated_at descending (most recent first)
    std::sort(videos.begin(), videos.end(), [](const VideoEntry& a, const VideoEntry& b) {
        return a.updated_at > b.updated_at;
    });

    std::ostringstream oss;
    int count = std::min(max_count, static_cast<int>(videos.size()));
    
    oss << "Recent videos:\n";
    for (int i = 0; i < count; i++) {
        const auto& v = videos[i];
        oss << "  " << (i + 1) << ". " << truncate(v.title, 60) << " (" << format_duration(static_cast<int>(v.duration_ms / 1000)) << ")\n";
    }

    return oss.str();
}

std::string ChatContext::get_relevant_videos(const std::string& query, int max_count) const {
    if (!m_search || query.empty()) return "";

    // Do semantic search
    auto results = m_search->search_text(query, max_count);
    if (results.empty()) return "";

    return format_search_results(results);
}

std::string ChatContext::format_search_results(const std::vector<SearchResult>& results) const {
    if (results.empty()) return "";

    std::ostringstream oss;
    oss << "Relevant videos found:\n";

    // Group by video to avoid duplicates
    std::map<std::string, std::vector<const SearchResult*>> by_video;
    for (const auto& r : results) {
        by_video[r.video_id].push_back(&r);
    }

    int idx = 1;
    for (const auto& [video_id, matches] : by_video) {
        if (idx > 5) break;  // Limit to 5 videos

        // Get best match for this video
        const SearchResult* best = nullptr;
        double best_score = 0.0;
        std::string best_snippet;
        
        for (const auto* m : matches) {
            if (m->relevance_score > best_score) {
                best_score = m->relevance_score;
                best = m;
                best_snippet = m->snippet;
            }
        }

        if (best) {
            oss << "  " << idx << ". " << truncate(best->title, 50);
            if (best->timestamp_ms > 0) {
                oss << " @ " << format_duration(best->timestamp_ms / 1000);
            }
            oss << "\n";
            
            // Add snippet if available and relevant
            if (!best_snippet.empty() && best_snippet.length() > 10) {
                oss << "     \"" << truncate(best_snippet, 80) << "\"\n";
            }
            idx++;
        }
    }

    return oss.str();
}

std::string ChatContext::get_top_topics(int max_count) const {
    if (!m_db) return "";

    // Get all markers to extract topics
    auto videos = m_db->get_all_videos();
    
    std::map<std::string, int> word_counts;
    const int min_word_length = 4;  // Skip short words
    const int max_word_length = 20;
    
    for (const auto& video : videos) {
        // Count words in title
        std::istringstream title_stream(video.title);
        std::string word;
        while (title_stream >> word) {
            // Clean word
            word.erase(std::remove_if(word.begin(), word.end(), 
                [](char c) { return std::ispunct(c) && c != '_'; }), word.end());
            
            if (word.length() >= min_word_length && word.length() <= max_word_length) {
                // Skip common stop words
                std::string lower = word;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                
                static const std::vector<std::string> stop_words = {
                    "about", "after", "also", "another", "around", "because",
                    "been", "before", "being", "between", "both", "could",
                    "does", "doing", "during", "each", "even", "every",
                    "found", "from", "have", "having", "here", "into",
                    "just", "know", "like", "made", "make", "many", "more",
                    "most", "much", "must", "need", "only", "other", "over",
                    "said", "same", "should", "since", "some", "such",
                    "than", "that", "their", "them", "then", "there",
                    "these", "they", "this", "those", "through", "under",
                    "video", "videos", "what", "when", "where", "which",
                    "while", "will", "with", "would", "your"
                };
                
                bool is_stop = false;
                for (const auto& sw : stop_words) {
                    if (lower == sw) { is_stop = true; break; }
                }
                
                if (!is_stop) {
                    word_counts[word]++;
                }
            }
        }
    }

    if (word_counts.empty()) return "";

    // Sort by count descending
    std::vector<std::pair<std::string, int>> sorted(word_counts.begin(), word_counts.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    std::ostringstream oss;
    oss << "Topics in your library: ";
    
    int count = std::min(max_count, static_cast<int>(sorted.size()));
    for (int i = 0; i < count; i++) {
        if (i > 0) oss << ", ";
        oss << sorted[i].first << " (" << sorted[i].second << ")";
    }

    return oss.str();
}

std::string ChatContext::build_query_context(const std::string& user_query) const {
    std::ostringstream context;

    // Library stats
    context << get_library_stats() << "\n";

    // Top topics
    std::string topics = get_top_topics(10);
    if (!topics.empty()) {
        context << topics << "\n\n";
    }

    // Check if query seems to need video search
    // Look for natural language indicators
    std::string lower = user_query;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    std::vector<std::string> search_indicators = {
        "show", "find", "search", "look", "videos about", "videos on",
        "tell me about", "what videos", "do i have", "do you have",
        "related", "similar", "like", "more like"
    };
    
    bool needs_search = false;
    for (const auto& indicator : search_indicators) {
        if (lower.find(indicator) != std::string::npos) {
            needs_search = true;
            break;
        }
    }

    if (needs_search) {
        // Extract the search topic from the query
        std::string search_topic = user_query;
        
        // Remove common prefixes
        static const std::vector<std::string> prefixes = {
            "show me ", "find me ", "search for ", "search ",
            "videos about ", "videos on ", "look for ", "look up ",
            "tell me about ", "what videos about ", "do i have any ",
            "do you have any ", "videos ", ""
        };
        
        for (const auto& prefix : prefixes) {
            if (search_topic.length() > prefix.length() &&
                search_topic.substr(0, prefix.length()) == prefix) {
                search_topic = search_topic.substr(prefix.length());
                break;
            }
        }
        
        // Trim and remove trailing punctuation
        while (!search_topic.empty() && 
               (search_topic.back() == '?' || search_topic.back() == '.')) {
            search_topic.pop_back();
        }
        
        if (!search_topic.empty() && search_topic.length() > 2) {
            std::string relevant = get_relevant_videos(search_topic, 5);
            if (!relevant.empty()) {
                context << relevant << "\n";
            }
        }
    }

    // Recent videos as fallback context
    if (!needs_search) {
        context << get_recent_videos(3) << "\n";
    }

    return context.str();
}

std::optional<std::string> ChatContext::get_video_title(const std::string& video_id) const {
    if (!m_db) return std::nullopt;

    auto videos = m_db->get_all_videos();
    for (const auto& v : videos) {
        if (v.id == video_id) {
            return v.title;
        }
    }
    return std::nullopt;
}

std::string ChatContext::format_duration(int seconds) const {
    if (seconds <= 0) return "0:00";

    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;

    if (hours > 0) {
        return std::to_string(hours) + ":" + 
               (minutes < 10 ? "0" : "") + std::to_string(minutes) + ":" +
               (secs < 10 ? "0" : "") + std::to_string(secs);
    }
    return std::to_string(minutes) + ":" + (secs < 10 ? "0" : "") + std::to_string(secs);
}

std::string ChatContext::truncate(const std::string& text, int max_len) const {
    if (text.length() <= static_cast<size_t>(max_len)) {
        return text;
    }
    return text.substr(0, max_len - 3) + "...";
}

} // namespace lucid