// app_settings.cpp - Unified Application Settings Implementation

#include "app_settings.hpp"
#include "../logging.hpp"

#include <giomm.h>
#include <glib.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace lucid {

// ============================================================================
// Singleton Instance
// ============================================================================

AppSettings& AppSettings::instance() {
    static AppSettings instance;
    return instance;
}

// ============================================================================
// Constructor
// ============================================================================

AppSettings::AppSettings() {
    // Set default transcriptions directory
    const char* home = std::getenv("HOME");
    if (home) {
        m_transcriptions_dir = std::string(home) + "/.local/share/lucid/transcriptions";
    } else {
        m_transcriptions_dir = "/tmp/lucid_transcriptions";
    }
    
    // Load settings from disk
    load();
}

// ============================================================================
// Config Path
// ============================================================================

std::string AppSettings::get_config_path() const {
    return Glib::build_filename(Glib::get_user_config_dir(), "lucid", "settings.json");
}

// ============================================================================
// Load/Save
// ============================================================================

void AppSettings::load() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string config_path = get_config_path();
    
    if (!std::filesystem::exists(config_path)) {
        LUCID_INFO("[AppSettings] No config file found at %s, using defaults", config_path.c_str());
        reset_to_defaults();
        m_loaded.store(true);
        return;
    }
    
    try {
        std::ifstream file(config_path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        
        LUCID_DEBUG("[AppSettings] Loading config from %s", config_path.c_str());
        LUCID_DEBUG("[AppSettings] Config content length: %zu bytes", content.size());
        
        // Helper: extract string value
        auto find_string = [&content](const char* key) -> std::string {
            std::string search = std::string("\"") + key + "\"";
            size_t pos = content.find(search);
            if (pos == std::string::npos) return "";
            
            pos = content.find(':', pos);
            if (pos == std::string::npos) return "";
            
            pos = content.find('"', pos + 1);
            if (pos == std::string::npos) return "";
            
            size_t end = content.find('"', pos + 1);
            if (end == std::string::npos) return "";
            
            return content.substr(pos + 1, end - pos - 1);
        };
        
        // Helper: extract int value
        auto find_int = [&content](const char* key, int default_val) -> int {
            std::string search = std::string("\"") + key + "\"";
            size_t pos = content.find(search);
            if (pos == std::string::npos) return default_val;
            
            pos = content.find(':', pos);
            if (pos == std::string::npos) return default_val;
            
            // Skip whitespace
            while (pos < content.size() && 
                   (content[pos] == ' ' || content[pos] == '\t' || 
                    content[pos] == '\n' || content[pos] == '\r')) pos++;
            
            // Find end of number
            size_t end = pos;
            while (end < content.size() && 
                   (isdigit(content[end]) || content[end] == '-')) end++;
            
            if (end == pos) return default_val;
            
            try {
                return std::stoi(content.substr(pos, end - pos));
            } catch (...) {
                return default_val;
            }
        };
        
        // Helper: extract bool value
        auto find_bool = [&content](const char* key, bool default_val) -> bool {
            std::string search = std::string("\"") + key + "\"";
            size_t pos = content.find(search);
            if (pos == std::string::npos) return default_val;
            
            pos = content.find(':', pos);
            if (pos == std::string::npos) return default_val;
            
            // Skip whitespace
            while (pos < content.size() && 
                   (content[pos] == ' ' || content[pos] == '\t' || 
                    content[pos] == '\n' || content[pos] == '\r')) pos++;
            
            if (pos + 4 <= content.size() && content.substr(pos, 4) == "true") return true;
            if (pos + 5 <= content.size() && content.substr(pos, 5) == "false") return false;
            
            return default_val;
        };
        
        // Helper: extract string array
        auto find_string_array = [&content](const char* key) -> std::vector<std::string> {
            std::vector<std::string> result;
            std::string search = std::string("\"") + key + "\"";
            size_t pos = content.find(search);
            if (pos == std::string::npos) return result;
            
            // Find opening bracket
            pos = content.find('[', pos);
            if (pos == std::string::npos) return result;
            pos++;
            
            // Find closing bracket
            size_t end = content.find(']', pos);
            if (end == std::string::npos) return result;
            
            // Extract strings between brackets
            std::string array_content = content.substr(pos, end - pos);
            size_t search_pos = 0;
            while (search_pos < array_content.size()) {
                size_t quote1 = array_content.find('"', search_pos);
                if (quote1 == std::string::npos) break;
                size_t quote2 = array_content.find('"', quote1 + 1);
                if (quote2 == std::string::npos) break;
                result.push_back(array_content.substr(quote1 + 1, quote2 - quote1 - 1));
                search_pos = quote2 + 1;
            }
            
            return result;
        };
        
        // Load all settings
        m_whisper_model = find_string("whisper_model");
        m_embedding_model = find_string("embedding_model");
        m_llm_model = find_string("llm_model");
        
        m_use_gpu = find_bool("use_gpu", true);
        m_max_parallel_jobs = find_int("max_parallel_jobs", 4);
        m_embedding_context_size = find_int("embedding_context_size", 2048);
        m_whisper_context_size = find_int("whisper_context_size", 16384);
        m_thumbnail_size = static_cast<ThumbnailSize>(find_int("thumbnail_size", 1));
        m_transcriptions_dir = find_string("transcriptions_dir");
        m_watched_folders = find_string_array("watched_folders");
        
        // Vision scan is always enabled (no toggle)
        // Scan every 2nd thumbnail for good coverage
        m_vision_scan_interval = find_int("vision_scan_interval", 2);
        m_vision_max_per_video = find_int("vision_max_per_video", 25);
        m_vision_extract_size = static_cast<VisionExtractSize>(find_int("vision_extract_size", 1));
        
        m_video_page_size = find_int("video_page_size", 50);
        
        // Validate and set defaults for empty values
        if (m_whisper_model.empty()) m_whisper_model = "whisper-base";
        if (m_embedding_model.empty()) m_embedding_model = "nomic-embed-text";
        if (m_transcriptions_dir.empty()) {
            const char* home = std::getenv("HOME");
            if (home) {
                m_transcriptions_dir = std::string(home) + "/.local/share/lucid/transcriptions";
            }
        }
        
        LUCID_INFO("[AppSettings] Loaded config:");
        LUCID_INFO("  - whisper_model: %s", m_whisper_model.c_str());
        LUCID_INFO("  - embedding_model: %s", m_embedding_model.c_str());
        LUCID_INFO("  - llm_model: %s", m_llm_model.c_str());
        LUCID_INFO("  - use_gpu: %s", m_use_gpu ? "true" : "false");
        LUCID_INFO("  - max_parallel_jobs: %d", m_max_parallel_jobs);
        LUCID_INFO("  - vision_scan: ALWAYS ENABLED (hardcoded)");
        LUCID_INFO("  - vision_scan_interval: %d", m_vision_scan_interval);
        LUCID_INFO("  - vision_max_per_video: %d", m_vision_max_per_video);
        LUCID_INFO("  - vision_extract_size: %d (%dpx)", 
                   static_cast<int>(m_vision_extract_size),
                   get_vision_extract_pixels(m_vision_extract_size));
        
        m_loaded.store(true);
        
    } catch (const std::exception& e) {
        LUCID_ERROR("[AppSettings] Exception loading config: %s", e.what());
        reset_to_defaults();
        m_loaded.store(true);
    }
}

