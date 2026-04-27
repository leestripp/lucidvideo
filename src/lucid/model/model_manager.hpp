// model_manager.hpp - Model State Orchestrator
//
// Central manager for AI model lifecycle: download, selection, availability tracking
// Acts as singleton to coordinate between UI (ModelHubPage, Settings) and workers
//
// NOTE: This class now delegates all settings access to AppSettings.
// All settings are stored in AppSettings::instance().

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <optional>
#include <atomic>
#include <thread>
#include <filesystem>
#include <algorithm>

#include "../settings/app_settings.hpp"

namespace lucid {

// ============================================================================
// Model Types and States
// ============================================================================

enum class ModelType {
    WHISPER,     // Speech-to-text transcription
    EMBEDDING,   // Text embeddings for semantic search
    LLM          // Large language model for chat
};

enum class ModelState {
    NOT_DOWNLOADED,
    DOWNLOADING,
    DOWNLOADED,
    ERROR
};

// Model descriptor
struct ModelDescriptor {
    std::string id;           // e.g., "whisper-tiny", "nomic-embed-text"
    std::string name;         // e.g., "Whisper Tiny", "Nomic Embed Text"
    ModelType type;
    std::string description;
    std::string param_count;  // e.g., "39M params", "244M params"
    size_t size_bytes;
    std::string huggingface_url;
    std::string sha256;       // For verification
    
    // Runtime state
    ModelState state = ModelState::NOT_DOWNLOADED;
    float download_progress = 0.0f;  // 0.0 to 1.0
    float download_speed = 0.0f;    // bytes/sec, updated during download
    size_t downloaded_bytes = 0;    // bytes downloaded so far
    std::string error_message = "";  // Default empty
    std::string local_path = "";    // Default empty
};

// ============================================================================
// Callbacks for UI Updates
// ============================================================================

struct ModelStatus {
    std::string model_id;
    ModelState state;
    float progress;
    float download_speed;  // bytes/sec, 0 when not downloading
    size_t downloaded_bytes;
    size_t total_bytes;
    std::string error;
};

using ModelStatusCallback = std::function<void(const ModelStatus&)>;
using AllModelsReadyCallback = std::function<void(bool whisper_ready, bool embedding_ready)>;

// ============================================================================
// Model Manager (Singleton)
// ============================================================================

class ModelManager {
public:
    // Get singleton instance
    static ModelManager& instance();
    
