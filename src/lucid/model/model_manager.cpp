// model_manager.cpp - Model State Orchestrator Implementation
//
// Central manager for AI model lifecycle
//
// NOTE: All settings are now stored in AppSettings. This class delegates
// settings access to AppSettings::instance().

#include "model_manager.hpp"
#include "../settings/app_settings.hpp"
#include "../logging.hpp"
#include "../buffer/byte_buffer.hpp"
#include "nlohmann/json.hpp"

#include <giomm.h>
#include <glib.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <curl/curl.h>
#include <algorithm>
#include <cctype>

namespace lucid {

// ============================================================================
// Singleton Instance
// ============================================================================

ModelManager& ModelManager::instance() {
    static ModelManager instance;
    return instance;
}

// ============================================================================
// Constructor & Initialization
// ============================================================================

ModelManager::ModelManager() {
    // IMPORTANT: Initialize libcurl globally before using it
    // This is required for progress callbacks to work
    static bool curl_initialized = []{
        CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (res != CURLE_OK) {
            LUCID_ERROR("[ModelManager] curl_global_init failed: %d", (int)res);
        }
        return res == CURLE_OK;
    }();
    (void)curl_initialized;  // Suppress unused variable warning
    
    // Set up models directory
    m_models_dir = Glib::build_filename(Glib::get_user_data_dir(), "lucid", "models");
    
    // Ensure directories exist
    std::filesystem::create_directories(m_models_dir);
    
    LUCID_DEBUG("ModelManager initialized, models_dir: %s", m_models_dir.c_str());
    // Note: AppSettings::instance() is initialized automatically on first access
    // This ensures settings are loaded before any other component tries to use them
}

void ModelManager::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    LUCID_DEBUG("[ModelManager] initialize() called");
    LUCID_DEBUG("[ModelManager] Settings are now managed by AppSettings");
    
    // AppSettings is loaded automatically when instance() is called
    // Just ensure it happens here for proper initialization order
    (void)AppSettings::instance().is_loaded();
    LUCID_DEBUG("[ModelManager] AppSettings loaded and ready");
    
    // Build catalog of available models
    build_catalog();
    
    // Settings are now loaded by AppSettings::instance() in constructor
    // Just ensure AppSettings is initialized
    (void)AppSettings::instance().is_loaded();
    
    // Scan for already-downloaded models
    scan_downloaded_models();
    
    // Log catalog summary
    int whisper_count = 0, embed_count = 0, downloaded_count = 0;
    for (const auto& model : m_catalog) {
        if (model.type == ModelType::WHISPER) whisper_count++;
        else embed_count++;
        if (model.state == ModelState::DOWNLOADED) downloaded_count++;
    }
    
    LUCID_DEBUG("[ModelManager] Catalog summary: %d whisper, %d embedding, %d downloaded",
                whisper_count, embed_count, downloaded_count);
    
    // Auto-select first available LLM model if none selected
    // For vision scanning, must use f16 precision model
    if (AppSettings::instance().get_llm_model().empty()) {
        // First check if qwen2-vl-2b (f16) is available (required for vision)
        for (const auto& model : m_catalog) {
            if (model.type == ModelType::LLM && model.state == ModelState::DOWNLOADED && 
                model.id == "qwen2-vl-2b") {
                AppSettings::instance().set_llm_model(model.id);
                LUCID_INFO("[ModelManager] Auto-selected LLM model: %s (f16, required for vision)", model.id.c_str());
                break;
            }
        }
        // If not found, fall back to any other available LLM
        if (AppSettings::instance().get_llm_model().empty()) {
            for (const auto& model : m_catalog) {
                if (model.type == ModelType::LLM && model.state == ModelState::DOWNLOADED) {
                    AppSettings::instance().set_llm_model(model.id);
                    LUCID_INFO("[ModelManager] Auto-selected LLM model: %s", model.id.c_str());
                    break;
                }
            }
        }
    }
    
    LUCID_INFO("[ModelManager] Ready - selected whisper: %s, embedding: %s, llm: %s", 
               AppSettings::instance().get_whisper_model().c_str(), 
               AppSettings::instance().get_embedding_model().c_str(),
               AppSettings::instance().get_llm_model().c_str());
}

