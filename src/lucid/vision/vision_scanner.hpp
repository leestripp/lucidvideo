// vision_scanner.hpp - Vision Language Model Scanner
//
// Uses Qwen2-VL 2B to analyze video thumbnail images and extract
// visual information for enhanced search capabilities.

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <chrono>
#include <filesystem>

// Include llama/mtmd types (must be before lucid namespace)
#include <mtmd.h>

namespace lucid {

// ============================================================================
// Vision Analysis Result
// ============================================================================

struct VisionScanResult {
    std::string thumbnail_path;       // Path to the analyzed thumbnail
    int64_t timestamp_ms = 0;       // Timestamp in video
    std::string description;         // Natural language description
    std::vector<std::string> objects; // Detected objects
    std::string scene_type;         // indoor/outdoor/screen_capture/etc.
    std::string text_content;       // Any visible text in frame
    float confidence = 1.0f;
    int64_t processing_time_ms = 0;
    bool success = false;
    std::string error_message;
};

// ============================================================================
// Vision Scanner Class
// ============================================================================

class VisionScanner {
public:
    VisionScanner();
    ~VisionScanner();
    
    // Prevent copying (holds GPU/model state)
    VisionScanner(const VisionScanner&) = delete;
    VisionScanner& operator=(const VisionScanner&) = delete;
    
    // Initialize with model paths
    bool initialize(const std::string& model_path, 
                    const std::string& mmproj_path);
    
    // Analyze a single thumbnail image
    std::optional<VisionScanResult> analyze_image(
        const std::string& image_path,
        const std::string& prompt = "");
    
    // Batch process multiple images for efficiency
    std::vector<VisionScanResult> analyze_batch(
        const std::vector<std::pair<std::string, int64_t>>& images,
        int batch_size = 4,
        std::function<void(int, int)> progress_callback = nullptr);
    
    // Check if scanner is ready
    bool is_loaded() const { return m_model != nullptr && m_ctx_vision != nullptr; }
    
    // Get model identifier
    std::string get_model_id() const { return m_model_id; }
    
    // Performance configuration (call before initialize)
    void set_thread_count(int n_threads);
    void set_batch_size(int n_batch);
    
    // Pre-warm the model with a dummy inference for faster first use
    bool prewarm();
    
    // Shutdown and free resources
    void shutdown();
    
    // Default analysis prompt
    static constexpr const char* DEFAULT_PROMPT = R"(
Analyze this video frame and describe:
1. Scene type: indoor, outdoor, screen_capture, motion_graphics, studio, etc.
2. Main subjects and objects visible
3. Any text or graphics overlaid
4. Key visual elements useful for video search indexing

Provide a concise but descriptive analysis suitable for video search. Be specific about what is shown.
)";

private:
    // Internal helpers
    bool load_model(const std::string& model_path);
    bool init_vision_context(const std::string& mmproj_path);
    bool load_image(const std::string& image_path);
    std::string generate_response(int max_tokens = 256);
    VisionScanResult parse_and_format_response(
        const std::string& image_path, 
        int64_t timestamp_ms,
        const std::string& response);
    
    // Llama/MTMD state - using void* to avoid conflicts with global namespace types
    void* m_model = nullptr;
    void* m_context = nullptr;
    void* m_ctx_vision = nullptr;
    void* m_current_bitmap = nullptr;
    
    // Configuration
    std::string m_model_id;
    int m_n_threads = 4;
    int m_n_batch = 512;
    int m_n_ctx = 4096;
    int m_n_past = 0;  // Position in KV cache
    
    // Performance tracking
    int m_tokens_generated = 0;
    std::chrono::steady_clock::time_point m_start_time;
};

// ============================================================================
// Helper: Find mmproj file for a model
// ============================================================================

inline std::string find_mmproj_for_model(const std::string& model_path) {
    std::filesystem::path model_fs(model_path);
    std::filesystem::path dir = model_fs.parent_path();
    
    std::vector<std::string> patterns = {
        model_fs.stem().string() + "-mmproj.gguf",
        "qwen2-vl-2b-mmproj.gguf",
        "mmproj.gguf",
        "qwen2-vl-mmproj.gguf"
    };
    
    for (const auto& pattern : patterns) {
        std::filesystem::path candidate = dir / pattern;
        if (std::filesystem::exists(candidate)) {
            return candidate.string();
        }
    }
    
    return "";
}

} // namespace lucid