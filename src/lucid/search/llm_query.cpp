// llm_query.cpp - LLM-powered query enhancement implementation
#include "llm_query.hpp"
#include "../inference/llm_engine.hpp"
#include "../logging.hpp"

#include <algorithm>
#include <system_error>
#include <filesystem>
#include <regex>

namespace lucid {

LLMQueryEnhancer::LLMQueryEnhancer() = default;
LLMQueryEnhancer::~LLMQueryEnhancer() = default;

void LLMQueryEnhancer::set_engine(LLMEngine* engine) {
    m_engine = engine;
    LUCID_INFO("[LLMQueryEnhancer] Engine set: %s", 
               (engine ? (engine->is_ready() ? "ready" : "not ready") : "null"));
}

LLMQueryResult LLMQueryEnhancer::enhance(const std::string& query) {
    LLMQueryResult result;
    
    // First use rule-based parsing for filters
    result.parsed_query = parse_fallback(query);
    
    // If LLM is available, use it for query enhancement
    if (is_available()) {
        result = enhance_with_llm(query);
        result.parsed_query = parse_fallback(query);  // Keep rule-based parsing for filters
    } else {
        // Use fallback expansion
        result.expanded_terms = expand_keywords(query);
        result.confidence = 0.5f;  // Lower confidence for fallback
        result.error = "LLM not available, using rule-based enhancement";
    }
    
    LUCID_DEBUG("[LLMQueryEnhancer] Enhanced query: '%s' (confidence: %.2f)", 
                query.c_str(), result.confidence);
    
    return result;
}

LLMQueryResult LLMQueryEnhancer::enhance_with_llm(const std::string& query) {
    LLMQueryResult result;
    
    // Create a prompt for query enhancement
    // The LLM will help expand the query with related terms and concepts
    std::string enhancement_prompt = 
        "You are a video search query enhancer. Given a user's search query, "
        "output a list of related search terms that would help find relevant videos. "
        "Output ONLY a comma-separated list of 3-5 additional search terms, nothing else.\n\n"
        "Query: " + query + "\n\n"
        "Related terms:";
    
    // Use LLM engine to generate expansions
    std::vector<ChatMessage> messages = {
        {ChatMessage::Role::SYSTEM, "You are a helpful search assistant that outputs only comma-separated terms."},
        {ChatMessage::Role::USER, enhancement_prompt}
    };
    
    std::string expansion_text;
    bool success = m_engine->generate(messages, 
        [&expansion_text](const LLMResponse& chunk) -> bool {
            expansion_text += chunk.text;
            return true;  // Continue
        },
        &expansion_text);
    
    if (success && !expansion_text.empty()) {
        result.expanded_terms = parse_llm_expansion(expansion_text);
        result.confidence = 0.85f;
        LUCID_DEBUG("[LLMQueryEnhancer] LLM expansion: '%s'", expansion_text.c_str());
    } else {
        // Fallback to rule-based expansion
        result.expanded_terms = expand_keywords(query);
        result.confidence = 0.5f;
        result.error = "LLM generation failed, using fallback";
        LUCID_WARN("[LLMQueryEnhancer] LLM enhancement failed");
    }
    
    return result;
}

std::vector<std::string> LLMQueryEnhancer::parse_llm_expansion(const std::string& llm_response) {
    std::vector<std::string> terms;
    
    if (llm_response.empty()) {
        return terms;
    }
    
    // Split by comma and clean up
    std::stringstream ss(llm_response);
    std::string term;
    while (std::getline(ss, term, ',')) {
        // Trim whitespace
        term.erase(0, term.find_first_not_of(" \t\n\r"));
        term.erase(term.find_last_not_of(" \t\n\r") + 1);
        
        // Remove any extra punctuation at the end
        if (!term.empty() && (term.back() == '.' || term.back() == '!' || term.back() == '?')) {
            term.pop_back();
        }
        
        if (!term.empty() && term.length() > 1) {
            terms.push_back(term);
        }
    }
    
    // Limit to 5 terms max
    if (terms.size() > 5) {
        terms.resize(5);
    }
    
    return terms;
}

ParsedQuery LLMQueryEnhancer::parse_fallback(const std::string& query) {
    QueryParser parser;
    return parser.parse(query);
}

std::vector<std::string> LLMQueryEnhancer::expand_keywords(const std::string& query) {
    std::vector<std::string> expanded;
    
    // Simple rule-based expansion
    std::string lower = query;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    // Common expansions
    std::vector<std::pair<std::string, std::vector<std::string>>> expansions = {
        {"video", {"clip", "footage", "recording"}},
        {"search", {"find", "look for", "locate"}},
        {"how", {"what", "ways to"}},
        {"tutorial", {"how to", "guide", "lesson", "walkthrough"}},
        {"review", {"analysis", "overview", "summary"}},
        {"demo", {"demonstration", "example"}},
        {"python", {"python3", "py"}},
        {"code", {"programming", "coding", "script"}},
        {"linux", {"ubuntu", "debian", "arch", "unix"}},
        {"config", {"configuration", "setup", "settings"}},
        {"install", {"installation", "setup", "deploy"}},
        {"error", {"bug", "issue", "problem", "fix"}},
        {"learn", {"tutorial", "course", "lesson", "education"}},
        {"build", {"compile", "make", "construct"}},
        {"debug", {"troubleshoot", "fix", "investigate"}}
    };
    
    for (const auto& [word, synonyms] : expansions) {
        if (lower.find(word) != std::string::npos) {
            for (const auto& syn : synonyms) {
                expanded.push_back(syn);
            }
        }
    }
    
    // Limit to 5 terms max
    if (expanded.size() > 5) {
        expanded.resize(5);
    }
    
    return expanded;
}

LLMQueryEnhancer::Intent LLMQueryEnhancer::classify_intent(const std::string& query) {
    std::string lower = query;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower.find("find") != std::string::npos || 
        lower.find("search") != std::string::npos ||
        lower.find("show") != std::string::npos) {
        return Intent::SEARCH;
    }
    
    if (lower.find("what") != std::string::npos ||
        lower.find("where") != std::string::npos ||
        lower.find("when") != std::string::npos ||
        lower.find("who") != std::string::npos ||
        lower.find("why") != std::string::npos) {
        return Intent::QUESTION;
    }
    
    if (lower.find("play") != std::string::npos ||
        lower.find("open") != std::string::npos ||
        lower.find("start") != std::string::npos) {
        return Intent::COMMAND;
    }
    
    return Intent::UNKNOWN;
}

} // namespace lucid