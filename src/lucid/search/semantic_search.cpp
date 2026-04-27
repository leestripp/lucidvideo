// semantic_search.cpp - Semantic video search implementation
//
// Wires to EmbeddingStore (llama.cpp) for real text embeddings.
// Pipeline: load stored embeddings from DB → search via cosine similarity.

#include "semantic_search.hpp"
#include "../inference/embedding_store.hpp"
#include "../model/database.hpp"
#include "../logging.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <regex>
#include <sstream>

namespace lucid {

// ============================================================================
// Internal Implementation (pimpl to avoid leaking llama types in header)
// ============================================================================

struct SemanticSearch::EmbeddingStoreImpl {
    lucid::EmbeddingStore store;
    int embedding_dim = 768;
    int context_size = 2048;
    bool initialized = false;

    bool init(const std::string& model_path, int ctx_size = 2048) {
        if (!std::filesystem::exists(model_path)) {
            LUCID_ERROR("[SemanticSearch] Embedding model not found: %s", model_path.c_str());
            return false;
        }

        context_size = ctx_size;
        store.set_num_threads(4);
        store.set_context_size(context_size);
        // Note: use_gpu parameter passed during init, not stored in struct
        
        if (!store.initialize(model_path)) {
            LUCID_ERROR("[SemanticSearch] Failed to init EmbeddingStore");
            return false;
        }

        embedding_dim = store.get_embedding_dim();
        initialized = true;
        LUCID_INFO("[SemanticSearch] EmbeddingStore initialized (dim=%d)", embedding_dim);
        return true;
    }
    
    bool init_with_gpu(const std::string& model_path, int ctx_size, bool use_gpu) {
        if (!std::filesystem::exists(model_path)) {
            LUCID_ERROR("[SemanticSearch] Embedding model not found: %s", model_path.c_str());
            return false;
        }
        
        context_size = ctx_size;
        store.set_num_threads(4);
        store.set_context_size(context_size);
        store.set_use_gpu(use_gpu);
        
        if (!store.initialize(model_path)) {
            LUCID_ERROR("[SemanticSearch] Failed to init EmbeddingStore");
            return false;
        }

        embedding_dim = store.get_embedding_dim();
        initialized = true;
        LUCID_INFO("[SemanticSearch] EmbeddingStore initialized (dim=%d, use_gpu=%s)", 
                    embedding_dim, use_gpu ? "true" : "false");
        return true;
    }

    std::optional<std::vector<float>> embed(const std::string& text) {
        if (!initialized) return std::nullopt;
        auto result = store.get_embedding(text);
        if (result.has_value()) {
            return result->embedding;
        }
        return std::nullopt;
    }
    