// ============================================================================
// Catalog Management
// ============================================================================

void ModelManager::build_catalog() {
    LUCID_DEBUG("[ModelManager] Building model catalog...");
    
    m_catalog.clear();
    m_catalog_index.clear();
    
    // Whisper models (from ggerganov/whisper.cpp on HuggingFace)
    // Note: Only whisper-base is available for download - other models are disabled
    // because the whisper model cannot be changed during video processing
    m_catalog.push_back(ModelDescriptor{
        "whisper-base",
        "Whisper Base",
        ModelType::WHISPER,
        "Balanced speed/accuracy for English. Recommended for video transcription",
        "74M params",
        148 * 1024 * 1024,  // ~148 MB
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin",
        ""
    });
    LUCID_DEBUG("[ModelManager] Added whisper-base to catalog (148 MB)");
    
    // Embedding models (from nomic-ai on HuggingFace)
    // Note: Only nomic-embed-text is available for download - other models are disabled
    // because the embedding model cannot be changed during video processing
    m_catalog.push_back(ModelDescriptor{
        "nomic-embed-text",
        "Nomic Embed Text v1.5",
        ModelType::EMBEDDING,
        "High quality 768-dim embeddings for semantic search. Recommended for video search",
        "137M params",
        274 * 1024 * 1024,  // ~274 MB
        "https://huggingface.co/nomic-ai/nomic-embed-text-v1.5-GGUF/resolve/main/nomic-embed-text-v1.5.Q4_K_M.gguf",
        ""
    });
    LUCID_DEBUG("[ModelManager] Added nomic-embed-text to catalog (274 MB)");
    
    // LLM/Vision models - Qwen2-VL 2B f16 for both text chat and image understanding
    // BOTH models MUST be f16 precision - they MUST match for vision to work
    // Q4_K_M quantized version does NOT work with vision
    m_catalog.push_back(ModelDescriptor{
        "qwen2-vl-2b",
        "Qwen2-VL 2B (f16) - BOTH MODELS MUST BE F16",
        ModelType::LLM,
        "Full precision f16 model for vision + text chat. Vision requires f16 mmproj - both must match!",
        "2B params",
        static_cast<size_t>(4) * 1024 * 1024 * 1024,  // ~4 GB (f16 precision)
        "https://huggingface.co/ggml-org/Qwen2-VL-2B-Instruct-GGUF/resolve/main/Qwen2-VL-2B-Instruct-f16.gguf",
        ""
    });
    
    // Qwen2-VL Vision Projector (mmproj) - required for vision scanning
    // BOTH models MUST be f16 precision - they MUST match for vision to work
    m_catalog.push_back(ModelDescriptor{
        "qwen2-vl-2b-mmproj",
        "Qwen2-VL 2B Vision Projector (f16) - BOTH MUST BE F16",
        ModelType::LLM,
        "Vision projector f16 - BOTH this AND the main model must be f16 for vision to work!",
        "mmproj",
        746 * 1024 * 1024,  // ~746 MB (f16 precision)
        "https://huggingface.co/ggml-org/Qwen2-VL-2B-Instruct-GGUF/resolve/main/mmproj-Qwen2-VL-2B-Instruct-f16.gguf",
        ""
    });
    
    // Build index for quick lookup
    for (auto& model : m_catalog) {
        m_catalog_index[model.id] = &model;
    }
    
    LUCID_DEBUG("[ModelManager] Built catalog with %zu models", m_catalog.size());
    for (const auto& m : m_catalog) {
        LUCID_DEBUG("  - %s: %s (type=%s, size=%zu bytes)", 
                   m.id.c_str(), m.name.c_str(),
                   m.type == ModelType::WHISPER ? "WHISPER" : "EMBEDDING",
                   m.size_bytes);
    }
}

// Utility: case-insensitive string comparison
static bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(a[i]) != std::tolower(b[i])) return false;
    }
    return true;
}

