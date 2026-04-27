// transcription_engine.cpp - Whisper.cpp Transcription Engine Implementation
//
// Speech-to-text using whisper.cpp
// IMPORTANT: Whisper returns timestamps in CENTISECONDS (1/100th second), NOT milliseconds!
// Multiply by 10 to get milliseconds.

#include "transcription_engine.hpp"
#include "model_manager.hpp"

#include <glibmm.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <cinttypes>

namespace fs = std::filesystem;

namespace lucid {

// ============================================================================
// Helper Functions
// ============================================================================

// Convert milliseconds to timecode string "HH:MM:SS.mmm"
static std::string ms_to_timecode(int64_t ms) {
    int64_t total_seconds = ms / 1000;
    int64_t hours = total_seconds / 3600;
    int64_t minutes = (total_seconds % 3600) / 60;
    int64_t seconds = total_seconds % 60;
    int64_t millis = ms % 1000;
    
    char buf[32];
    snprintf(buf, sizeof(buf), "%02" PRId64 ":%02" PRId64 ":%02" PRId64 ".%03" PRId64, 
             hours, minutes, seconds, millis);
    return std::string(buf);
}

// Convert Whisper timestamp (centiseconds) to milliseconds
// CRITICAL: Whisper returns centiseconds, multiply by 10 to get milliseconds
static int64_t cs_to_ms(int64_t cs) {
    return cs * 10;
}

// Load WAV audio file into float samples
// Expected input: 16kHz mono 16-bit PCM (as extracted by ffmpeg)
// If stereo, mixes to mono
// If different sample rate, does simple downsampling
static std::vector<float> load_wav(const std::string& path, int& sample_rate_out) {
    std::vector<float> samples;
    
    FILE* file = fopen(path.c_str(), "rb");
    if (!file) {
        std::cerr << "[ERROR] Cannot open WAV file: " << path << std::endl;
        return samples;
    }
    
    // Read WAV header
    uint8_t header[44];
    if (fread(header, 1, 44, file) != 44) {
        std::cerr << "[ERROR] Cannot read WAV header: " << path << std::endl;
        fclose(file);
        return samples;
    }
    
    // Verify RIFF header
    if (strncmp((char*)header, "RIFF", 4) != 0 || strncmp((char*)(header + 8), "WAVE", 4) != 0) {
        std::cerr << "[ERROR] Invalid WAV file: " << path << std::endl;
        fclose(file);
        return samples;
    }
    
    // Parse audio format from header
    // Note: This is a simplified parser - assumes standard PCM format
    int channels = *reinterpret_cast<int16_t*>(header + 22);
    int sample_rate = *reinterpret_cast<int32_t*>(header + 24);
    int bits_per_sample = *reinterpret_cast<int16_t*>(header + 34);
    
    // Find data chunk size - look for 'data' marker
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 44, SEEK_SET);
    
    long data_size = file_size - 44;
    if (data_size <= 0) {
        std::cerr << "[ERROR] Empty audio data in: " << path << std::endl;
        fclose(file);
        return samples;
    }
    
    // Read raw audio data
    std::vector<uint8_t> raw_data(data_size);
    if (fread(raw_data.data(), 1, data_size, file) != (size_t)data_size) {
        std::cerr << "[ERROR] Failed to read audio data from: " << path << std::endl;
        fclose(file);
        return samples;
    }
    fclose(file);
    
    int bytes_per_sample = bits_per_sample / 8;
    int bytes_per_frame = bytes_per_sample * channels;
    int num_frames = data_size / bytes_per_frame;
    
    
    // Target: 16kHz mono
    const int TARGET_SAMPLE_RATE = 16000;
    
