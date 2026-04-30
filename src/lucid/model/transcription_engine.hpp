// transcription_engine.hpp - Whisper.cpp Transcription Engine
//
// Speech-to-text using whisper.cpp

#pragma once

#include "database.hpp"

#include <string>
#include <vector>
#include <optional>
#include <functional>

// Whisper C API - only include if headers are available
#ifdef LUCID_HAVE_WHISPER
extern "C" {
#include <whisper.h>
}
#endif

namespace lucid {

// ============================================================================
// Data Structures (using TranscriptionSegment from database.hpp)
// ============================================================================

// TranscriptionSegment is defined in database.hpp

struct TranscriptionResult {
    std::string video_id;
    std::vector<TranscriptionSegment> segments;
    std::string detected_language;
    int duration_ms = 0;
};

// ============================================================================
// Transcription Engine
// ============================================================================

class TranscriptionEngine {
public:
    TranscriptionEngine();
    ~TranscriptionEngine();
    
    // Check if engine is initialized
    bool is_initialized() const { return m_initialized; }
    
    // Initialize with model path
    bool initialize(const std::string& model_path);
    
    // Transcribe audio file
    std::optional<TranscriptionResult> transcribe(
        const std::string& audio_path,
        const std::string& video_id,
        std::function<void(double)> progress_callback = nullptr);
    
    // Set language for transcription
    void set_language(const std::string& lang);
    
    // Get/set number of threads
    int get_n_threads() const { return m_n_threads; }
    void set_n_threads(int n) { m_n_threads = n; }
    
    // Get/set context size (for whisper, this is n_audio_ctx)
    int get_context_size() const { return m_context_size; }
    void set_context_size(int n) { m_context_size = n; }
    
    // Get/set GPU usage
    bool get_use_gpu() const { return m_use_gpu; }
    void set_use_gpu(bool use) { m_use_gpu = use; }
    
    // Get/set GPU layers for whisper
    int get_gpu_layers() const { return m_gpu_layers; }
    void set_gpu_layers(int layers) { m_gpu_layers = layers; }
    
    // Get/set GPU layers for whisper
    int get_whisper_gpu_layers() const { return m_gpu_layers; }
    void set_whisper_gpu_layers(int layers) { m_gpu_layers = layers; }
    
    // Language detection
    std::string detect_language(const std::string& audio_path);
    
    // Model path management
    static std::string get_default_model_path();
    static std::vector<std::string> get_available_models();

private:
    bool m_initialized = false;
    int m_n_threads = 4;
    int m_context_size = 16384;  // whisper audio context size
    bool m_use_gpu = false;       // GPU acceleration
    int m_gpu_layers = 0;        // GPU layers for whisper (0 = auto)
    std::string m_language = "auto";
    std::string m_model_path;
    
#ifdef LUCID_HAVE_WHISPER
    // Internal whisper context
    struct whisper_context* m_ctx = nullptr;
#else
    void* m_ctx = nullptr;
#endif
};

} // namespace lucid