// Utility: check if filename matches model (case-insensitive, handles various naming conventions)
static bool filename_matches_model(const std::string& filename, const std::string& model_id) {
    std::string lower_fn = filename;
    std::string lower_id = model_id;
    std::transform(lower_fn.begin(), lower_fn.end(), lower_fn.begin(), ::tolower);
    std::transform(lower_id.begin(), lower_id.end(), lower_id.begin(), ::tolower);
    
    // Replace hyphens and underscores with wildcards for matching
    for (auto& c : lower_fn) {
        if (c == '-' || c == '_' || c == '.') c = ' ';
    }
    for (auto& c : lower_id) {
        if (c == '-' || c == '_' || c == '.') c = ' ';
    }
    
    // Check if model_id words are contained in filename
    std::istringstream iss(lower_id);
    std::string word;
    while (iss >> word) {
        // Remove common suffixes and model type identifiers
        if (word == "instruct" || word == "gguf" || word == "q4" || word == "k" || 
            word == "m" || word == "f16" || word == "mmproj" || word == "embedding" ||
            word == "llm" || word == "vision" || word == "projector" || word == "tiny" ||
            word == "base" || word == "small" || word == "medium" || word == "large") continue;
        if (lower_fn.find(word) == std::string::npos) {
            return false;
        }
    }
    return true;
}

void ModelManager::scan_downloaded_models() {
    std::error_code ec;
    
    LUCID_DEBUG("[ModelManager] Scanning for downloaded models in: %s", m_models_dir.c_str());
    
    if (!std::filesystem::exists(m_models_dir, ec)) {
        LUCID_DEBUG("[ModelManager] Models directory does not exist, no pre-downloaded models");
        return;
    }
    
    // Build a list of all downloaded files
    std::vector<std::string> downloaded_files;
    for (const auto& entry : std::filesystem::directory_iterator(m_models_dir, ec)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            downloaded_files.push_back(filename);
            LUCID_DEBUG("[ModelManager] Found file: %s", filename.c_str());
        }
    }
    
    int found_count = 0;
    
    // Match models to downloaded files
    for (auto& model : m_catalog) {
        // Build expected filename from URL
        std::string expected_filename = model.huggingface_url.substr(
            model.huggingface_url.find_last_of('/') + 1
        );
        
        // First try exact match
        std::filesystem::path model_path = std::filesystem::path(m_models_dir) / expected_filename;
        if (std::filesystem::exists(model_path, ec)) {
            model.state = ModelState::DOWNLOADED;
            model.local_path = model_path.string();
            model.download_progress = 1.0f;
            found_count++;
            LUCID_INFO("[ModelManager] Found downloaded model (exact): %s at %s", 
                      model.id.c_str(), model_path.c_str());
            continue;
        }
        
        // Try case-insensitive match
        for (const auto& filename : downloaded_files) {
            if (iequals(filename, expected_filename)) {
                model_path = std::filesystem::path(m_models_dir) / filename;
                model.state = ModelState::DOWNLOADED;
                model.local_path = model_path.string();
                model.download_progress = 1.0f;
                found_count++;
                LUCID_INFO("[ModelManager] Found downloaded model (case-insensitive): %s at %s", 
                          model.id.c_str(), model_path.c_str());
                break;
            }
        }
        
        // Try model ID matching (for files downloaded with different naming conventions)
        if (model.state != ModelState::DOWNLOADED) {
            for (const auto& filename : downloaded_files) {
                if (filename_matches_model(filename, model.id)) {
                    model_path = std::filesystem::path(m_models_dir) / filename;
                    model.state = ModelState::DOWNLOADED;
                    model.local_path = model_path.string();
                    model.download_progress = 1.0f;
                    found_count++;
                    LUCID_INFO("[ModelManager] Found downloaded model (ID match): %s -> %s", 
                              model.id.c_str(), filename.c_str());
                    break;
                }
            }
        }
    }
    
    LUCID_DEBUG("[ModelManager] Scan complete: %d of %zu models are downloaded", 
                found_count, m_catalog.size());
}

// ============================================================================
// Model Selection (Delegated to AppSettings)
// ============================================================================

