// semantic_search.hpp - Semantic video search implementation
#ifndef LUCID_SEMANTIC_SEARCH_HPP
#define LUCID_SEMANTIC_SEARCH_HPP

#include "../model/database.hpp"  // Provides SearchResult, SemanticQuery, Database

namespace lucid {

// ============================================================================
// Semantic Search
// ============================================================================

class SemanticSearch {
public:
    // Database must be provided for embedding storage/retrieval
    explicit SemanticSearch(Database& db);
    ~SemanticSearch();

    // Initialize: load stored embeddings from DB, init embedding model
    // context_size: context window size for embedding model (default: 2048)
    bool initialize(const std::string& model_path, int context_size = 2048);

    // Search videos by semantic query
    std::vector<SearchResult> search(const SemanticQuery& query);

    // Search with text only
    std::vector<SearchResult> search_text(const std::string& text, int max_results = 20);

    // Hybrid search: combines keyword matching with semantic similarity
    // Returns results ranked by combined score
    std::vector<SearchResult> hybrid_search(
        const std::string& keyword_query,
        int max_results = 20,
        float semantic_weight = 0.7f,
        float keyword_weight = 0.3f
    );

    // Search within a specific video
    std::vector<SearchResult> search_in_video(const std::string& video_id,
                                              const std::string& text,
                                              int max_results = 10);

    // Add embeddings for a video (index marker transcripts)
    bool index_video(const std::string& video_id,
                    const std::vector<std::pair<uint64_t, std::vector<float>>>& embeddings);

    // Index a video by its title (for semantic filename search)
    // Returns true if indexed, false if model not available
    bool index_video_title(const std::string& video_id, const std::string& title);

    // Index all existing videos by their titles (called on startup)
    // Loads video titles from DB and generates embeddings
    void index_all_video_titles();

    // Remove video from search index
    bool remove_from_index(const std::string& video_id);

    // Clean a filename for embedding (removes extension, replaces _ with spaces, etc.)
    static std::string clean_filename(const std::string& filename);

    // Check if semantic search is ready
    bool is_initialized() const { return m_initialized; }

    // Generate embedding for text using llama.cpp
    std::optional<std::vector<float>> embed_text(const std::string& text);
    
    // Set context size for embedding model (default: 2048)
    void set_context_size(int size) { m_context_size = size; }
    
    // Get current context size
    int get_context_size() const { return m_context_size; }

    // Compute cosine similarity between two vectors
    static float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b);

    // Normalize embedding vector
    static std::vector<float> normalize(const std::vector<float>& vec);

    // Get embedding dimension
    int get_embedding_dim() const { return m_embedding_dim; }
    
    // Shutdown and cleanup for GPU/CPU switching
    void shutdown();
    
    // Reload with new settings (for GPU toggle)
    bool reload(const std::string& model_path, int context_size, bool use_gpu);
    
    // Get model path (for reload)
    std::string get_model_path() const { return m_model_path; }

private:
    struct IndexEntry {
        std::string video_id;
        std::string text;
        uint64_t timestamp_ms;
        std::vector<float> embedding;
        bool is_video_title = false;  // True for video title embeddings, false for markers
    };

    // Load all stored embeddings from database
    void load_embeddings_from_db();

    // Search over loaded embeddings with a query embedding
    std::vector<SearchResult> search_embeddings(const std::vector<float>& query_emb,
                                               float threshold = 0.5f,
                                               int max_results = 20);

    float compute_distance(const std::vector<float>& a, const std::vector<float>& b);

    bool m_initialized = false;
    int m_embedding_dim = 768;
    int m_context_size = 2048;  // Context window for embedding model
    std::string m_model_path;   // Model path for reload
    
    // Embedding store for generating query embeddings via llama.cpp
    // (pimpl to avoid leaking llama types in this header)
    struct EmbeddingStoreImpl;
    std::unique_ptr<EmbeddingStoreImpl> m_embed_store;

    // Database reference
    Database* m_db = nullptr;

    // In-memory index of stored embeddings (loaded from DB)
    std::vector<IndexEntry> m_index;
};

} // namespace lucid

#endif // LUCID_SEMANTIC_SEARCH_HPP