void AppSettings::save() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string config_dir = Glib::build_filename(Glib::get_user_config_dir(), "lucid");
    std::error_code ec;
    std::filesystem::create_directories(config_dir, ec);
    if (ec) {
        LUCID_ERROR("[AppSettings] Failed to create config directory: %s", ec.message().c_str());
        return;
    }
    
    std::string config_path = get_config_path();
    LUCID_INFO("[AppSettings] save() called, path: %s", config_path.c_str());
    
    try {
        std::ofstream file(config_path);
        if (!file.is_open()) {
            LUCID_ERROR("[AppSettings] Failed to open file for writing: %s", config_path.c_str());
            return;
        }
        
        file << "{\n";
        
        // Model selections
        file << "  \"whisper_model\": \"" << m_whisper_model << "\",\n";
        file << "  \"embedding_model\": \"" << m_embedding_model << "\",\n";
        file << "  \"llm_model\": \"" << m_llm_model << "\",\n";
        
        // Performance
        file << "  \"use_gpu\": " << (m_use_gpu ? "true" : "false") << ",\n";
        file << "  \"max_parallel_jobs\": " << m_max_parallel_jobs << ",\n";
        
        // Context sizes
        file << "  \"embedding_context_size\": " << m_embedding_context_size << ",\n";
        file << "  \"whisper_context_size\": " << m_whisper_context_size << ",\n";
        
        // UI
        file << "  \"thumbnail_size\": " << static_cast<int>(m_thumbnail_size) << ",\n";
        file << "  \"transcriptions_dir\": \"" << m_transcriptions_dir << "\",\n";
        
        // Watched folders
        file << "  \"watched_folders\": [";
        for (size_t i = 0; i < m_watched_folders.size(); i++) {
            file << "\"" << m_watched_folders[i] << "\"";
            if (i < m_watched_folders.size() - 1) file << ", ";
        }
        file << "],\n";
        
        // Vision settings (always enabled - no toggle saved)
        file << "  \"vision_scan_interval\": " << m_vision_scan_interval << ",\n";
        file << "  \"vision_max_per_video\": " << m_vision_max_per_video << ",\n";
        file << "  \"vision_extract_size\": " << static_cast<int>(m_vision_extract_size) << ",\n";
        
        // Pagination
        file << "  \"video_page_size\": " << m_video_page_size << "\n";
        
        file << "}\n";
        
        file.close();
        
        LUCID_INFO("[AppSettings] Config saved successfully to %s", config_path.c_str());
        
    } catch (const std::exception& e) {
        LUCID_ERROR("[AppSettings] Exception saving config: %s", e.what());
    }
}