void ModelManager::set_selected_whisper_model(const std::string& model_id) {
    LUCID_INFO("set_selected_whisper_model called with: %s", model_id.c_str());
    // Validate model exists in catalog before setting
    if (m_catalog_index.find(model_id) != m_catalog_index.end()) {
        AppSettings::instance().set_whisper_model(model_id);
        LUCID_INFO("Selected whisper model: %s", model_id.c_str());
    } else {
        LUCID_WARN("Unknown whisper model: %s", model_id.c_str());
    }
    LUCID_INFO("set_selected_whisper_model exiting");
}

void ModelManager::set_selected_embedding_model(const std::string& model_id) {
    // Validate model exists in catalog before setting
    if (m_catalog_index.find(model_id) != m_catalog_index.end()) {
        AppSettings::instance().set_embedding_model(model_id);
        LUCID_INFO("Selected embedding model: %s", model_id.c_str());
    } else {
        LUCID_WARN("Unknown embedding model: %s", model_id.c_str());
    }
}

void ModelManager::set_selected_llm_model(const std::string& model_id) {
    // Validate model exists in catalog before setting
    if (m_catalog_index.find(model_id) != m_catalog_index.end()) {
        AppSettings::instance().set_llm_model(model_id);
        LUCID_INFO("Selected LLM model: %s", model_id.c_str());
    } else {
        LUCID_WARN("Unknown LLM model: %s", model_id.c_str());
    }
}

void ModelManager::set_use_gpu(bool value) {
    AppSettings::instance().set_use_gpu(value);
    LUCID_INFO("Setting use_gpu: %s", value ? "true" : "false");
}

void ModelManager::set_max_parallel_jobs(int value) {
    AppSettings::instance().set_max_parallel_jobs(value);
    LUCID_INFO("Setting max_parallel_jobs: %d", value);
}

void ModelManager::set_embedding_context_size(int value) {
    AppSettings::instance().set_embedding_context_size(value);
    LUCID_INFO("Setting embedding_context_size: %d", value);
}

void ModelManager::set_whisper_context_size(int value) {
    AppSettings::instance().set_whisper_context_size(value);
    LUCID_INFO("Setting whisper_context_size: %d", value);
}

void ModelManager::set_transcriptions_dir(const std::string& value) {
    AppSettings::instance().set_transcriptions_dir(value);
    LUCID_INFO("Setting transcriptions_dir: %s", value.c_str());
}

void ModelManager::set_watched_folders(const std::vector<std::string>& folders) {
    AppSettings::instance().set_watched_folders(folders);
    LUCID_INFO("Setting watched_folders: %zu folders", folders.size());
}

void ModelManager::set_thumbnail_size(int value) {
    AppSettings::instance().set_thumbnail_size_int(value);
    LUCID_INFO("Setting thumbnail_size: %d", value);
}

void ModelManager::set_vision_scan_interval(int value) {
    AppSettings::instance().set_vision_scan_interval(value);
    LUCID_INFO("Setting vision_scan_interval: %d", value);
}

void ModelManager::set_vision_max_per_video(int value) {
    AppSettings::instance().set_vision_max_per_video(value);
    LUCID_INFO("Setting vision_max_per_video: %d", value);
}

VisionExtractSize ModelManager::get_vision_extract_size() const {
    return AppSettings::instance().get_vision_extract_size();
}

void ModelManager::set_vision_extract_size(VisionExtractSize value) {
    AppSettings::instance().set_vision_extract_size(value);
    LUCID_INFO("Setting vision_extract_size: %d", static_cast<int>(value));
}

// ============================================================================
// Settings Getters (Delegated to AppSettings)
// ============================================================================

std::string ModelManager::get_selected_whisper_model() const {
    return AppSettings::instance().get_whisper_model();
}

std::string ModelManager::get_selected_embedding_model() const {
    return AppSettings::instance().get_embedding_model();
}

std::string ModelManager::get_selected_llm_model() const {
    return AppSettings::instance().get_llm_model();
}

bool ModelManager::get_use_gpu() const {
    return AppSettings::instance().get_use_gpu();
}

int ModelManager::get_max_parallel_jobs() const {
    return AppSettings::instance().get_max_parallel_jobs();
}

int ModelManager::get_embedding_context_size() const {
    return AppSettings::instance().get_embedding_context_size();
}

int ModelManager::get_whisper_context_size() const {
    return AppSettings::instance().get_whisper_context_size();
}

