// query_parser.cpp - Natural language query parser implementation
#include "query_parser.hpp"
#include "../logging.hpp"
#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace lucid {

QueryParser::QueryParser() = default;
QueryParser::~QueryParser() = default;

ParsedQuery QueryParser::parse(const std::string& query) {
    ParsedQuery result;
    std::string remaining = query;
    
    // Normalize: lowercase for processing
    std::string lower = query;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    LUCID_DEBUG("Parsing query: %s", query.c_str());
    
    // Extract timestamp patterns: "at X minutes", "at X:XX"
    if (auto time = parse_time(lower)) {
        result.timestamp_ms = time;
        remaining = std::regex_replace(remaining, std::regex(R"(\bat\s+\d+\s*(?:min(?:ute)?s?|minutes?)\s*)"), " ");
        remaining = std::regex_replace(remaining, std::regex(R"(\bat\s+\d+:\d+\s*)"), " ");
    }
    
    // Extract duration filters
    // "longer than X minutes", "more than X minutes"
    std::regex longer_regex(R"(longer\s+than\s+(\d+)\s*(?:min(?:ute)?s?|minutes?|hours?|h))");
    std::smatch match;
    if (std::regex_search(lower, match, longer_regex)) {
        int minutes = std::stoi(match[1]);
        if (lower.find("hour") != std::string::npos) {
            minutes *= 60;
        }
        result.min_duration_sec = minutes * 60;
        remaining = std::regex_replace(remaining, longer_regex, " ");
    }
    
    // "shorter than X minutes"
    std::regex shorter_regex(R"(shorter\s+than\s+(\d+)\s*(?:min(?:ute)?s?|minutes?|hours?|h))");
    if (std::regex_search(lower, match, shorter_regex)) {
        int minutes = std::stoi(match[1]);
        if (lower.find("hour") != std::string::npos) {
            minutes *= 60;
        }
        result.max_duration_sec = minutes * 60;
        remaining = std::regex_replace(remaining, shorter_regex, " ");
    }
    
    // "between X and Y" (for time ranges)
    auto [start, end] = parse_range(lower);
    if (start && end) {
        result.start_time_ms = start;
        result.end_time_ms = end;
        // Remove the range pattern
        remaining = std::regex_replace(remaining, std::regex(R"(\bbetween\s+\d+\s*(?:min(?:ute)?s?)?\s+and\s+\d+\s*(?:min(?:ute)?s?)?)"), " ");
    }
    
    // Extract channel: "from channel X", "by X"
    if (auto channel = extract_channel(lower)) {
        result.channel = channel;
        remaining = std::regex_replace(remaining, std::regex(R"(\bfrom\s+channel\s+\w+)"), " ");
        remaining = std::regex_replace(remaining, std::regex(R"(\bby\s+\w+)"), " ");
    }
    
    // Extract keywords in quotes
    result.keywords = extract_keywords(query);
    
    // Sort options
    if (lower.find("newest") != std::string::npos || lower.find("latest") != std::string::npos) {
        result.sort_by = ParsedQuery::SortBy::DATE;
        result.sort_ascending = false;
    } else if (lower.find("oldest") != std::string::npos) {
        result.sort_by = ParsedQuery::SortBy::DATE;
        result.sort_ascending = true;
    } else if (lower.find("longest") != std::string::npos) {
        result.sort_by = ParsedQuery::SortBy::DURATION;
        result.sort_ascending = false;
    } else if (lower.find("shortest") != std::string::npos) {
        result.sort_by = ParsedQuery::SortBy::DURATION;
        result.sort_ascending = true;
    }
    
    // Clean up remaining text
    remaining = std::regex_replace(remaining, std::regex(R"(\s+)"), " ");
    result.main_text = remaining;
    
    // Trim whitespace
    result.main_text.erase(0, result.main_text.find_first_not_of(" \t\n\r"));
    result.main_text.erase(result.main_text.find_last_not_of(" \t\n\r") + 1);
    
    LUCID_DEBUG("Parsed: main='%s', timestamp=%s, channel=%s",
                 result.main_text.c_str(),
                 result.timestamp_ms ? std::to_string(*result.timestamp_ms).c_str() : "none",
                 result.channel ? result.channel->c_str() : "none");
    
    return result;
}