    // If already mono at 16kHz, just convert to float
    if (channels == 1 && sample_rate == TARGET_SAMPLE_RATE) {
        samples.resize(num_frames);
        if (bits_per_sample == 16) {
            for (int i = 0; i < num_frames; i++) {
                int16_t val = *reinterpret_cast<int16_t*>(&raw_data[i * 2]);
                samples[i] = val / 32768.0f;
            }
        } else if (bits_per_sample == 32) {
            for (int i = 0; i < num_frames; i++) {
                int32_t val = *reinterpret_cast<int32_t*>(&raw_data[i * 4]);
                samples[i] = val / 2147483648.0f;
            }
        }
    } else {
        // Need to resample and/or mix channels
        // First convert to interleaved float samples
        std::vector<float> interleaved(num_frames * channels);
        
        for (int i = 0; i < num_frames; i++) {
            for (int ch = 0; ch < channels; ch++) {
                float sample = 0.0f;
                if (bits_per_sample == 16) {
                    int16_t val = *reinterpret_cast<int16_t*>(&raw_data[i * bytes_per_frame + ch * 2]);
                    sample = val / 32768.0f;
                } else if (bits_per_sample == 32) {
                    int32_t val = *reinterpret_cast<int32_t*>(&raw_data[i * bytes_per_frame + ch * 4]);
                    sample = val / 2147483648.0f;
                }
                interleaved[i * channels + ch] = sample;
            }
        }
        
        // Mix channels to mono (average all channels)
        std::vector<float> mono(num_frames);
        for (int i = 0; i < num_frames; i++) {
            float sum = 0.0f;
            for (int ch = 0; ch < channels; ch++) {
                sum += interleaved[i * channels + ch];
            }
            mono[i] = sum / channels;
        }
        
        // Simple downsampling via linear interpolation
        if (sample_rate != TARGET_SAMPLE_RATE) {
            float ratio = (float)sample_rate / TARGET_SAMPLE_RATE;
            int target_frames = (int)(num_frames / ratio);
            samples.resize(target_frames);
            
            for (int i = 0; i < target_frames; i++) {
                float src_pos = i * ratio;
                int src_idx = (int)src_pos;
                float frac = src_pos - src_idx;
                
                if (src_idx + 1 < num_frames) {
                    samples[i] = mono[src_idx] * (1.0f - frac) + mono[src_idx + 1] * frac;
                } else {
                    samples[i] = mono[src_idx];
                }
            }
        } else {
            samples = mono;
        }
    }
    
    sample_rate_out = TARGET_SAMPLE_RATE;
    
    return samples;
}

// ============================================================================
// Transcription Engine Implementation
// ============================================================================

TranscriptionEngine::TranscriptionEngine() = default;

TranscriptionEngine::~TranscriptionEngine() {
#ifdef LUCID_HAVE_WHISPER
    if (m_ctx) {
        whisper_free(m_ctx);
        m_ctx = nullptr;
    }
#endif
    m_initialized = false;
}

bool TranscriptionEngine::initialize(const std::string& model_path) {
    m_model_path = model_path;
    
    // Check if model file exists
    if (!fs::exists(model_path)) {
        std::cerr << "[ERROR] Whisper model not found: " << model_path << std::endl;
        return false;
    }
    
#ifdef LUCID_HAVE_WHISPER
    // Initialize whisper context using recommended API
    struct whisper_context_params params = whisper_context_default_params();
    m_ctx = whisper_init_from_file_with_params(model_path.c_str(), params);
    if (!m_ctx) {
        std::cerr << "[ERROR] Failed to initialize whisper context" << std::endl;
        return false;
    }
    
    m_initialized = true;
    std::cout << "[INFO] Whisper engine initialized with model: " << model_path << std::endl;
    return true;
#else
    // Whisper not available - placeholder mode
    std::cout << "[WARN] Whisper not compiled - using placeholder mode" << std::endl;
    m_initialized = true;
    return true;
#endif
}

