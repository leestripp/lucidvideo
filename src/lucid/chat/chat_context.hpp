// chat_context.hpp - Chat Context Helper
// Bridges Chat with SemanticSearch and Database
// Provides video library context for LLM responses

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>

// Full include for SearchResult (used in method signatures)
#include "../model/database.hpp"

namespace lucid {

class Database;
class SemanticSearch;

// ============================================================================
// ChatContext
// ============================================================================
// Provides video library context for chat interactions

class ChatContext {
public:
    explicit ChatContext(Database* db, SemanticSearch* search);
    ~ChatContext();

    // Get library statistics formatted for prompts
    std::string get_library_stats() const;

    // Get recent videos formatted for prompts
    std::string get_recent_videos(int max_count = 5) const;

    // Get videos relevant to a query (semantic search)
    std::string get_relevant_videos(const std::string& query, int max_count = 5) const;

    // Format search results as readable text
    std::string format_search_results(const std::vector<SearchResult>& results) const;

    // Get topics/keywords from the library
    std::string get_top_topics(int max_count = 10) const;

    // Build full context for a user query
    // Includes library stats, recent videos, and semantic search results
    std::string build_query_context(const std::string& user_query) const;

    // Check if context is available (db and search initialized)
    bool is_available() const { return m_db != nullptr && m_search != nullptr; }

private:
    // Get video title by ID
    std::optional<std::string> get_video_title(const std::string& video_id) const;

    // Format duration for display
    std::string format_duration(int seconds) const;

    // Truncate text with ellipsis
    std::string truncate(const std::string& text, int max_len) const;

    Database* m_db = nullptr;
    SemanticSearch* m_search = nullptr;
};

} // namespace lucid