void AppSettings::reset_to_defaults() {
    LUCID_DEBUG("[AppSettings] Resetting to defaults");
    
    m_whisper_model = "whisper-base";
    m_embedding_model = "nomic-embed-text";
    m_llm_model = "qwen2-vl-2b";
    
    m_use_gpu = true;
    m_max_parallel_jobs = 4;
    
    m_embedding_context_size = 2048;
    m_whisper_context_size = 16384;
    
    m_thumbnail_size = ThumbnailSize::MEDIUM;
    m_watched_folders.clear();
    
    // Vision scan is always enabled
    // Scan every 2nd thumbnail for good coverage
    m_vision_scan_interval = 2;
    m_vision_max_per_video = 25;
    m_vision_extract_size = VisionExtractSize::MEDIUM;
    
    m_video_page_size = 50;
    
    const char* home = std::getenv("HOME");
    if (home) {
        m_transcriptions_dir = std::string(home) + "/.local/share/lucid/transcriptions";
    }
}

// ============================================================================
// Signal System
// ============================================================================

void AppSettings::on_changed(SignalType type, Callback callback) {
    std::lock_guard<std::mutex> lock(m_callback_mutex);
    m_callbacks.push_back({type, std::move(callback)});
}

void AppSettings::emit_signal(SignalType type) {
    LUCID_INFO("AppSettings::emit_signal called with type=%d", (int)type);
    std::lock_guard<std::mutex> lock(m_callback_mutex);
    LUCID_INFO("AppSettings::emit_signal: lock acquired, %d callbacks registered", (int)m_callbacks.size());
    for (const auto& [cb_type, callback] : m_callbacks) {
        if (cb_type == type || cb_type == SignalType::ANY_CHANGED) {
            LUCID_INFO("AppSettings::emit_signal: calling callback");
            callback();
            LUCID_INFO("AppSettings::emit_signal: callback returned");
        }
    }
    LUCID_INFO("AppSettings::emit_signal done");
}

void AppSettings::clear_callbacks() {
    std::lock_guard<std::mutex> lock(m_callback_mutex);
    m_callbacks.clear();
}

// ============================================================================
// Setters with Auto-Save
// ============================================================================

void AppSettings::set_whisper_model(const std::string& model_id) {
    LUCID_INFO("AppSettings::set_whisper_model called with: %s", model_id.c_str());
    std::lock_guard<std::mutex> lock(m_mutex);
    LUCID_INFO("AppSettings::set_whisper_model: lock acquired");
    if (m_whisper_model != model_id) {
        m_whisper_model = model_id;
        save();
        emit_signal(SignalType::MODEL_CHANGED);
        emit_signal(SignalType::ANY_CHANGED);
    }
    LUCID_INFO("AppSettings::set_whisper_model done");
}

void AppSettings::set_embedding_model(const std::string& model_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_embedding_model != model_id) {
        m_embedding_model = model_id;
        save();
        emit_signal(SignalType::MODEL_CHANGED);
        emit_signal(SignalType::ANY_CHANGED);
    }
}

void AppSettings::set_llm_model(const std::string& model_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_llm_model != model_id) {
        m_llm_model = model_id;
        save();
        emit_signal(SignalType::MODEL_CHANGED);
        emit_signal(SignalType::ANY_CHANGED);
    }
}