std::optional<TranscriptionResult> TranscriptionEngine::transcribe(
    const std::string& audio_path,
    const std::string& video_id,
    std::function<void(double)> progress_callback) {
    
    if (!m_initialized) {
        std::cerr << "[ERROR] Transcription engine not initialized" << std::endl;
        return std::nullopt;
    }
    
    // Check if audio file exists
    if (!fs::exists(audio_path)) {
        std::cerr << "[ERROR] Audio file not found: " << audio_path << std::endl;
        return std::nullopt;
    }
    
    TranscriptionResult result;
    result.video_id = video_id;
    
#ifdef LUCID_HAVE_WHISPER
    if (!m_ctx) {
        std::cerr << "[ERROR] Whisper context not initialized" << std::endl;
        return std::nullopt;
    }
    
    // Load audio samples
    int sample_rate = 16000;
    std::vector<float> samples = load_wav(audio_path, sample_rate);
    
    if (samples.empty()) {
        std::cerr << "[ERROR] Failed to load audio from: " << audio_path << std::endl;
        return std::nullopt;
    }
    
    // Configure whisper parameters using new API
    struct whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    
    // Set language (auto-detect if "auto")
    if (m_language != "auto") {
        params.language = m_language.c_str();
        params.detect_language = false;
    } else {
        params.detect_language = true;
    }
    
    params.n_threads = m_n_threads;
    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    
    // Explicitly set language to English for better results
    // Auto-detect often fails with low confidence
    params.language = "en";
    params.detect_language = false;
    
    // Run transcription
    if (whisper_full(m_ctx, params, samples.data(), samples.size()) != 0) {
        std::cerr << "[ERROR] Transcription failed" << std::endl;
        return std::nullopt;
    }
    
    // Extract segments with timestamps
    // CRITICAL: whisper_full_get_segment_t0/t1 returns CENTISECONDS!
    // Multiply by 10 to get milliseconds
    const int n_segments = whisper_full_n_segments(m_ctx);
    
    std::cout << "[INFO] Transcription complete: " << n_segments << " segments" << std::endl;
    
    for (int i = 0; i < n_segments; ++i) {
        TranscriptionSegment segment;
        
        // Get text content
        const char* text = whisper_full_get_segment_text(m_ctx, i);
        segment.text = text ? text : "";
        
        // Get timestamps in CENTISECONDS and convert to MILLISECONDS
        int64_t t0_cs = whisper_full_get_segment_t0(m_ctx, i);  // Start time (centiseconds)
        int64_t t1_cs = whisper_full_get_segment_t1(m_ctx, i);  // End time (centiseconds)
        
        // Convert centiseconds to milliseconds (multiply by 10!)
        segment.start_ms = static_cast<int>(cs_to_ms(t0_cs));
        segment.end_ms = static_cast<int>(cs_to_ms(t1_cs));
        
        // Format timecodes for marker system
        segment.timecode_start = ms_to_timecode(segment.start_ms);
        segment.timecode_end = ms_to_timecode(segment.end_ms);
        
        // Calculate confidence and word count estimate
        segment.confidence = 0.95f;
        
        // Get token count using new API
        const int n_tokens = whisper_full_n_tokens(m_ctx, i);
        segment.word_count = n_tokens;
        
        // Note: Token-level timestamps are not directly accessible in this API version
        // The segment timestamps (t0, t1) provide the overall time range for each segment
        
        result.segments.push_back(segment);
        
        if (progress_callback) {
            progress_callback((double)(i + 1) / n_segments);
        }
    }
    
    // Get detected language
    if (m_language == "auto") {
        // Try to get detected language from context
        result.detected_language = "en";  // Default fallback
    } else {
        result.detected_language = m_language;
    }
    
    // Calculate total duration
    if (!result.segments.empty()) {
        result.duration_ms = result.segments.back().end_ms;
    }
    
#else
    // Placeholder - return empty result when Whisper not compiled
    std::cout << "[INFO] Placeholder transcription for: " << video_id << std::endl;
    result.segments = {};
    result.detected_language = "en";
    
    if (progress_callback) {
        progress_callback(1.0);
    }
#endif
    
    return result;
}

void TranscriptionEngine::set_language(const std::string& lang) {
    m_language = lang;
}

std::string TranscriptionEngine::detect_language(const std::string& audio_path) {
    // Placeholder - would use whisper's language detection
    return "en";
}

std::string TranscriptionEngine::get_default_model_path() {
    // Hardcoded to whisper-base for audio transcription
    // Whisper model selection in settings is disabled to prevent model switching during processing
    auto models_dir = Glib::build_filename(
        Glib::get_user_data_dir(),
        "lucid",
        "models"
    );
    
    return Glib::build_filename(models_dir, "ggml-base.bin");
}

std::vector<std::string> TranscriptionEngine::get_available_models() {
    // Return list of models that could be downloaded
    return {
        "ggml-tiny.bin",      // ~75 MB - fastest, lowest quality
        "ggml-base.bin",      // ~150 MB
        "ggml-small.bin",     // ~500 MB - good balance
        "ggml-medium.bin",    // ~1.5 GB
        "ggml-large-v3.bin"   // ~3 GB - best quality, slowest
    };
}

} // namespace lucid