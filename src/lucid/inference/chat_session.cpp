// chat_session.cpp - Chat Session Implementation

#include "chat_session.hpp"
#include "../logging.hpp"

#include <thread>
#include <atomic>

namespace lucid {

ChatSession::ChatSession() = default;

ChatSession::~ChatSession() {
    stop();
}

void ChatSession::set_engine(LLMEngine* engine) {
    m_engine = engine;
}

void ChatSession::clear() {
    m_messages.clear();
    m_active = false;
    LUCID_DEBUG("[ChatSession] Cleared");
}

void ChatSession::add_user_message(const std::string& content) {
    m_messages.push_back({ChatMessage::Role::USER, content});
    trim_history();
    LUCID_DEBUG("[ChatSession] Added user message (%d total)", (int)m_messages.size());
}

void ChatSession::add_assistant_message(const std::string& content) {
    m_messages.push_back({ChatMessage::Role::ASSISTANT, content});
    LUCID_DEBUG("[ChatSession] Added assistant message");
}

void ChatSession::set_system_message(const std::string& content) {
    m_system_prompt = content;
    LUCID_DEBUG("[ChatSession] System prompt set");
}

void ChatSession::trim_history() {
    // Keep system prompt + last N pairs (each pair = user + assistant)
    int current_pairs = 0;
    int first_to_keep = 0;
    
    for (int i = 0; i < (int)m_messages.size(); i++) {
        if (m_messages[i].role == ChatMessage::Role::USER) {
            current_pairs++;
            if (current_pairs > m_max_history) {
                first_to_keep = i;
                break;
            }
        }
    }
    
    if (first_to_keep > 0) {
        m_messages.erase(m_messages.begin(), m_messages.begin() + first_to_keep);
        LUCID_DEBUG("[ChatSession] Trimmed history, now %d messages", (int)m_messages.size());
    }
}

std::vector<ChatMessage> ChatSession::build_context() const {
    std::vector<ChatMessage> context;
    
    // Add system prompt first
    if (!m_system_prompt.empty()) {
        context.push_back({ChatMessage::Role::SYSTEM, m_system_prompt});
    }
    
    // Add conversation history
    for (const auto& msg : m_messages) {
        context.push_back(msg);
    }
    
    return context;
}

void ChatSession::generate_response(TokenCallback on_token) {
    if (!m_engine) {
        LUCID_ERROR("[ChatSession] No LLM engine set");
        if (on_token) on_token("");
        return;
    }
    
    if (m_messages.empty()) {
        LUCID_WARN("[ChatSession] No messages to respond to");
        return;
    }
    
    // Build context with system prompt
    auto context = build_context();
    
    m_active = true;
    
    // Create callback that converts LLMResponse to string token
    auto response_callback = [on_token](const LLMResponse& chunk) -> bool {
        if (on_token) {
            return on_token(chunk.text);
        }
        return true;
    };
    
    std::string full_response;
    
    bool success = m_engine->generate(context, response_callback, &full_response);
    
    if (success && !full_response.empty()) {
        add_assistant_message(full_response);
    }
    
    m_active = false;
    
    LUCID_DEBUG("[ChatSession] Generation %s", success ? "succeeded" : "failed");
}

void ChatSession::generate_response_async(CompleteCallback on_complete) {
    if (!m_engine) {
        LUCID_ERROR("[ChatSession] No LLM engine set");
        if (on_complete) on_complete("", false);
        return;
    }
    
    // Run generation in background thread
    std::thread([this, on_complete]() {
        std::string response;
        bool success = false;
        
        auto callback = [&response](const std::string& token) -> bool {
            response += token;
            return true;
        };
        
        generate_response(callback);
        
        success = !response.empty();
        
        if (on_complete) {
            on_complete(response, success);
        }
    }).detach();
}

void ChatSession::generate_with_context(const std::string& context, TokenCallback on_token) {
    if (!m_engine) {
        LUCID_ERROR("[ChatSession] No LLM engine set");
        if (on_token) on_token("");
        return;
    }
    
    if (m_messages.empty()) {
        LUCID_WARN("[ChatSession] No messages to respond to");
        return;
    }
    
    // Build context with extra context prepended to system prompt
    auto ctx = build_context_with_extra(context);
    
    m_active = true;
    
    auto response_callback = [on_token](const LLMResponse& chunk) -> bool {
        if (on_token) {
            return on_token(chunk.text);
        }
        return true;
    };
    
    std::string full_response;
    bool success = m_engine->generate(ctx, response_callback, &full_response);
    
    if (success && !full_response.empty()) {
        add_assistant_message(full_response);
    }
    
    m_active = false;
    
    LUCID_DEBUG("[ChatSession] Context-aware generation %s", success ? "succeeded" : "failed");
}

std::vector<ChatMessage> ChatSession::build_context_with_extra(const std::string& extra_context) const {
    std::vector<ChatMessage> context;
    
    // Build enhanced system prompt with extra context
    std::string enhanced_prompt;
    
    if (!m_system_prompt.empty()) {
        enhanced_prompt = m_system_prompt;
        
        // Append extra context if provided
        if (!extra_context.empty()) {
            enhanced_prompt += "\n\n";
            enhanced_prompt += "=== VIDEO LIBRARY CONTEXT ===\n";
            enhanced_prompt += extra_context;
            enhanced_prompt += "\n============================\n\n";
            enhanced_prompt += "Use the above video library context to answer the user's question. ";
            enhanced_prompt += "Reference specific video titles and timestamps when relevant.";
        }
        
        context.push_back({ChatMessage::Role::SYSTEM, enhanced_prompt});
    } else if (!extra_context.empty()) {
        // No base system prompt, create one with context
        enhanced_prompt = "You are a helpful assistant for a video library application called Lucid.\n\n";
        enhanced_prompt += "=== VIDEO LIBRARY CONTEXT ===\n";
        enhanced_prompt += extra_context;
        enhanced_prompt += "\n============================\n\n";
        enhanced_prompt += "Use the above video library context to answer the user's question. ";
        enhanced_prompt += "Reference specific video titles and timestamps when relevant.";
        
        context.push_back({ChatMessage::Role::SYSTEM, enhanced_prompt});
    }
    
    // Add conversation history
    for (const auto& msg : m_messages) {
        context.push_back(msg);
    }
    
    return context;
}

void ChatSession::stop() {
    if (m_engine) {
        m_engine->stop();
    }
    m_active = false;
}

} // namespace lucid