void AppSettings::set_use_gpu(bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_use_gpu != enabled) {
        m_use_gpu = enabled;
        save();
        emit_signal(SignalType::GPU_CHANGED);
        emit_signal(SignalType::ANY_CHANGED);
    }
}

void AppSettings::set_max_parallel_jobs(int jobs) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_max_parallel_jobs != jobs) {
        m_max_parallel_jobs = std::max(1, std::min(16, jobs));
        save();
        emit_signal(SignalType::GPU_CHANGED);
        emit_signal(SignalType::ANY_CHANGED);
    }
}

void AppSettings::set_embedding_context_size(int size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_embedding_context_size != size) {
        m_embedding_context_size = std::max(512, std::min(8192, size));
        save();
        emit_signal(SignalType::CONTEXT_SIZE_CHANGED);
        emit_signal(SignalType::ANY_CHANGED);
    }
}

void AppSettings::set_whisper_context_size(int size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_whisper_context_size != size) {
        m_whisper_context_size = std::max(4096, std::min(32768, size));
        save();
        emit_signal(SignalType::CONTEXT_SIZE_CHANGED);
        emit_signal(SignalType::ANY_CHANGED);
    }
}

void AppSettings::set_thumbnail_size(ThumbnailSize size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_thumbnail_size != size) {
        m_thumbnail_size = size;
        save();
        emit_signal(SignalType::THUMBNAIL_SIZE_CHANGED);
        emit_signal(SignalType::ANY_CHANGED);
    }
}

void AppSettings::set_thumbnail_size_int(int size) {
    set_thumbnail_size(static_cast<ThumbnailSize>(std::max(0, std::min(2, size))));
}

void AppSettings::add_watched_folder(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (std::find(m_watched_folders.begin(), m_watched_folders.end(), path) == m_watched_folders.end()) {
        m_watched_folders.push_back(path);
        save();
        emit_signal(SignalType::FOLDERS_CHANGED);
        emit_signal(SignalType::ANY_CHANGED);
    }
}

void AppSettings::remove_watched_folder(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::find(m_watched_folders.begin(), m_watched_folders.end(), path);
    if (it != m_watched_folders.end()) {
        m_watched_folders.erase(it);
        save();
        emit_signal(SignalType::FOLDERS_CHANGED);
        emit_signal(SignalType::ANY_CHANGED);
    }
}

void AppSettings::set_watched_folders(const std::vector<std::string>& folders) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_watched_folders = folders;
    save();
    emit_signal(SignalType::FOLDERS_CHANGED);
    emit_signal(SignalType::ANY_CHANGED);
}

void AppSettings::set_vision_scan_interval(int interval) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_vision_scan_interval != interval) {
        m_vision_scan_interval = std::max(1, std::min(20, interval));
        save();
        emit_signal(SignalType::VISION_CHANGED);
        emit_signal(SignalType::ANY_CHANGED);
    }
}

void AppSettings::set_vision_max_per_video(int max_per_video) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_vision_max_per_video != max_per_video) {
        m_vision_max_per_video = std::max(5, std::min(100, max_per_video));
        save();
        emit_signal(SignalType::VISION_CHANGED);
        emit_signal(SignalType::ANY_CHANGED);
    }
}

void AppSettings::set_vision_extract_size(VisionExtractSize size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_vision_extract_size != size) {
        m_vision_extract_size = size;
        save();
        emit_signal(SignalType::VISION_CHANGED);
        emit_signal(SignalType::ANY_CHANGED);
    }
}

void AppSettings::set_vision_extract_size_int(int size) {
    set_vision_extract_size(static_cast<VisionExtractSize>(std::max(0, std::min(2, size))));
}

void AppSettings::set_transcriptions_dir(const std::string& dir) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_transcriptions_dir != dir) {
        m_transcriptions_dir = dir;
        save();
        emit_signal(SignalType::ANY_CHANGED);
    }
}

void AppSettings::set_video_page_size(int size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Clamp to valid range (same as pagination widget options: 25, 50, 100, 200)
    int clamped = std::max(25, std::min(200, size));
    if (m_video_page_size != clamped) {
        m_video_page_size = clamped;
        save();
        emit_signal(SignalType::PAGINATION_CHANGED);
        emit_signal(SignalType::ANY_CHANGED);
    }
}

} // namespace lucid