    // Prevent copying
    ModelManager(const ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;
    
    // ------------------------------------------------------------------------
    // Initialization
    // ------------------------------------------------------------------------
    
    // Scan models directory and check availability
    void initialize();
    
    // Get models directory path
    std::string get_models_dir() const { return m_models_dir; }
    
    // ------------------------------------------------------------------------
    // Model Catalog
    // ------------------------------------------------------------------------
    
    // Get all available models in catalog
    const std::vector<ModelDescriptor>& get_catalog() const { return m_catalog; }
    
    // Get models by type
    std::vector<ModelDescriptor> get_models_by_type(ModelType type) const;
    
    // Get downloaded models only
    std::vector<ModelDescriptor> get_downloaded_models() const;
    
    // ------------------------------------------------------------------------
    // Model Availability
    // ------------------------------------------------------------------------
    
    // Check if a specific model is downloaded
    bool is_model_downloaded(const std::string& model_id) const;
    
    // Check if we have minimum required models
    bool has_whisper_model() const;
    bool has_embedding_model() const;
    
    // Get path to downloaded model (if downloaded)
    std::optional<std::string> get_model_path(const std::string& model_id) const;
    
    // ------------------------------------------------------------------------
    // Model Selection (Settings - delegated to AppSettings)
    // ------------------------------------------------------------------------
    
    // Get selected models (delegates to AppSettings)
    std::string get_selected_whisper_model() const;
    std::string get_selected_embedding_model() const;
    std::string get_selected_llm_model() const;
    
    // Set selected models (delegates to AppSettings)
    void set_selected_whisper_model(const std::string& model_id);
    void set_selected_embedding_model(const std::string& model_id);
    void set_selected_llm_model(const std::string& model_id);
    
    // General settings getters/setters (delegates to AppSettings)
    bool get_use_gpu() const;
    void set_use_gpu(bool value);
    
    int get_max_parallel_jobs() const;
    void set_max_parallel_jobs(int value);
    
    int get_embedding_context_size() const;
    void set_embedding_context_size(int value);
    
    int get_whisper_context_size() const;
    void set_whisper_context_size(int value);
    
    std::string get_transcriptions_dir() const;
    void set_transcriptions_dir(const std::string& value);
    
    std::vector<std::string> get_watched_folders() const;
    void set_watched_folders(const std::vector<std::string>& folders);
    
    int get_thumbnail_size() const;
    void set_thumbnail_size(int value);
    
    // Vision scan settings (ALWAYS ENABLED, delegates to AppSettings for interval/max)
    bool is_vision_scan_enabled() const { return true; }  // Always returns true
    int get_vision_scan_interval() const;
    void set_vision_scan_interval(int value);
    
    int get_vision_max_per_video() const;
    void set_vision_max_per_video(int value);
    
    VisionExtractSize get_vision_extract_size() const;
    void set_vision_extract_size(VisionExtractSize value);
    
    // Save all settings to disk
    void save_all_settings();
    
    // Get selected model path (for TranscriptionEngine/EmbeddingStore)
    std::optional<std::string> get_selected_whisper_path() const;
    std::optional<std::string> get_selected_embedding_path() const;
    std::optional<std::string> get_selected_llm_path() const;
    
    // Vision model paths (for Qwen2-VL vision scanning)
    std::optional<std::string> get_vision_model_path() const;
    std::optional<std::string> get_vision_mmproj_path() const;
    
    // ------------------------------------------------------------------------
    // Downloads
    // ------------------------------------------------------------------------
    
    // Start downloading a model
    // Returns true if download started, false if already downloading or not in catalog
    bool start_download(const std::string& model_id);
    
    // Cancel ongoing download
    void cancel_download(const std::string& model_id);
    
    // Delete downloaded model
    bool delete_model(const std::string& model_id);
    
    // Check if a download is in progress
    bool is_downloading(const std::string& model_id) const;
    
    // ------------------------------------------------------------------------
    // Callbacks
    // ------------------------------------------------------------------------
    
    // Register for model state changes (download progress, errors)
    void set_on_status_change(ModelStatusCallback callback);
    
    // Register for "all models ready" notification
    void set_on_models_ready(AllModelsReadyCallback callback);
    
    // ------------------------------------------------------------------------
    // First-run Detection
    // ------------------------------------------------------------------------
    
    // Check if this is first run (no config file exists)
    bool is_first_run() const;
    
    // Check if any models have been downloaded before
    bool has_any_downloaded_models() const;
    
    // ------------------------------------------------------------------------
    // Utilities
    // ------------------------------------------------------------------------
    
    // Format size for display
    static std::string format_size(size_t bytes);
    
    // Get model by ID
    const ModelDescriptor* get_model(const std::string& model_id) const;

private:
    ModelManager();
    ~ModelManager() = default;
    
    void build_catalog();
    void load_config();
    void save_config();
    void scan_downloaded_models();
    void notify_status_change(const ModelStatus& status);
    
    std::string m_models_dir;
    std::string m_config_path;
    
    std::vector<ModelDescriptor> m_catalog;
    std::map<std::string, ModelDescriptor*> m_catalog_index;  // Quick lookup
    
    // Settings are now stored in AppSettings - not duplicated here
    
    std::mutex m_mutex;
    std::vector<ModelStatusCallback> m_status_callbacks;
    std::vector<AllModelsReadyCallback> m_ready_callbacks;
    
    // Download thread
    std::thread m_download_thread;
    std::atomic<bool> m_download_cancelled{false};
    std::string m_current_download_id;
};

// ============================================================================
// Inline Implementation
// ============================================================================

inline std::vector<ModelDescriptor> ModelManager::get_models_by_type(ModelType type) const {
    std::vector<ModelDescriptor> result;
    for (const auto& model : m_catalog) {
        if (model.type == type) {
            result.push_back(model);
        }
    }
    return result;
}

inline std::vector<ModelDescriptor> ModelManager::get_downloaded_models() const {
    std::vector<ModelDescriptor> result;
    for (const auto& model : m_catalog) {
        if (model.state == ModelState::DOWNLOADED) {
            result.push_back(model);
        }
    }
    return result;
}

inline bool ModelManager::is_model_downloaded(const std::string& model_id) const {
    auto it = m_catalog_index.find(model_id);
    if (it != m_catalog_index.end()) {
        return it->second->state == ModelState::DOWNLOADED;
    }
    return false;
}

inline bool ModelManager::has_whisper_model() const {
    for (const auto& model : m_catalog) {
        if (model.type == ModelType::WHISPER && model.state == ModelState::DOWNLOADED) {
            return true;
        }
    }
    return false;
}

inline bool ModelManager::has_embedding_model() const {
    for (const auto& model : m_catalog) {
        if (model.type == ModelType::EMBEDDING && model.state == ModelState::DOWNLOADED) {
            return true;
        }
    }
    return false;
}

inline std::optional<std::string> ModelManager::get_model_path(const std::string& model_id) const {
    auto it = m_catalog_index.find(model_id);
    if (it != m_catalog_index.end() && it->second->state == ModelState::DOWNLOADED) {
        return it->second->local_path;
    }
    return std::nullopt;
}

inline std::optional<std::string> ModelManager::get_selected_whisper_path() const {
    return get_model_path(AppSettings::instance().get_whisper_model());
}

inline std::optional<std::string> ModelManager::get_selected_embedding_path() const {
    return get_model_path(AppSettings::instance().get_embedding_model());
}

inline std::optional<std::string> ModelManager::get_selected_llm_path() const {
    return get_model_path(AppSettings::instance().get_llm_model());
}

// Vision model paths (for Qwen2-VL vision scanning)
inline std::optional<std::string> ModelManager::get_vision_model_path() const {
    // Use LLM path for vision - Qwen2-VL f16 handles both
    return get_selected_llm_path();
}

inline std::optional<std::string> ModelManager::get_vision_mmproj_path() const {
    auto model_path = get_vision_model_path();
    if (!model_path.has_value()) {
        return std::nullopt;
    }
    
    // Look for mmproj file in the same directory
    // The mmproj file uses HuggingFace naming: mmproj-{ModelName}.gguf
    // Examples:
    //   - Qwen2-VL-2B-Instruct-f16.gguf -> mmproj-Qwen2-VL-2B-Instruct-f16.gguf
    //   - Only f16 precision models work with vision scanning
    std::filesystem::path p(model_path.value());
    std::filesystem::path dir = p.parent_path();
    std::string stem = p.stem().string();  // filename without extension
    
    // Try various patterns to find the mmproj file
    // Pattern 1: mmproj-{original_model_name}.gguf (HuggingFace convention)
    // Pattern 2: {model_stem}-mmproj.gguf (alternative naming)
    // Pattern 3: Any file containing "mmproj" in the name
    std::vector<std::string> patterns = {
        // HuggingFace convention: mmproj- prefix
        "mmproj-" + stem + ".gguf",
        "mmproj-" + stem.substr(stem.find('-') + 1) + ".gguf",  // Skip repo prefix if present
        // Alternative: -mmproj suffix
        stem + "-mmproj.gguf",
        stem + "-mmproj-f16.gguf",
        // Legacy patterns
        p.filename().string().substr(0, p.filename().string().find_last_of('.')) + "-mmproj.gguf"
    };
    
    std::error_code ec;
    
    // First try exact pattern matches
    for (const auto& pattern : patterns) {
        std::filesystem::path candidate = dir / pattern;
        if (std::filesystem::exists(candidate, ec)) {
            return candidate.string();
        }
    }
    
    // Fallback: look for any file with mmproj in the name (case-insensitive check)
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (entry.is_regular_file(ec)) {
            std::string name = entry.path().filename().string();
            std::string lower_name = name;
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
            if (lower_name.find("mmproj") != std::string::npos) {
                return entry.path().string();
            }
        }
    }
    
    return std::nullopt;
}

inline bool ModelManager::is_downloading(const std::string& model_id) const {
    // Note: This is a const method that needs special handling
    // We just check if download is active (no lock needed for this simple check)
    return !m_current_download_id.empty() && 
           m_current_download_id == model_id;
}

} // namespace lucid
