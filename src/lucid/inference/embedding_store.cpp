// embedding_store.cpp - Llama.cpp Embedding Store Implementation
//
// Text embeddings for semantic search using llama.cpp
//
// FIXED: Now uses proper llama.cpp embedding API with correct API calls:
// - llama_init_from_model() instead of deprecated llama_new_context_with_model()
// - ctx_params.embeddings = true to enable embedding extraction
// - llama_batch_init() for proper batch handling
// - llama_get_embeddings_ith() instead of llama_get_logits_ith()
// - llama_model_get_vocab() for proper tokenization

#include "embedding_store.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <cmath>

#include "../logging.hpp"

// Llama C API - only include if headers are available
#ifdef LUCID_HAVE_LLAMA
extern "C" {
#include <llama.h>
}
#endif

namespace lucid {

// ============================================================================
// Helper Functions (from llama.cpp examples/embedding/embedding.cpp)
// ============================================================================

#ifdef LUCID_HAVE_LLAMA
// Add token to batch with proper sequence tracking
static void batch_add_seq(llama_batch & batch, const std::vector<llama_token>& tokens, llama_seq_id seq_id) {
    size_t n_tokens = tokens.size();
    for (size_t i = 0; i < n_tokens; i++) {
        // Shift position for proper KV cache management
        batch.pos[batch.n_tokens] = i;
        batch.token[batch.n_tokens] = tokens[i];
        batch.n_seq_id[batch.n_tokens] = 1;
        batch.seq_id[batch.n_tokens][0] = seq_id;
        batch.logits[batch.n_tokens] = 1;  // Request logits for embedding extraction
        batch.n_tokens++;
    }
}
#endif // LUCID_HAVE_LLAMA

// ============================================================================
// Embedding Store Implementation
// ============================================================================

EmbeddingStore::EmbeddingStore() = default;

EmbeddingStore::~EmbeddingStore() {
    shutdown();
}

bool EmbeddingStore::initialize(const std::string& model_path) {
    // Check if model file exists
    if (!std::filesystem::exists(model_path)) {
        std::cerr << "[ERROR] Embedding model not found: " << model_path << std::endl;
        return false;
    }
    
#ifdef LUCID_HAVE_LLAMA
    // Initialize GGML/llama backend
    llama_backend_init();
    
    // Load model
    struct llama_model_params params = llama_model_default_params();
    params.n_gpu_layers = m_use_gpu ? -1 : 0;  // -1 = auto, 0 = CPU only
    
    m_model = llama_model_load_from_file(model_path.c_str(), params);
    if (!m_model) {
        std::cerr << "[ERROR] Failed to load llama model" << std::endl;
        return false;
    }
    
    // Get embedding dimension from model
    m_embedding_dim = llama_model_n_embd_out(m_model);
    std::cout << "[INFO] Embedding model dimension: " << m_embedding_dim << std::endl;
    
    // Create context with embeddings enabled
    struct llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = m_context_size;
    ctx_params.n_threads = m_num_threads;
    ctx_params.embeddings = true;  // CRITICAL: Enable embedding extraction
    
    LUCID_DEBUG("EmbeddingStore: context_size=%d, threads=%d, use_gpu=%s",
                m_context_size, m_num_threads, m_use_gpu ? "true" : "false");
    
    // Use new recommended API (replaces deprecated llama_new_context_with_model)
    m_ctx = llama_init_from_model(m_model, ctx_params);
    if (!m_ctx) {
        std::cerr << "[ERROR] Failed to create llama context" << std::endl;
        llama_model_free(m_model);
        m_model = nullptr;
        return false;
    }
    
    // Check pooling type
    enum llama_pooling_type pooling = llama_pooling_type(m_ctx);
    std::cout << "[INFO] Pooling type: " << pooling << std::endl;
    
    m_initialized = true;
    std::cout << "[INFO] Embedding store initialized with model: " << model_path << std::endl;
    return true;
#else
    // Llama not available - placeholder mode
    std::cout << "[WARN] Llama not compiled - using placeholder mode" << std::endl;
    m_embedding_dim = 768;
    m_initialized = true;
    return true;
#endif
}

std::optional<EmbeddingResult> EmbeddingStore::get_embedding(
    const std::string& text,
    const std::string& instruction) {
    
    if (!m_initialized) {
        std::cerr << "[ERROR] Embedding store not initialized" << std::endl;
        return std::nullopt;
    }
    
    EmbeddingResult result;
    
#ifdef LUCID_HAVE_LLAMA
    // Format text with instruction prefix
    std::string formatted = instruction + text;
    
    // Get vocab from model for tokenization
    const struct llama_vocab* vocab = llama_model_get_vocab(m_model);
    
    // Tokenize using vocab
    std::vector<llama_token> tokens(2048);
    int32_t n_tokens = llama_tokenize(vocab, formatted.c_str(), 
                                       (int32_t)formatted.size(),
                                       tokens.data(), (int32_t)tokens.size(),
                                       true, true);  // add_special=true, parse_special=true
    if (n_tokens < 0) {
        std::cerr << "[ERROR] Failed to tokenize text" << std::endl;
        return std::nullopt;
    }
    tokens.resize(n_tokens);
    
    // Initialize batch
    struct llama_batch batch = llama_batch_init((int32_t)n_tokens, 0, 1);
    
    // Add tokens to batch
    batch_add_seq(batch, tokens, 0);
    
    // Set logits for all tokens to request embedding extraction
    for (int i = 0; i < batch.n_tokens; i++) {
        batch.logits[i] = 1;
    }
    
    // Run inference
    if (llama_decode(m_ctx, batch) != 0) {
        std::cerr << "[ERROR] Failed to run inference" << std::endl;
        llama_batch_free(batch);
        return std::nullopt;
    }
    
    // Get embedding from last token (standard for embedding models)
    // For pooled embeddings (like nomic-bert), use llama_get_embeddings_seq
    // which returns the pooled embedding for the entire sequence
    const float* embd = nullptr;
    
    // Try seq-based embedding first (pooled across all tokens in sequence)
    embd = llama_get_embeddings_seq(m_ctx, 0);
    
    if (!embd) {
        // Fallback to last token embedding if pooling not available
        embd = llama_get_embeddings_ith(m_ctx, batch.n_tokens - 1);
    }
    
    if (!embd) {
        std::cerr << "[ERROR] Failed to get embeddings" << std::endl;
        llama_batch_free(batch);
        return std::nullopt;
    }
    
    // Copy to result
    result.embedding.assign(embd, embd + m_embedding_dim);
    result.n_tokens = n_tokens;
    
    // Free batch
    llama_batch_free(batch);
    
    LUCID_DEBUG("Generated embedding for text: %.30s... (%d tokens)", 
                text.c_str(), n_tokens);
#else
    // Placeholder - return dummy embedding
    result.embedding.resize(m_embedding_dim, 0.0f);
    result.n_tokens = 0;
#endif
    
    return result;
}

std::vector<std::optional<EmbeddingResult>> EmbeddingStore::get_embeddings(
    const std::vector<std::string>& texts,
    const std::string& instruction) {
    
    std::vector<std::optional<EmbeddingResult>> results;
    results.reserve(texts.size());
    
    for (const auto& text : texts) {
        results.push_back(get_embedding(text, instruction));
    }
    
    return results;
}

void EmbeddingStore::shutdown() {
#ifdef LUCID_HAVE_LLAMA
    if (m_ctx) {
        llama_free(m_ctx);
        m_ctx = nullptr;
    }
    if (m_model) {
        llama_model_free(m_model);
        m_model = nullptr;
    }
    llama_backend_free();
#endif
    m_initialized = false;
}

} // namespace lucid
