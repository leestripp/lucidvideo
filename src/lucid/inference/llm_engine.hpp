// llm_engine.hpp - LLM Engine for Chat Completions
//
// Local LLM inference using llama.cpp for chat functionality

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <atomic>

// Forward declarations
struct llama_model;
struct llama_context;
struct llama_vocab;
struct llama_batch;

namespace lucid {

// ============================================================================
// Data Structures
// ============================================================================

struct LLMResponse {
    std::string text;
    int n_tokens_generated = 0;
    bool is_final = false;
};

struct LLMConfig {
    int context_size = 2048;
    int threads = 4;
    int gpu_layers = 0;  // 0 = CPU only
    float temperature = 0.7f;
    int max_tokens = 512;
    float repeat_penalty = 1.1f;
    float presence_penalty = 0.0f;
    float frequency_penalty = 0.0f;
};

struct ChatMessage {
    enum class Role { SYSTEM, USER, ASSISTANT };
    Role role;
    std::string content;
    std::string timestamp = "";  // Default empty string
    bool is_error = false;
};

// ============================================================================
// LLM Engine
// ============================================================================

class LLMEngine {
public:
    LLMEngine();
    ~LLMEngine();

    // Check if engine is ready
    bool is_ready() const { return m_ready.load(); }
    bool is_loading() const { return m_loading.load(); }
    bool is_generating() const { return m_generating.load(); }
    
    // Get/Set configuration
    LLMConfig get_config() const { return m_config; }
    void set_config(const LLMConfig& config);
    
    // Get model info
    std::string get_model_name() const;
    int get_model_size() const;
    int get_vocab_size() const;
    
    // Initialize with model path
    bool initialize(const std::string& model_path);
    bool initialize(const std::string& model_path, const LLMConfig& config);
    
    // Shutdown
    void shutdown();
    
    // Generate completion - streams via callback
    // Returns false if generation was stopped or failed
    bool generate(
        const std::vector<ChatMessage>& messages,
        std::function<bool(const LLMResponse& chunk)> on_token,
        std::string* full_response = nullptr);
    
    // Stop ongoing generation
    void stop();
    
    // Simple completion without streaming
    std::optional<std::string> complete(const std::string& prompt);
    
    // Format messages as prompt (uses model's chat template if available)
    std::string format_prompt(const std::vector<ChatMessage>& messages);

private:
    // Internal generation loop
    bool generate_tokens(struct llama_context* ctx, struct llama_model* model,
                        const std::vector<ChatMessage>& messages,
                        std::function<bool(const LLMResponse& chunk)> on_token,
                        std::string* full_response);
    
    // Tokenization helpers
    std::vector<int> tokenize(const std::string& text);
    std::string detokenize(const std::vector<int>& tokens);

    // Configuration
    LLMConfig m_config;
    
    // State
    std::atomic<bool> m_ready{false};
    std::atomic<bool> m_loading{false};
    std::atomic<bool> m_generating{false};
    std::atomic<bool> m_stop_requested{false};
    
    // Llama handles
    struct llama_model* m_model = nullptr;
    struct llama_context* m_ctx = nullptr;
    
    // Vocab for tokenization
    const ::llama_vocab* m_vocab = nullptr;
};

} // namespace lucid