std::string ModelManager::get_transcriptions_dir() const {
    return AppSettings::instance().get_transcriptions_dir();
}

std::vector<std::string> ModelManager::get_watched_folders() const {
    return AppSettings::instance().get_watched_folders();
}

int ModelManager::get_thumbnail_size() const {
    return AppSettings::instance().get_thumbnail_size_int();
}

// is_vision_scan_enabled() is defined inline in the header (always returns true)

int ModelManager::get_vision_scan_interval() const {
    return AppSettings::instance().get_vision_scan_interval();
}

int ModelManager::get_vision_max_per_video() const {
    return AppSettings::instance().get_vision_max_per_video();
}

void ModelManager::save_config() {
    std::error_code ec;
    std::filesystem::path config_dir = std::filesystem::path(m_config_path).parent_path();
    std::filesystem::create_directories(config_dir, ec);
    if (ec) {
        LUCID_ERROR("save_config: Failed to create directory: %s", ec.message().c_str());
        return;
    }
    
    try {
        nlohmann::json j;
        j["whisper_model"] = AppSettings::instance().get_whisper_model();
        j["embedding_model"] = AppSettings::instance().get_embedding_model();
        j["llm_model"] = AppSettings::instance().get_llm_model();
        j["use_gpu"] = AppSettings::instance().get_use_gpu();
        j["max_parallel_jobs"] = AppSettings::instance().get_max_parallel_jobs();
        j["embedding_context_size"] = AppSettings::instance().get_embedding_context_size();
        j["whisper_context_size"] = AppSettings::instance().get_whisper_context_size();
        j["thumbnail_size"] = AppSettings::instance().get_thumbnail_size_int();
        j["transcriptions_dir"] = AppSettings::instance().get_transcriptions_dir();
        j["watched_folders"] = AppSettings::instance().get_watched_folders();
        j["vision_scan_enabled"] = true;  // Always enabled
        j["vision_scan_interval"] = AppSettings::instance().get_vision_scan_interval();
        j["vision_max_per_video"] = AppSettings::instance().get_vision_max_per_video();
        
        std::ofstream out(m_config_path);
        if (!out) {
            LUCID_ERROR("save_config: Failed to open file for writing");
            return;
        }
        out << j.dump(4);
        LUCID_INFO("save_config: Config saved successfully to %s", m_config_path.c_str());
    } catch (const std::exception& e) {
        LUCID_ERROR("save_config: Exception: %s", e.what());
    } catch (...) {
        LUCID_ERROR("save_config: Unknown exception occurred");
    }
}

void ModelManager::save_all_settings() {
    std::lock_guard<std::mutex> lock(m_mutex);
    save_config();
}

// ============================================================================
// Downloads
// ============================================================================

