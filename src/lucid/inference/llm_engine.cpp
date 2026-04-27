// llm_engine.cpp - LLM Engine Implementation
//
// Local LLM inference using llama.cpp

#include "llm_engine.hpp"
#include "../logging.hpp"

#include <cstring>
#include <iostream>
#include <sstream>

// Llama C API
extern "C" {
#include <llama.h>
}

namespace lucid {

// ============================================================================
// Helper Functions
// ============================================================================

// ============================================================================
// LLM Engine Implementation
// ============================================================================

LLMEngine::LLMEngine() = default;

LLMEngine::~LLMEngine() {
    shutdown();
}

void LLMEngine::set_config(const LLMConfig& config) {
    m_config = config;
    
    // If already initialized, would need to recreate context
    // For now, config only affects new initializations
    if (m_ready.load()) {
        LUCID_WARN("[LLMEngine] Config change requires re-initialization");
    }
}

std::string LLMEngine::get_model_name() const {
    if (!m_model) return "unknown";
    char buf[256];
    llama_model_desc(m_model, buf, sizeof(buf));
    return buf;
}

int LLMEngine::get_model_size() const {
    if (!m_model) return 0;
    return llama_model_size(m_model);
}

int LLMEngine::get_vocab_size() const {
    if (!m_model) return 0;
    auto vocab = llama_model_get_vocab(m_model);
    return llama_vocab_n_tokens(vocab);
}

bool LLMEngine::initialize(const std::string& model_path) {
    return initialize(model_path, m_config);
}

bool LLMEngine::initialize(const std::string& model_path, const LLMConfig& config) {
    if (m_ready.load() || m_loading.load()) {
        LUCID_WARN("[LLMEngine] Already initialized or loading");
        return false;
    }
    
    m_loading.store(true);
    m_config = config;
    
    LUCID_INFO("[LLMEngine] Initializing with model: %s", model_path.c_str());
    
    // Initialize llama backend
    llama_backend_init();
    
    // Load model
    struct llama_model_params params = llama_model_default_params();
    params.n_gpu_layers = m_config.gpu_layers;
    
    m_model = llama_model_load_from_file(model_path.c_str(), params);
    if (!m_model) {
        LUCID_ERROR("[LLMEngine] Failed to load model from: %s", model_path.c_str());
        llama_backend_free();
        m_loading.store(false);
        return false;
    }
    
    // Log model info
    char buf[256];
    llama_model_desc(m_model, buf, sizeof(buf));
    LUCID_INFO("[LLMEngine] Model loaded: %s (%llu bytes)", 
               buf, (unsigned long long)llama_model_size(m_model));
    
    // Get vocab
    m_vocab = llama_model_get_vocab(m_model);
    if (m_vocab) {
        LUCID_INFO("[LLMEngine] Vocab size: %d", llama_vocab_n_tokens(m_vocab));
    }
    
    // Create context
    struct llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = m_config.context_size;
    ctx_params.n_threads = m_config.threads;
    
    m_ctx = llama_init_from_model(m_model, ctx_params);
    if (!m_ctx) {
        LUCID_ERROR("[LLMEngine] Failed to create context");
        llama_model_free(m_model);
        m_model = nullptr;
        llama_backend_free();
        m_loading.store(false);
        return false;
    }
    
    m_ready.store(true);
    m_loading.store(false);
    
    LUCID_INFO("[LLMEngine] Ready. Context: %d tokens", m_config.context_size);
    
    return true;
}

void LLMEngine::shutdown() {
    m_stop_requested.store(true);
    
    if (m_ctx) {
        llama_free(m_ctx);
        m_ctx = nullptr;
    }
    if (m_model) {
        llama_model_free(m_model);
        m_model = nullptr;
    }
    
    llama_backend_free();
    
    m_ready.store(false);
    m_loading.store(false);
    m_generating.store(false);
    
    LUCID_INFO("[LLMEngine] Shutdown complete");
}

bool LLMEngine::generate(
    const std::vector<ChatMessage>& messages,
    std::function<bool(const LLMResponse& chunk)> on_token,
    std::string* full_response) {
    
    if (!m_ready.load()) {
        LUCID_ERROR("[LLMEngine] Not ready - call initialize() first");
        return false;
    }
    
    if (m_generating.load()) {
        LUCID_ERROR("[LLMEngine] Already generating - call stop() first");
        return false;
    }
    
    if (!m_model || !m_ctx) {
        LUCID_ERROR("[LLMEngine] Model or context is null");
        return false;
    }
    
    m_generating.store(true);
    m_stop_requested.store(false);
    
    bool success = generate_tokens(m_ctx, m_model, messages, on_token, full_response);
    
    m_generating.store(false);
    
    return success;
}

void LLMEngine::stop() {
    LUCID_INFO("[LLMEngine] Stop requested");
    m_stop_requested.store(true);
}

std::optional<std::string> LLMEngine::complete(const std::string& prompt) {
    std::string result;
    
    auto callback = [&result](const LLMResponse& chunk) -> bool {
        result += chunk.text;
        return true;  // Continue
    };
    
    std::vector<ChatMessage> messages = {
        {ChatMessage::Role::USER, prompt, 0}  // timestamp=0 for one-off queries
    };
    
    if (generate(messages, callback, &result)) {
        return result;
    }
    
    return std::nullopt;
}

std::string LLMEngine::format_prompt(const std::vector<ChatMessage>& messages) {
    // Try to use the model's chat template first (proper for instruction-tuned models)
    const char* tmpl = llama_model_chat_template(m_model, nullptr);
    
    if (tmpl) {
        // Build message array for the API
        std::vector<llama_chat_message> chat_msgs;
        for (const auto& msg : messages) {
            llama_chat_message m;
            switch (msg.role) {
                case ChatMessage::Role::SYSTEM:
                    m.role = "system";
                    break;
                case ChatMessage::Role::USER:
                    m.role = "user";
                    break;
                case ChatMessage::Role::ASSISTANT:
                    m.role = "assistant";
                    break;
                default:
                    m.role = "user";
            }
            m.content = msg.content.c_str();
            chat_msgs.push_back(m);
        }
        
        // Get required buffer size
        int32_t len = llama_chat_apply_template(tmpl, chat_msgs.data(), chat_msgs.size(), true, nullptr, 0);
        if (len > 0) {
            std::vector<char> buf(len + 1);
            int32_t result_len = llama_chat_apply_template(tmpl, chat_msgs.data(), chat_msgs.size(), true, buf.data(), len);
            if (result_len > 0) {
                buf[result_len] = '\0';
                LUCID_DEBUG("[LLMEngine] Using model chat template, prompt length: %d", result_len);
                return std::string(buf.data(), result_len);
            }
        }
        
        LUCID_WARN("[LLMEngine] Chat template apply failed, falling back to simple format");
    } else {
        LUCID_DEBUG("[LLMEngine] Model has no chat template, using simple format");
    }
    
    // Fallback: Simple format
    std::ostringstream oss;
    for (const auto& msg : messages) {
        switch (msg.role) {
            case ChatMessage::Role::SYSTEM:
                oss << "system: " << msg.content << "\n\n";
                break;
            case ChatMessage::Role::USER:
                oss << "user: " << msg.content << "\n\n";
                break;
            case ChatMessage::Role::ASSISTANT:
                oss << "assistant: " << msg.content << "\n\n";
                break;
        }
    }
    
    oss << "assistant: ";
    return oss.str();
}

bool LLMEngine::generate_tokens(
    struct llama_context* ctx,
    struct llama_model* model,
    const std::vector<ChatMessage>& messages,
    std::function<bool(const LLMResponse& chunk)> on_token,
    std::string* full_response) {
    
    // Format prompt
    std::string prompt = format_prompt(messages);
    
    // Tokenize prompt using vocab from model
    auto vocab = llama_model_get_vocab(model);
    if (!vocab) {
        LUCID_ERROR("[LLMEngine] Failed to get vocab");
        return false;
    }
    
    std::vector<llama_token> tokens(2048);
    int n_tokens = llama_tokenize(vocab, prompt.c_str(), 
                                  (int)prompt.size(),
                                  tokens.data(), (int)tokens.size(),
                                  true, true);  // add_special=true, parse_special=true
    if (n_tokens < 0) {
        LUCID_ERROR("[LLMEngine] Failed to tokenize prompt");
        return false;
    }
    tokens.resize(n_tokens);
    
    LUCID_DEBUG("[LLMEngine] Prompt tokenized: %d tokens", n_tokens);
    
    // Check context size
    int n_ctx = llama_n_ctx(ctx);
    LUCID_DEBUG("[LLMEngine] Context size: %d, prompt tokens: %zu", n_ctx, tokens.size());
    if ((int)tokens.size() >= n_ctx) {
        LUCID_ERROR("[LLMEngine] Prompt too long for context (%d >= %d)", 
                   (int)tokens.size(), n_ctx);
        return false;
    }
    
    // Log first few tokens for debugging
    LUCID_DEBUG("[LLMEngine] First 10 tokens:");
    for (int i = 0; i < std::min(10, (int)tokens.size()); i++) {
        char buf[64];
        int n = llama_token_to_piece(vocab, tokens[i], buf, sizeof(buf) - 1, 0, true);
        if (n > 0) {
            buf[n] = '\0';
            LUCID_DEBUG("  token[%d]=%d -> '%s'", i, tokens[i], buf);
        }
    }
    
    // Clear KV cache before processing new prompt
    // This is required because the context may have residual data from previous generation
    llama_memory_clear(llama_get_memory(ctx), true);
    LUCID_DEBUG("[LLMEngine] KV cache cleared");
    
    // Create batch for prompt evaluation
    // For the full prompt, we need to use a proper batch
    struct llama_batch batch = llama_batch_init(n_tokens, 0, 1);
    
    for (int i = 0; i < n_tokens; i++) {
        batch.token[batch.n_tokens] = tokens[i];
        batch.pos[batch.n_tokens] = i;
        batch.n_seq_id[batch.n_tokens] = 1;
        batch.seq_id[batch.n_tokens][0] = 0;
        batch.logits[batch.n_tokens] = (i == n_tokens - 1); // only logits for last token
        batch.n_tokens++;
    }
    
    LUCID_DEBUG("[LLMEngine] Evaluating prompt batch with %d tokens", batch.n_tokens);
    
    // Evaluate prompt tokens
    if (llama_decode(ctx, batch)) {
        LUCID_ERROR("[LLMEngine] Failed to decode prompt");
        llama_batch_free(batch);
        return false;
    }
    
    llama_batch_free(batch);
    
    LUCID_DEBUG("[LLMEngine] Prompt evaluated successfully");
    
    // Generation loop
    std::string output;
    int n_generated = 0;
    const int max_new = m_config.max_tokens;
    
    LUCID_DEBUG("[LLMEngine] Prompt evaluated, starting generation loop (max %d tokens)", max_new);
    
    while (n_generated < max_new) {
        if (m_stop_requested.load()) {
            LUCID_INFO("[LLMEngine] Generation stopped by user");
            break;
        }
        
        // Sample next token
        auto logits = llama_get_logits_ith(ctx, -1);
        auto n_vocab = llama_vocab_n_tokens(vocab);
        
        // Simple greedy sampling
        int next_token = 0;
        float max_logit = -1e30f;
        for (int i = 0; i < n_vocab; i++) {
            if (logits[i] > max_logit) {
                max_logit = logits[i];
                next_token = i;
            }
        }
        
        // Check for EOS using llama_vocab_is_eog (handles multiple EOG tokens)
        if (llama_vocab_is_eog(vocab, next_token)) {
            LUCID_DEBUG("[LLMEngine] EOS token reached: %d", next_token);
            break;
        }
        
        // Convert token to text using llama_token_to_piece (handles special tokens properly)
        char token_buf[256];
        int token_len = llama_token_to_piece(vocab, next_token, token_buf, sizeof(token_buf) - 1, 0, true);
        std::string token_text;
        if (token_len > 0) {
            token_buf[token_len] = '\0';
            token_text = token_buf;
        }
        
        output += token_text;
        n_generated++;
        
        // Debug: log first 20 tokens
        if (n_generated <= 20) {
            LUCID_DEBUG("[LLMEngine] Generated token %d: id=%d text='%s'", 
                       n_generated, next_token, token_text.c_str());
        } else if (n_generated == 21) {
            LUCID_DEBUG("[LLMEngine] ... (suppressing further token logging)");
        }
        
        // Create response chunk
        LLMResponse chunk;
        chunk.text = token_text;
        chunk.n_tokens_generated = n_generated;
        chunk.is_final = false;
        
        // Callback
        if (on_token) {
            if (!on_token(chunk)) {
                LUCID_INFO("[LLMEngine] Callback requested stop");
                break;
            }
        }
        
        // Decode for next iteration
        llama_token tokens_batch[1] = { (llama_token)next_token };
        if (llama_decode(ctx, llama_batch_get_one(tokens_batch, 1))) {
            LUCID_ERROR("[LLMEngine] Failed to decode token");
            break;
        }
    }
    
    // Final chunk
    if (full_response) {
        *full_response = output;
    }
    
    if (on_token) {
        LLMResponse final_chunk;
        final_chunk.text = "";
        final_chunk.n_tokens_generated = n_generated;
        final_chunk.is_final = true;
        on_token(final_chunk);
    }
    
    LUCID_DEBUG("[LLMEngine] Generation complete: %d tokens", n_generated);
    return true;
}

std::vector<int> LLMEngine::tokenize(const std::string& text) {
    if (!m_vocab) return {};
    
    std::vector<int> tokens(512);
    int n = llama_tokenize(m_vocab, text.c_str(), (int)text.size(),
                          tokens.data(), (int)tokens.size(),
                          true, true);  // add_special=true, parse_special=true
    if (n < 0) return {};
    
    tokens.resize(n);
    return tokens;
}

std::string LLMEngine::detokenize(const std::vector<int>& tokens) {
    if (!m_vocab) return {};
    
    std::string result;
    for (int tok : tokens) {
        char buf[256];
        int n = llama_token_to_piece(m_vocab, tok, buf, sizeof(buf) - 1, 0, true);
        if (n > 0) {
            buf[n] = '\0';
            result += buf;
        }
    }
    return result;
}

} // namespace lucid
