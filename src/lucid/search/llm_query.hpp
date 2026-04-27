// llm_query.hpp - LLM-powered query enhancement
#pragma once

#include "query_parser.hpp"
#include "../inference/llm_engine.hpp"
#include <string>
#include <optional>

namespace lucid {

// LLM-enhanced query with context
struct LLMQueryResult {
    // Structured query from LLM parsing
    ParsedQuery parsed_query;
    
    // Expanded query terms (LLM adds related terms)
    std::vector<std::string> expanded_terms;
    
    // Confidence score (0.0 to 1.0)
    float confidence = 0.0f;
    
    // Error message if LLM failed
    std::optional<std::string> error;
};

class LLMQueryEnhancer {
public:
    LLMQueryEnhancer();
    ~LLMQueryEnhancer();
    
    // Set the LLM engine to use for enhancement
    void set_engine(LLMEngine* engine);
    
    // Check if LLM is available
    bool is_available() const { return m_engine != nullptr && m_engine->is_ready(); }
    
    // Enhance a query using LLM
    LLMQueryResult enhance(const std::string& query);
    
    // Fallback to rule-based parsing if LLM unavailable
    ParsedQuery parse_fallback(const std::string& query);

private:
    // Use LLM to parse and enhance query
    LLMQueryResult enhance_with_llm(const std::string& query);
    
    // Parse LLM response to extract structured query info
    std::vector<std::string> parse_llm_expansion(const std::string& llm_response);
    
    // Simple keyword expansion (fallback)
    std::vector<std::string> expand_keywords(const std::string& query);
    
    // Query intent classification
    enum class Intent { SEARCH, QUESTION, COMMAND, UNKNOWN };
    Intent classify_intent(const std::string& query);
    
    LLMEngine* m_engine = nullptr;
};

} // namespace lucid