bool ModelManager::start_download(const std::string& model_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    LUCID_DEBUG("[ModelManager] start_download() called for: %s", model_id.c_str());
    
    auto it = m_catalog_index.find(model_id);
    if (it == m_catalog_index.end()) {
        LUCID_WARN("[ModelManager] Model not in catalog: %s", model_id.c_str());
        return false;
    }
    
    ModelDescriptor* model = it->second;
    LUCID_DEBUG("[ModelManager] Model info: name=%s, type=%s, size=%zu bytes",
                model->name.c_str(),
                model->type == ModelType::WHISPER ? "WHISPER" : "EMBEDDING",
                model->size_bytes);
    LUCID_DEBUG("[ModelManager] Current state: %d, progress: %.1f%%",
                (int)model->state, model->download_progress * 100);
    
    if (model->state == ModelState::DOWNLOADING) {
        LUCID_DEBUG("[ModelManager] Already downloading: %s", model_id.c_str());
        return false;
    }
    
    if (model->state == ModelState::DOWNLOADED) {
        LUCID_DEBUG("[ModelManager] Already downloaded: %s", model_id.c_str());
        return false;
    }
    
    // Set state to downloading
    model->state = ModelState::DOWNLOADING;
    model->download_progress = 0.0f;
    model->error_message.clear();
    m_current_download_id = model_id;
    m_download_cancelled = false;
    
    // Prepare download info
    std::string url = model->huggingface_url;
    std::string filename = url.substr(url.find_last_of('/') + 1);
    std::filesystem::path output_path_fs = std::filesystem::path(m_models_dir) / filename;
    std::string output_path = output_path_fs.string();
    
    LUCID_INFO("[ModelManager] Starting download: %s (file: %s)", model_id.c_str(), filename.c_str());
    LUCID_DEBUG("[ModelManager] Download URL: %s", url.c_str());
    LUCID_DEBUG("[ModelManager] Output path: %s", output_path.c_str());
    
    // Notify UI
    LUCID_DEBUG("[ModelManager] Sending DOWNLOADING status to callbacks");
    notify_status_change(ModelStatus{model_id, ModelState::DOWNLOADING, 0.0f, 0.0f, 0, 0, ""});
    LUCID_DEBUG("[ModelManager] Status callback sent, preparing download thread...");
    
    // Copy values for lambda capture
    size_t model_size = model->size_bytes;
    
    // Wait for any previous download thread to complete before starting a new one
    // This prevents thread handle leaks and race conditions
    LUCID_DEBUG("[ModelManager] Checking if previous thread is joinable...");
    if (m_download_thread.joinable()) {
        LUCID_DEBUG("[ModelManager] Waiting for previous download thread to finish...");
        m_download_thread.join();
        LUCID_DEBUG("[ModelManager] Previous download thread finished");
    }
    
    LUCID_DEBUG("[ModelManager] Creating new download thread for: %s", model_id.c_str());
    fflush(nullptr); // Flush all output buffers
    
    // Start download in background thread
    // NOTE: All UI notifications MUST go through g_idle_add to run on main thread
    LUCID_DEBUG("[ModelManager] About to create std::thread...");
    m_download_thread = std::thread([this, model_id, url, output_path, model_size]() {
        LUCID_DEBUG("[ModelManager] Inside download thread for: %s", model_id.c_str());
        
        bool success = false;
        std::string error;
        
        // Use ByteBuffer to collect download data
        ByteBuffer buffer;
        
        LUCID_DEBUG("[ModelManager] Using ByteBuffer for download");
        CURL* curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "Lucid/1.0");
            
            // Progress callback setup - ORDER MATTERS!
            // Progress callback data structure - holds ModelManager and model_id
struct ProgressCallbackData {
    ModelManager* mm;
    std::string model_id;
};
            
            // Create callback data - will be freed after download completes
            auto* callback_data = new ProgressCallbackData{this, model_id};
            
            // Must set function and data BEFORE setting NOPROGRESS to 0
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, +[](void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t) -> int {
                auto* data = static_cast<ProgressCallbackData*>(clientp);
                if (data->mm->m_download_cancelled.load()) {
                    return 1;  // Abort
                }
                // Update progress callback (only if we have a total size)
                if (dltotal > 0) {
                    float progress = static_cast<float>(dlnow) / static_cast<float>(dltotal);
                    ModelStatus status;
                    status.model_id = data->model_id;  // Include model_id so UI knows which progress bar to update
                    status.state = ModelState::DOWNLOADING;
                    status.progress = progress;
                    status.downloaded_bytes = static_cast<uint64_t>(dlnow);
                    status.total_bytes = static_cast<uint64_t>(dltotal);
                    status.download_speed = 0.0f;
                    data->mm->notify_status_change(status);
                }
                return 0;  // Continue download
            });
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, callback_data);
            
            // Enable progress callback - MUST be set after function/data
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            
            // Suppress signals to avoid interference with callbacks
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
            
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
            
            LUCID_DEBUG("[ModelManager] Performing curl download from: %s", url.c_str());
            CURLcode res = curl_easy_perform(curl);
            LUCID_DEBUG("[ModelManager] curl_easy_perform returned: %d", (int)res);
            
            if (res == CURLE_OK) {
                curl_off_t size = 0;
                curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &size);
                
                LUCID_INFO("[ModelManager] Download complete: %s (%" CURL_FORMAT_CURL_OFF_T " bytes into buffer)", 
                          model_id.c_str(), size);
                
                // Write buffer to file
                if (buffer.write_to_file(output_path)) {
                    success = true;
                    LUCID_INFO("[ModelManager] Wrote %zu bytes to: %s", buffer.size(), output_path.c_str());
                } else {
                    error = "Failed to write buffer to file";
                    success = false;
                    LUCID_ERROR("[ModelManager] Failed to write file: %s", output_path.c_str());
                }
            } else if (res == CURLE_ABORTED_BY_CALLBACK) {
                error = "Download cancelled by user";
                success = false;
                LUCID_WARN("[ModelManager] Download cancelled: %s", model_id.c_str());
            } else {
                error = curl_easy_strerror(res);
                success = false;
                LUCID_ERROR("[ModelManager] Download failed: %s - %s", model_id.c_str(), error.c_str());
            }
            
            curl_easy_cleanup(curl);
            
            // Free callback data after curl cleanup
            delete callback_data;
        } else {
            error = "Failed to initialize curl";
            success = false;
            LUCID_ERROR("[ModelManager] Failed to initialize curl");
        }
        
        // Update model state
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_catalog_index.find(model_id);
            if (it != m_catalog_index.end()) {
                if (success) {
                    it->second->state = ModelState::DOWNLOADED;
                    it->second->download_progress = 1.0f;
                    it->second->local_path = output_path;
                    LUCID_INFO("[ModelManager] Model marked as DOWNLOADED: %s", model_id.c_str());
                } else {
                    it->second->state = ModelState::ERROR;
                    it->second->error_message = error;
                    LUCID_ERROR("[ModelManager] Model marked as ERROR: %s - %s", model_id.c_str(), error.c_str());
                }
            }
            m_current_download_id.clear();
        }
        
        // Notify UI
        LUCID_DEBUG("[ModelManager] Sending final status to callbacks: state=%s, progress=%.1f%%",
                    success ? "DOWNLOADED" : "ERROR",
                    success ? 100.0f : 0.0f);
        ModelStatus final_status;
        final_status.model_id = model_id;
        final_status.state = success ? ModelState::DOWNLOADED : ModelState::ERROR;
        final_status.progress = success ? 1.0f : 0.0f;
        final_status.download_speed = 0.0f;
        final_status.downloaded_bytes = success ? model_size : 0;
        final_status.total_bytes = model_size;
        final_status.error = error;
        notify_status_change(final_status);
        
        // Check if all required models are now ready
        if (success) {
            bool whisper_ready = has_whisper_model();
            bool embedding_ready = has_embedding_model();
            LUCID_DEBUG("[ModelManager] All models ready check: whisper=%s, embedding=%s",
                        whisper_ready ? "yes" : "no",
                        embedding_ready ? "yes" : "no");
            for (auto& cb : m_ready_callbacks) {
                cb(whisper_ready, embedding_ready);
            }
        }
        
        LUCID_DEBUG("[ModelManager] Download thread finished for: %s", model_id.c_str());
    });
    
    LUCID_DEBUG("[ModelManager] Thread created, about to detach...");
    m_download_thread.detach();
    LUCID_DEBUG("[ModelManager] Thread detached, returning true");
    return true;
}

