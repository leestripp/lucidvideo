// app_settings.hpp - Unified Application Settings
// Single source of truth for all application settings
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>

namespace lucid {

enum class ThumbnailSize {
    SMALL = 0,
    MEDIUM = 1,
    LARGE = 2
};

// Vision extraction sizes for AI analysis (higher res for better recognition)
enum class VisionExtractSize {
    SMALL = 0,  // 256px - fast but lower quality
    MEDIUM = 1,  // 512px - balanced
    LARGE = 2   // 1024px - best quality, slower
};

enum class SignalType {
    ANY_CHANGED,
    MODEL_CHANGED,
    GPU_CHANGED,
    THUMBNAIL_SIZE_CHANGED,
    FOLDERS_CHANGED,
    VISION_CHANGED,
    CONTEXT_SIZE_CHANGED,
    PAGINATION_CHANGED  // Page size changed
};

class AppSettings {
public:
    // Singleton access
    static AppSettings& instance();
    
    // Prevent copying
    AppSettings(const AppSettings&) = delete;
    AppSettings& operator=(const AppSettings&) = delete;
    
    // ===== Load/Save =====
    void load();
    void save();
    void reset_to_defaults();
    bool is_loaded() const { return m_loaded.load(); }
    
    // ===== Model Settings =====
    const std::string& get_whisper_model() const { return m_whisper_model; }
    const std::string& get_embedding_model() const { return m_embedding_model; }
    const std::string& get_llm_model() const { return m_llm_model; }
    
    void set_whisper_model(const std::string& model_id);
    void set_embedding_model(const std::string& model_id);
    void set_llm_model(const std::string& model_id);
    
    // ===== Performance Settings =====
    bool get_use_gpu() const { return m_use_gpu; }
    int get_max_parallel_jobs() const { return m_max_parallel_jobs; }
    
    void set_use_gpu(bool enabled);
    void set_max_parallel_jobs(int jobs);
    
    // ===== Context Sizes =====
    int get_embedding_context_size() const { return m_embedding_context_size; }
    int get_whisper_context_size() const { return m_whisper_context_size; }
    
    void set_embedding_context_size(int size);
    void set_whisper_context_size(int size);
    
    // ===== UI Settings =====
    ThumbnailSize get_thumbnail_size() const { return m_thumbnail_size; }
    int get_thumbnail_size_int() const { return static_cast<int>(m_thumbnail_size); }
    void set_thumbnail_size(ThumbnailSize size);
    void set_thumbnail_size_int(int size);
    
    const std::vector<std::string>& get_watched_folders() const { return m_watched_folders; }
    void add_watched_folder(const std::string& path);
    void remove_watched_folder(const std::string& path);
    void set_watched_folders(const std::vector<std::string>& folders);
    
    // ===== Vision Settings (ALWAYS ENABLED) =====
    // Vision scan is now hardcoded to always be ON
    bool is_vision_scan_enabled() const { return true; }  // Always returns true
    int get_vision_scan_interval() const { return m_vision_scan_interval; }
    int get_vision_max_per_video() const { return m_vision_max_per_video; }
    
    void set_vision_scan_interval(int interval);
    void set_vision_max_per_video(int max_per_video);
    
    // Vision extraction size for AI analysis (separate from display thumbnail size)
    VisionExtractSize get_vision_extract_size() const { return m_vision_extract_size; }
    int get_vision_extract_size_int() const { return static_cast<int>(m_vision_extract_size); }
    void set_vision_extract_size(VisionExtractSize size);
    void set_vision_extract_size_int(int size);
    
    // ===== Storage Settings =====
    const std::string& get_transcriptions_dir() const { return m_transcriptions_dir; }
    void set_transcriptions_dir(const std::string& dir);
    
    // ===== Pagination Settings =====
    int get_video_page_size() const { return m_video_page_size; }
    void set_video_page_size(int size);
    
    // ===== Signal System =====
    using Callback = std::function<void()>;
    void on_changed(SignalType type, Callback callback);
    void clear_callbacks();
    
private:
    AppSettings();
    ~AppSettings() = default;
    
    void emit_signal(SignalType type);
    std::string get_config_path() const;
    
    // ===== Settings Values =====
    
    // Model selections (model IDs)
    std::string m_whisper_model = "whisper-tiny";
    std::string m_embedding_model = "nomic-embed-text";
    std::string m_llm_model = "qwen2-vl-2b";
    
    // Performance
    bool m_use_gpu = true;
    int m_max_parallel_jobs = 4;
    
    // Context sizes
    int m_embedding_context_size = 2048;
    int m_whisper_context_size = 16384;
    
    // UI
    ThumbnailSize m_thumbnail_size = ThumbnailSize::MEDIUM;
    std::vector<std::string> m_watched_folders;
    
    // Vision scanning (always enabled - no toggle setting)
    int m_vision_scan_interval = 10;
    int m_vision_max_per_video = 25;
    VisionExtractSize m_vision_extract_size = VisionExtractSize::MEDIUM;  // 512px default
    
    // Storage
    std::string m_transcriptions_dir;
    
    // Pagination for video library
    int m_video_page_size = 50;
    
    // ===== Internal State =====
    std::atomic<bool> m_loaded{false};
    std::mutex m_mutex;
    std::mutex m_callback_mutex;
    
    // Signal callbacks indexed by signal type
    std::vector<std::pair<SignalType, Callback>> m_callbacks;
};

// Helper function for thumbnail dimensions (display size)
inline std::pair<int, int> get_thumbnail_dimensions(ThumbnailSize size) {
    switch (size) {
        case ThumbnailSize::SMALL:  return {80, 45};
        case ThumbnailSize::MEDIUM: return {160, 90};
        case ThumbnailSize::LARGE:   return {240, 135};
        default: return {160, 90};
    }
}

// Helper function for vision extraction dimensions (AI analysis size)
inline int get_vision_extract_pixels(VisionExtractSize size) {
    switch (size) {
        case VisionExtractSize::SMALL:  return 256;
        case VisionExtractSize::MEDIUM: return 512;
        case VisionExtractSize::LARGE:  return 1024;
        default: return 512;
    }
}

} // namespace lucid