bool QueryParser::is_natural_language(const std::string& query) const {
    std::string lower = query;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    // Natural language indicators
    std::vector<std::string> nl_patterns = {
        "what", "when", "where", "who", "how",
        "find", "show", "search", "look for",
        "tell me", "can you", "i want", "looking for"
    };
    
    for (const auto& pattern : nl_patterns) {
        if (lower.find(pattern) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

std::string QueryParser::extract_semantic_part(const std::string& query) {
    auto parsed = parse(query);
    return parsed.main_text;
}

std::optional<int> QueryParser::parse_time(const std::string& text) {
    // "at 5 minutes"
    std::regex time_regex(R"(at\s+(\d+)\s*(?:min(?:ute)?s?)\s*(?:and\s+(\d+)\s*seconds?)?)");
    std::smatch match;
    if (std::regex_search(text, match, time_regex)) {
        int minutes = std::stoi(match[1]);
        int seconds = match[2].matched ? std::stoi(match[2]) : 0;
        return (minutes * 60 + seconds) * 1000;
    }
    
    // "at 5:30"
    std::regex mm_ss_regex(R"(at\s+(\d+):(\d{2}))");
    if (std::regex_search(text, match, mm_ss_regex)) {
        int minutes = std::stoi(match[1]);
        int seconds = std::stoi(match[2]);
        return (minutes * 60 + seconds) * 1000;
    }
    
    return std::nullopt;
}

int QueryParser::parse_duration_to_seconds(const std::string& text) {
    std::regex min_regex(R"((\d+)\s*min(?:ute)?s?)");
    std::smatch match;
    if (std::regex_search(text, match, min_regex)) {
        return std::stoi(match[1]) * 60;
    }
    
    std::regex hour_regex(R"((\d+)\s*hours?)");
    if (std::regex_search(text, match, hour_regex)) {
        return std::stoi(match[1]) * 3600;
    }
    
    return 0;
}

std::optional<std::string> QueryParser::extract_channel(const std::string& query) {
    std::regex channel_regex(R"(from\s+channel\s+([a-zA-Z0-9_]+))");
    std::smatch match;
    if (std::regex_search(query, match, channel_regex)) {
        return match[1].str();
    }
    
    // "by channelname"
    std::regex by_regex(R"(by\s+([a-zA-Z0-9_]+))");
    if (std::regex_search(query, match, by_regex)) {
        return match[1].str();
    }
    
    return std::nullopt;
}

std::vector<std::string> QueryParser::extract_keywords(const std::string& query) {
    std::vector<std::string> keywords;
    
    // Extract quoted strings
    std::regex quote_regex(R"xx("([^"]+)")xx");
    std::smatch match;
    std::string::const_iterator search_start = query.cbegin();
    while (std::regex_search(search_start, query.cend(), match, quote_regex)) {
        keywords.push_back(match[1].str());
        search_start = match.suffix().first;
    }
    
    return keywords;
}

std::pair<std::optional<int>, std::optional<int>> QueryParser::parse_range(const std::string& query) {
    std::pair<std::optional<int>, std::optional<int>> result{std::nullopt, std::nullopt};
    
    // "between 2 and 5 minutes"
    std::regex range_regex(R"(between\s+(\d+)\s*(?:min(?:ute)?s?)?\s+and\s+(\d+)\s*(?:min(?:ute)?s?)?)");
    std::smatch match;
    if (std::regex_search(query, match, range_regex)) {
        int start = std::stoi(match[1]);
        int end = std::stoi(match[2]);
        result.first = start * 60 * 1000;
        result.second = end * 60 * 1000;
    }
    
    return result;
}

} // namespace lucid