void ModelManager::cancel_download(const std::string& model_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    LUCID_DEBUG("[ModelManager] cancel_download() called for: %s", model_id.c_str());
    LUCID_DEBUG("[ModelManager] Current download: '%s'", m_current_download_id.c_str());
    
    if (m_current_download_id == model_id) {
        m_download_cancelled = true;
        LUCID_INFO("[ModelManager] Cancelled download: %s", model_id.c_str());
    } else {
        LUCID_DEBUG("[ModelManager] No active download to cancel for: %s", model_id.c_str());
    }
}

bool ModelManager::delete_model(const std::string& model_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    LUCID_DEBUG("[ModelManager] delete_model() called for: %s", model_id.c_str());
    
    auto it = m_catalog_index.find(model_id);
    if (it == m_catalog_index.end()) {
        LUCID_WARN("[ModelManager] Model not found in catalog: %s", model_id.c_str());
        return false;
    }
    
    ModelDescriptor* model = it->second;
    LUCID_DEBUG("[ModelManager] Model state: %d", (int)model->state);
    
    if (model->state != ModelState::DOWNLOADED) {
        LUCID_WARN("[ModelManager] Cannot delete model in state: %d", (int)model->state);
        return false;
    }
    
    std::error_code ec;
    LUCID_DEBUG("[ModelManager] Deleting file: %s", model->local_path.c_str());
    if (!model->local_path.empty()) {
        bool removed = std::filesystem::remove(model->local_path, ec);
        if (removed) {
            LUCID_INFO("[ModelManager] File deleted: %s", model->local_path.c_str());
        } else {
            LUCID_WARN("[ModelManager] Failed to delete file: %s (error: %s)", 
                      model->local_path.c_str(), ec.message().c_str());
        }
    }
    
    model->state = ModelState::NOT_DOWNLOADED;
    model->local_path.clear();
    model->download_progress = 0.0f;
    
    LUCID_INFO("[ModelManager] Deleted model: %s", model_id.c_str());
    
    // Notify UI
    LUCID_DEBUG("[ModelManager] Sending NOT_DOWNLOADED status to callbacks");
    notify_status_change(ModelStatus{model_id, ModelState::NOT_DOWNLOADED, 0.0f, 0.0f, 0, 0, ""});
    
    return true;
}

