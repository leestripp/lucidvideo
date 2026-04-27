// query_parser.hpp - Natural language query parser for video search
#pragma once

#include <string>
#include <vector>
#include <optional>

namespace lucid {

// Parsed query components
struct ParsedQuery {
    // Main search text (semantic query)
    std::string main_text;
    
    // Filters
    std::optional<int> timestamp_ms;           // "at 5 minutes"
    std::optional<int> min_duration_sec;       // "videos longer than 10 minutes"
    std::optional<int> max_duration_sec;        // "shorter than 5 minutes"
    std::optional<std::string> channel;         // "from channel X"
    std::vector<std::string> keywords;          // Exact keyword matches
    
    // Time range
    std::optional<int> start_time_ms;          // "between 2 and 5 minutes"
    std::optional<int> end_time_ms;
    
    // Sort options
    enum class SortBy { RELEVANCE, DATE, DURATION, TIMESTAMP };
    SortBy sort_by = SortBy::RELEVANCE;
    bool sort_ascending = false;
};

class QueryParser {
public:
    QueryParser();
    ~QueryParser();
    
    // Parse natural language query into structured query
    ParsedQuery parse(const std::string& query);
    
    // Check if query looks like a natural language question
    bool is_natural_language(const std::string& query) const;
    
    // Extract just the semantic part (remove filters)
    std::string extract_semantic_part(const std::string& query);
    
private:
    // Time patterns
    static std::optional<int> parse_time(const std::string& text);
    static int parse_duration_to_seconds(const std::string& text);
    
    // Channel patterns
    static std::optional<std::string> extract_channel(const std::string& query);
    
    // Keyword patterns
    static std::vector<std::string> extract_keywords(const std::string& query);
    
    // Range patterns ("between X and Y")
    static std::pair<std::optional<int>, std::optional<int>> parse_range(const std::string& query);
};

} // namespace lucid