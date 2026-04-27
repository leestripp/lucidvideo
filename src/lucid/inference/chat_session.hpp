// chat_session.hpp - Chat Session Management
//
// Manages conversation history and LLM interactions

#pragma once

#include "llm_engine.hpp"
#include <vector>
#include <string>
#include <functional>

namespace lucid {

// ============================================================================
// Chat Session
// ============================================================================

class ChatSession {
public:
    ChatSession();
    ~ChatSession();
    
    // Initialize with LLM engine
    void set_engine(LLMEngine* engine);
    
    // Session management
    void clear();
    bool is_active() const { return m_active; }
    
    // Message management
    void add_user_message(const std::string& content);
    void add_assistant_message(const std::string& content);
    void set_system_message(const std::string& content);
    
    const std::vector<ChatMessage>& get_messages() const { return m_messages; }
    const std::string& get_system_prompt() const { return m_system_prompt; }
    
    // Generation
    using TokenCallback = std::function<bool(const std::string& token)>;
    using CompleteCallback = std::function<void(const std::string& response, bool success)>;
    
    // Stream response to callback
    void generate_response(TokenCallback on_token);
    
    // Context-aware response generation
    // The context string is prepended to the system prompt
    void generate_with_context(const std::string& context, TokenCallback on_token);
    
    // Async generation with callback on complete
    void generate_response_async(CompleteCallback on_complete);
    
    // Stop ongoing generation
    void stop();
    
    // Settings
    void set_max_history(int max_messages) { m_max_history = max_messages; }
    int get_max_history() const { return m_max_history; }

private:
    void trim_history();
    std::vector<ChatMessage> build_context() const;
    std::vector<ChatMessage> build_context_with_extra(const std::string& extra_context) const;

    // LLM engine
    LLMEngine* m_engine = nullptr;
    
    // Conversation history
    std::vector<ChatMessage> m_messages;
    std::string m_system_prompt;
    
    // Extra context for context-aware generation
    std::string m_extra_context;
    
    // State
    bool m_active = false;
    int m_max_history = 50;  // Max user/assistant pairs to keep
};

} // namespace lucid
