// embedding_store.hpp - Llama.cpp Embedding Store
//
// Text embeddings for semantic search using llama.cpp

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <functional>

// Forward declarations for llama types
struct llama_model;
struct llama_context;

namespace lucid {

// ============================================================================
// Data Structures
// ============================================================================

struct EmbeddingResult {
    std::vector<float> embedding;
    int n_tokens = 0;
};

// ============================================================================
// Embedding Store
// ============================================================================

class EmbeddingStore {
public:
    EmbeddingStore();
    ~EmbeddingStore();
    
    // Check if store is initialized
    bool is_initialized() const { return m_initialized; }
    
    // Initialize with model path
    bool initialize(const std::string& model_path);
    
    // Generate embedding for text
    std::optional<EmbeddingResult> get_embedding(
        const std::string& text,
        const std::string& instruction = "search_document: ");
    
    // Batch embeddings
    std::vector<std::optional<EmbeddingResult>> get_embeddings(
        const std::vector<std::string>& texts,
        const std::string& instruction = "search_document: ");
    
    // Get embedding dimension
    int get_embedding_dim() const { return m_embedding_dim; }
    
    // Shutdown
    void shutdown();
    
    // Settings
    void set_context_size(int size) { m_context_size = size; }
    void set_num_threads(int threads) { m_num_threads = threads; }
    void set_use_gpu(bool use_gpu) { m_use_gpu = use_gpu; }

private:
    bool m_initialized = false;
    int m_embedding_dim = 768;  // nomic-embed-text-v1.5 is 768 dim
    
    // Settings
    int m_context_size = 2048;  // n_ctx
    int m_num_threads = 4;       // n_threads
    bool m_use_gpu = false;      // n_gpu_layers
    
    // Llama context and model (proper types, not void*)
    struct llama_model* m_model = nullptr;
    struct llama_context* m_ctx = nullptr;
};

} // namespace lucid