// ============================================================================
// Callbacks
// ============================================================================

void ModelManager::set_on_status_change(ModelStatusCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_status_callbacks.push_back(callback);
}

void ModelManager::set_on_models_ready(AllModelsReadyCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ready_callbacks.push_back(callback);
}

void ModelManager::notify_status_change(const ModelStatus& status) {
    // Capture status by value for the lambda
    ModelStatus status_copy = status;
    bool is_downloading = (status.state == ModelState::DOWNLOADING);
    std::string current_model_id = status.model_id;
    
    // Dispatch to main thread for UI callbacks (avoids GTK thread safety issues)
    // For DOWNLOADING progress, batch updates - only notify every 0.5% to reduce overhead
    static float last_progress_notified = -1.0f;
    static std::string last_model_id_notified;
    
    bool should_notify = true;
    if (is_downloading) {
        // Throttle progress updates to every 0.5%
        if (current_model_id == last_model_id_notified && 
            std::abs(status.progress - last_progress_notified) < 0.005f) {
            should_notify = false;
        }
    }
    
    if (should_notify) {
        g_idle_add_full(G_PRIORITY_DEFAULT, +[](void* data) -> gboolean {
            auto* status_ptr = static_cast<ModelStatus*>(data);
            ModelManager& mm = ModelManager::instance();
            std::lock_guard<std::mutex> lock(mm.m_mutex);
            for (auto& cb : mm.m_status_callbacks) {
                cb(*status_ptr);
            }
            delete status_ptr;
            return G_SOURCE_REMOVE;
        }, new ModelStatus(status_copy), nullptr);
        
        if (is_downloading) {
            last_progress_notified = status.progress;
            last_model_id_notified = current_model_id;
        }
    }
}

// ============================================================================
// First-run Detection
// ============================================================================

bool ModelManager::is_first_run() const {
    std::error_code ec;
    return !std::filesystem::exists(m_config_path, ec);
}

bool ModelManager::has_any_downloaded_models() const {
    for (const auto& model : m_catalog) {
        if (model.state == ModelState::DOWNLOADED) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// Utilities
// ============================================================================

const ModelDescriptor* ModelManager::get_model(const std::string& model_id) const {
    auto it = m_catalog_index.find(model_id);
    if (it != m_catalog_index.end()) {
        return it->second;
    }
    return nullptr;
}

std::string ModelManager::format_size(size_t bytes) {
    char buf[32];
    if (bytes >= 1024ULL * 1024 * 1024) {
        snprintf(buf, sizeof(buf), "%.1f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= 1024 * 1024) {
        snprintf(buf, sizeof(buf), "%.0f MB", bytes / (1024.0 * 1024.0));
    } else if (bytes >= 1024) {
        snprintf(buf, sizeof(buf), "%.0f KB", bytes / 1024.0);
    } else {
        snprintf(buf, sizeof(buf), "%zu B", bytes);
    }
    return buf;
}

} // namespace lucid