    void shutdown_store() {
        store.shutdown();
        initialized = false;
    }
};

// ============================================================================
// SemanticSearch Implementation
// ============================================================================

SemanticSearch::SemanticSearch(Database& db)
    : m_db(&db) {
}

SemanticSearch::~SemanticSearch() = default;

bool SemanticSearch::initialize(const std::string& model_path, int context_size) {
    LUCID_INFO("[SemanticSearch] Initializing semantic search engine (ctx=%d)", context_size);
    m_context_size = context_size;
    m_model_path = model_path;

    // Initialize the embedding store (llama.cpp)
    m_embed_store = std::make_unique<EmbeddingStoreImpl>();
    if (!m_embed_store->init(model_path, context_size)) {
        LUCID_WARN("[SemanticSearch] EmbeddingStore init failed - semantic search unavailable");
        m_embed_store.reset();
    } else {
        m_embedding_dim = m_embed_store->embedding_dim;
    }

    // Load stored embeddings from database (marker transcripts)
    load_embeddings_from_db();

    // Index all video titles for semantic filename search
    index_all_video_titles();

    m_initialized = true;
    return true;
}

void SemanticSearch::shutdown() {
    if (m_embed_store) {
        LUCID_INFO("[SemanticSearch] Shutting down EmbeddingStore");
        m_embed_store->shutdown_store();
        m_embed_store.reset();
    }
    m_initialized = false;
}

bool SemanticSearch::reload(const std::string& model_path, int context_size, bool use_gpu) {
    LUCID_INFO("[SemanticSearch] Reloading EmbeddingStore (use_gpu=%s)", use_gpu ? "true" : "false");
    
    // Shutdown existing store
    shutdown();
    
    // Create new store with GPU settings
    m_embed_store = std::make_unique<EmbeddingStoreImpl>();
    m_context_size = context_size;
    m_model_path = model_path;
    
    if (!m_embed_store->init_with_gpu(model_path, context_size, use_gpu)) {
        LUCID_WARN("[SemanticSearch] Failed to reload EmbeddingStore - semantic search unavailable");
        m_embed_store.reset();
        return false;
    }
    
    m_embedding_dim = m_embed_store->embedding_dim;
    m_initialized = true;
    
    // Reload embeddings from DB and re-index titles
    load_embeddings_from_db();
    index_all_video_titles();
    
    return true;
}

void SemanticSearch::load_embeddings_from_db() {
    if (!m_db) return;

    auto stored = m_db->get_all_embeddings();
    m_index.clear();
    m_index.reserve(stored.size());

    for (auto& e : stored) {
        if (e.embedding.empty()) continue;

        IndexEntry entry;
        entry.video_id = e.video_id;
        entry.text = e.text;
        entry.timestamp_ms = e.timestamp_ms;
        entry.embedding = normalize(e.embedding);
        m_index.push_back(std::move(entry));
    }

    LUCID_INFO("[SemanticSearch] Loaded %zu embeddings from database", m_index.size());
}

std::optional<std::vector<float>> SemanticSearch::embed_text(const std::string& text) {
    if (m_embed_store) {
        return m_embed_store->embed(text);
    }
    return std::nullopt;
}

std::vector<SearchResult> SemanticSearch::search(const SemanticQuery& query) {
    std::vector<float> query_emb;

    if (!query.query_embedding.empty()) {
        query_emb = normalize(query.query_embedding);
    } else if (!query.query_text.empty()) {
        auto emb = embed_text(query.query_text);
        if (emb.has_value()) {
            query_emb = normalize(*emb);
        }
    }

    if (query_emb.empty()) {
        LUCID_WARN("[SemanticSearch] No query embedding available");
        return {};
    }

    LUCID_INFO("[SemanticSearch] Search: query='%s', threshold=%.2f, max_results=%d, index_size=%zu, query_emb[0]=%.3f",
                query.query_text.c_str(), query.similarity_threshold, query.max_results, m_index.size(), query_emb[0]);

    return search_embeddings(query_emb, query.similarity_threshold, query.max_results);
}

std::vector<SearchResult> SemanticSearch::search_embeddings(
    const std::vector<float>& query_emb,
    float threshold,
    int max_results) {

    std::vector<SearchResult> results;

    LUCID_INFO("[SemanticSearch] search_embeddings: threshold=%.2f, max_results=%d, index entries=%zu, first entry emb size=%zu",
                threshold, max_results, m_index.size(), 
                m_index.empty() ? 0 : m_index[0].embedding.size());

    for (const auto& entry : m_index) {
        if (entry.embedding.empty()) continue;

        float similarity = cosine_similarity(query_emb, entry.embedding);
        LUCID_DEBUG("[SemanticSearch]   Entry video=%s, text='%s', similarity=%.4f, passes=%.4f>=%.2f",
                    entry.video_id.c_str(), entry.text.substr(0, 50).c_str(), similarity, similarity, threshold);
        if (similarity < threshold) continue;

        SearchResult result;
        result.video_id = entry.video_id;
        result.timestamp_ms = static_cast<int>(entry.timestamp_ms);
        result.relevance_score = similarity;
        result.snippet = entry.text;

        // Set match type based on whether it's a video title or marker
        result.match_type = entry.is_video_title ? MatchType::TITLE : MatchType::TRANSCRIPT;

        results.push_back(result);
    }

    // Sort by score descending
    std::sort(results.begin(), results.end(),
              [](const SearchResult& a, const SearchResult& b) {
                  return a.relevance_score > b.relevance_score;
              });

    if (static_cast<int>(results.size()) > max_results) {
        results.resize(max_results);
    }

    LUCID_DEBUG("[SemanticSearch] search_embeddings: returned %zu results", results.size());

    return results;
}

// ============================================================================
// Hybrid Search (Keyword + Semantic)
// ============================================================================

std::vector<SearchResult> SemanticSearch::hybrid_search(
    const std::string& keyword_query,
    int max_results,
    float semantic_weight,
    float keyword_weight) {

    // Normalize query for keyword matching
    std::string normalized_query = keyword_query;
    std::transform(normalized_query.begin(), normalized_query.end(),
                   normalized_query.begin(), ::tolower);

    // Generate semantic embedding for the query
    auto query_emb = embed_text(keyword_query);
    std::vector<float> query_vec;
    if (query_emb.has_value()) {
        query_vec = normalize(*query_emb);
    }

    // Score map: video_id -> best score for that video
    struct ScoredEntry {
        std::string video_id;
        std::string snippet;
        int timestamp_ms = 0;
        double semantic_score = 0.0;
        double keyword_score = 0.0;
        double combined_score = 0.0;
        MatchType match_type = MatchType::UNKNOWN;
    };
    std::map<std::string, ScoredEntry> video_scores;

    for (const auto& entry : m_index) {
        if (entry.embedding.empty()) continue;

        // Calculate keyword match score (exact substring match)
        double kw_score = 0.0;
        std::string entry_text_lower = entry.text;
        std::transform(entry_text_lower.begin(), entry_text_lower.end(),
                       entry_text_lower.begin(), ::tolower);

        if (entry_text_lower.find(normalized_query) != std::string::npos) {
            // Exact substring match
            kw_score = 1.0;
        } else {
            // Check for word-level matches
            std::istringstream query_stream(normalized_query);
            std::istringstream text_stream(entry_text_lower);
            std::string query_word, text_word;
            int matched_words = 0, total_words = 0;

            while (query_stream >> query_word) {
                total_words++;
                while (text_stream >> text_word) {
                    if (text_word.find(query_word) != std::string::npos) {
                        matched_words++;
                        break;
                    }
                }
            }
            if (total_words > 0) {
                kw_score = static_cast<double>(matched_words) / total_words;
            }
        }

        // Calculate semantic similarity
        double sem_score = 0.0;
        if (!query_vec.empty()) {
            sem_score = cosine_similarity(query_vec, entry.embedding);
        }

        // Combined score
        double combined = (keyword_weight * kw_score) + (semantic_weight * sem_score);

        // Update best score for this video
        std::string vid = entry.video_id;
        auto it = video_scores.find(vid);
        if (it == video_scores.end() || combined > it->second.combined_score) {
            ScoredEntry se;
            se.video_id = vid;
            se.snippet = entry.text;
            se.timestamp_ms = static_cast<int>(entry.timestamp_ms);
            se.semantic_score = sem_score;
            se.keyword_score = kw_score;
            se.combined_score = combined;
            se.match_type = entry.is_video_title ? MatchType::TITLE : MatchType::TRANSCRIPT;
            video_scores[vid] = se;
        }
    }

    // Convert to SearchResult vector
    std::vector<SearchResult> results;
    for (const auto& pair : video_scores) {
        const auto& se = pair.second;
        SearchResult result;
        result.video_id = se.video_id;
        result.snippet = se.snippet;
        result.timestamp_ms = se.timestamp_ms;
        result.relevance_score = se.combined_score;
        result.match_type = se.match_type;
        results.push_back(result);
    }

    // Sort by combined score descending
    std::sort(results.begin(), results.end(),
              [](const SearchResult& a, const SearchResult& b) {
                  return a.relevance_score > b.relevance_score;
              });

    if (static_cast<int>(results.size()) > max_results) {
        results.resize(max_results);
    }

    LUCID_DEBUG("[SemanticSearch] Hybrid search for \"%s\": %zu unique videos, top score: %.2f",
                keyword_query.c_str(), results.size(),
                results.empty() ? 0.0 : results[0].relevance_score);

    return results;
}

std::vector<SearchResult> SemanticSearch::search_text(const std::string& text,
                                                     int max_results) {
    SemanticQuery query;
    query.query_text = text;
    query.max_results = max_results;
    return search(query);
}

std::vector<SearchResult> SemanticSearch::search_in_video(
    const std::string& video_id,
    const std::string& text,
    int max_results) {

    auto emb = embed_text(text);
    if (!emb.has_value()) return {};
    auto query_vec = normalize(*emb);

    std::vector<SearchResult> results;

    for (const auto& entry : m_index) {
        if (entry.video_id != video_id) continue;
        if (entry.embedding.empty()) continue;

        float similarity = cosine_similarity(query_vec, entry.embedding);
        if (similarity < 0.3f) continue;

        SearchResult result;
        result.video_id = video_id;
        result.timestamp_ms = static_cast<int>(entry.timestamp_ms);
        result.relevance_score = similarity;
        result.snippet = entry.text;
        results.push_back(result);
    }

    std::sort(results.begin(), results.end(),
              [](const SearchResult& a, const SearchResult& b) {
                  return a.relevance_score > b.relevance_score;
              });

    if (static_cast<int>(results.size()) > max_results) {
        results.resize(max_results);
    }

    return results;
}

bool SemanticSearch::index_video(
    const std::string& video_id,
    const std::vector<std::pair<uint64_t, std::vector<float>>>& embeddings) {

    LUCID_DEBUG("[SemanticSearch] Indexing video: %s (%lu embeddings)",
                video_id.c_str(), (unsigned long)embeddings.size());

    for (const auto& emb : embeddings) {
        IndexEntry entry;
        entry.video_id = video_id;
        entry.timestamp_ms = emb.first;
        entry.embedding = normalize(emb.second);
        m_index.push_back(std::move(entry));
    }

    LUCID_INFO("[SemanticSearch] Indexed %lu embeddings for video %s",
               (unsigned long)embeddings.size(), video_id.c_str());
    return true;
}

bool SemanticSearch::remove_from_index(const std::string& video_id) {
    auto it = std::remove_if(m_index.begin(), m_index.end(),
                             [&video_id](const IndexEntry& e) {
                                 return e.video_id == video_id;
                             });
    m_index.erase(it, m_index.end());

    LUCID_INFO("[SemanticSearch] Removed video %s from search index", video_id.c_str());
    return true;
}

float SemanticSearch::compute_distance(const std::vector<float>& a,
                                      const std::vector<float>& b) {
    return cosine_similarity(a, b);
}

float SemanticSearch::cosine_similarity(const std::vector<float>& a,
                                        const std::vector<float>& b) {
    if (a.size() != b.size() || a.empty()) return 0.0f;

    float dot_product = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (size_t i = 0; i < a.size(); ++i) {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    float denom = std::sqrt(norm_a) * std::sqrt(norm_b);
    if (denom < 1e-8f) return 0.0f;

    return dot_product / denom;
}

std::vector<float> SemanticSearch::normalize(const std::vector<float>& vec) {
    std::vector<float> result(vec);

    float norm = 0.0f;
    for (const auto& v : result) {
        norm += v * v;
    }
    norm = std::sqrt(norm);

    if (norm > 1e-8f) {
        for (auto& v : result) {
            v /= norm;
        }
    }

    return result;
}

// ============================================================================
// Filename Indexing for Semantic Search
// ============================================================================

std::string SemanticSearch::clean_filename(const std::string& filename) {
    if (filename.empty()) return "";

    std::string result = filename;

    // Remove common video extensions
    static const std::regex ext_regex(R"(\.(mp4|mov|avi|mkv|webm|m4v|wmv|flv|3gp)$)",
                                      std::regex::icase);
    result = std::regex_replace(result, ext_regex, "");

    // Replace separators and camelCase with spaces
    result = std::regex_replace(result, std::regex("[_\\-\\.\\s]+"), " ");

    // Split camelCase: "pandasDance" -> "pandas Dance"
    result = std::regex_replace(result, std::regex("([a-z])([A-Z])"), "$1 $2");

    // Convert to lowercase for consistency
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);

    // Trim whitespace
    result = std::regex_replace(result, std::regex("^\\s+|\\s+$"), "");

    // Remove extra spaces
    result = std::regex_replace(result, std::regex("\\s+"), " ");

    return result;
}

bool SemanticSearch::index_video_title(const std::string& video_id,
                                       const std::string& title) {
    if (!m_embed_store || title.empty()) {
        return false;
    }

    // Clean the filename to make it more searchable
    std::string cleaned_title = clean_filename(title);

    // Generate embedding for the cleaned title
    auto embedding = m_embed_store->embed(cleaned_title);
    if (!embedding.has_value() || embedding->empty()) {
        LUCID_WARN("[SemanticSearch] Failed to embed title: %s", title.c_str());
        return false;
    }

    // Create index entry with timestamp_ms = 0 to indicate video-level
    IndexEntry entry;
    entry.video_id = video_id;
    entry.text = cleaned_title;
    entry.timestamp_ms = 0;  // 0 = video-level entry
    entry.embedding = normalize(*embedding);
    entry.is_video_title = true;

    m_index.push_back(std::move(entry));

    // Also store in database for persistence
    std::string emb_id = video_id + ":title";
    m_db->add_embedding(emb_id, cleaned_title, *embedding);

    LUCID_DEBUG("[SemanticSearch] Indexed title: %s -> \"%s\"",
                title.c_str(), cleaned_title.c_str());
    return true;
}

void SemanticSearch::index_all_video_titles() {
    if (!m_db) {
        LUCID_WARN("[SemanticSearch] No database available for indexing titles");
        return;
    }

    LUCID_INFO("[SemanticSearch] Indexing all video titles...");

    auto videos = m_db->get_all_videos();
    int indexed = 0;

    for (const auto& video : videos) {
        if (index_video_title(video.id, video.title)) {
            indexed++;
        }
    }

    LUCID_INFO("[SemanticSearch] Indexed %d/%zu video titles",
                indexed, videos.size());
}

} // namespace lucid