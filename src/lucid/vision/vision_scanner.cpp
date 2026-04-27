// vision_scanner.cpp - Vision Language Model Scanner Implementation
//
// Uses Qwen2-VL 2B to analyze video thumbnail images via llama.cpp mtmd API.

#include "vision_scanner.hpp"
#include "../logging.hpp"

#include <filesystem>
#include <sstream>
#include <cstring>
#include <chrono>
namespace fs = std::filesystem;

#include <llama.h>
#include <mtmd.h>
#include <mtmd-helper.h>

namespace lucid {

// ============================================================================
// Construction / Destruction
// ============================================================================

VisionScanner::VisionScanner() 
    : m_model(nullptr)
    , m_context(nullptr)
    , m_ctx_vision(nullptr)
    , m_current_bitmap(nullptr)
    , m_tokens_generated(0) {
}

VisionScanner::~VisionScanner() {
    shutdown();
}

// ============================================================================
// Initialization
// ============================================================================
bool VisionScanner::initialize(const std::string& model_path, 
                               const std::string& mmproj_path) {
    
    LUCID_INFO("[VisionScanner] Initializing with model: %s", model_path.c_str());
    LUCID_INFO("[VisionScanner] Vision projector: %s", mmproj_path.c_str());
    
    if (!fs::exists(model_path)) {
        LUCID_ERROR("[VisionScanner] Model file not found: %s", model_path.c_str());
        return false;
    }
    
    if (!fs::exists(mmproj_path)) {
        LUCID_ERROR("[VisionScanner] MMProj file not found: %s", mmproj_path.c_str());
        return false;
    }
    
    llama_backend_init();
    mtmd_helper_log_set(nullptr, nullptr);
    
    if (!load_model(model_path)) {
        LUCID_ERROR("[VisionScanner] Failed to load model");
        return false;
    }
    
    if (!init_vision_context(mmproj_path)) {
        LUCID_ERROR("[VisionScanner] Failed to initialize vision context");
        return false;
    }
    
    fs::path model_fs(model_path);
    m_model_id = model_fs.filename().string();
    
    LUCID_INFO("[VisionScanner] Initialization complete");
    return true;
}

bool VisionScanner::load_model(const std::string& model_path) {
    LUCID_DEBUG("[VisionScanner] Loading text model: %s", model_path.c_str());
    
    // For vision models (Qwen2-VL), we load the TEXT model first
    // The mmproj is a vision projector that gets initialized via mtmd_init_from_file()
    struct llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = 99;
    
    auto model = llama_model_load_from_file(model_path.c_str(), mparams);
    if (!model) {
        LUCID_ERROR("[VisionScanner] llama_model_load_from_file failed for: %s", model_path.c_str());
        return false;
    }
    m_model = static_cast<void*>(model);
    
    // Create context for text generation
    struct llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx = m_n_ctx;
    cparams.n_batch = m_n_batch;
    cparams.n_threads = m_n_threads;
    cparams.n_threads_batch = m_n_threads;
    
    auto ctx = llama_init_from_model(model, cparams);
    if (!ctx) {
        LUCID_ERROR("[VisionScanner] llama_init_from_model failed");
        return false;
    }
    m_context = static_cast<void*>(ctx);
    
    LUCID_DEBUG("[VisionScanner] Text model loaded: vocab_size=%d, ctx_size=%d",
                llama_vocab_n_tokens(llama_model_get_vocab(model)), llama_n_ctx(ctx));
    
    return true;
}

bool VisionScanner::init_vision_context(const std::string& mmproj_path) {
    LUCID_DEBUG("[VisionScanner] Initializing vision context: %s", mmproj_path.c_str());
    
    auto model = static_cast<llama_model*>(m_model);
    
    struct mtmd_context_params mparams = mtmd_context_params_default();
    mparams.use_gpu = true;
    mparams.n_threads = m_n_threads;
    mparams.warmup = true;
    // Qwen2-VL requires minimum 1024 image tokens for proper operation
    mparams.image_min_tokens = 1024;
    
    auto ctx = mtmd_init_from_file(mmproj_path.c_str(), model, mparams);
    if (!ctx) {
        LUCID_ERROR("[VisionScanner] mtmd_init_from_file failed");
        return false;
    }
    m_ctx_vision = static_cast<void*>(ctx);
    
    LUCID_DEBUG("[VisionScanner] Vision context initialized");
    return true;
}

bool VisionScanner::load_image(const std::string& image_path) {
    auto ctx = static_cast<mtmd_context*>(m_ctx_vision);
    
    if (m_current_bitmap) {
        mtmd_bitmap_free(static_cast<mtmd_bitmap*>(m_current_bitmap));
        m_current_bitmap = nullptr;
    }
    
    auto bmp = mtmd_helper_bitmap_init_from_file(ctx, image_path.c_str());
    if (!bmp) {
        LUCID_DEBUG("[VisionScanner] Failed to load image: %s", image_path.c_str());
        return false;
    }
    
    m_current_bitmap = static_cast<void*>(bmp);
    return true;
}

// ============================================================================
// Single Image Analysis
// ============================================================================

std::optional<VisionScanResult> VisionScanner::analyze_image(
    const std::string& image_path,
    const std::string& prompt) {
    
    VISION_DEBUG("=== Analyzing image: %s ===", image_path.c_str());
    
    if (!is_loaded()) {
        VISION_DEBUG("Scanner not loaded - returning nullopt");
        LUCID_WARN("[VisionScanner] Scanner not loaded");
        return std::nullopt;
    }
    
    auto start = std::chrono::steady_clock::now();
    VISION_DEBUG("Loading image...");
    
    if (!load_image(image_path)) {
        VISION_DEBUG("Failed to load image");
        VisionScanResult result;
        result.thumbnail_path = image_path;
        result.success = false;
        result.error_message = "Failed to load image";
        return result;
    }
    VISION_DEBUG("Image loaded successfully");
    
    auto ctx = static_cast<mtmd_context*>(m_ctx_vision);
    
    if (!mtmd_support_vision(ctx)) {
        VISION_DEBUG("Model does not support vision");
        VisionScanResult result;
        result.thumbnail_path = image_path;
        result.success = false;
        result.error_message = "Model does not support vision";
    }
    VISION_DEBUG("Vision support verified");
    
    // Build prompt using the media marker - let mtmd_tokenize handle special tokens
    // The tokenizer will replace <__media__> with <|im_start|>user\n...<image>...<|im_end|>
    std::string actual_prompt = prompt.empty() ? DEFAULT_PROMPT : prompt;
    std::string full_prompt = "<__media__>\n" + actual_prompt;
    VISION_DEBUG("Prompt: %s...", actual_prompt.substr(0, 50).c_str());
    
    struct mtmd_input_text text;
    text.text = full_prompt.c_str();
    text.add_special = true;  // Let tokenizer add BOS/EOS (depends on model config)
    text.parse_special = true;
    
    mtmd_input_chunks* chunks = mtmd_input_chunks_init();
    auto bmp = static_cast<mtmd_bitmap*>(m_current_bitmap);
    const mtmd_bitmap* bitmaps[1] = { bmp };
    VISION_DEBUG("Tokenizing prompt with image...");
    
    int32_t res = mtmd_tokenize(ctx, chunks, &text, bitmaps, 1);
    if (res != 0) {
        VISION_DEBUG("Tokenization failed with code: %d", res);
        LUCID_ERROR("[VisionScanner] mtmd_tokenize failed: %d", res);
        mtmd_input_chunks_free(chunks);
        VisionScanResult result;
        result.thumbnail_path = image_path;
        result.success = false;
        result.error_message = "Tokenization failed";
        return result;
    }
    VISION_DEBUG("Tokenization successful");
    
    auto llctx = static_cast<llama_context*>(m_context);
    llama_pos new_n_past;
    VISION_DEBUG("Evaluating chunks...");
    if (mtmd_helper_eval_chunks(ctx, llctx, chunks, m_n_past, 0,
                                 m_n_batch, true, &new_n_past) != 0) {
        VISION_DEBUG("Chunk evaluation failed");
        LUCID_ERROR("[VisionScanner] mtmd_helper_eval_chunks failed");
        mtmd_input_chunks_free(chunks);
        VisionScanResult result;
        result.thumbnail_path = image_path;
        result.success = false;
        result.error_message = "Processing failed";
        return result;
    }
    VISION_DEBUG("Chunk evaluation successful");
    
    m_n_past = new_n_past;
    mtmd_input_chunks_free(chunks);
    
    VISION_DEBUG("Generating response (max 256 tokens)...");
    std::string response = generate_response(256);
    VISION_DEBUG("Generated %d tokens", m_tokens_generated);
    
    auto end = std::chrono::steady_clock::now();
    int64_t processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    VISION_DEBUG("Total processing time: %ld ms", processing_time);
    
    VisionScanResult result = parse_and_format_response(image_path, 0, response);
    result.processing_time_ms = processing_time;
    result.success = true;
    VISION_DEBUG("Response parsed: scene_type=%s, objects=%zu", 
                 result.scene_type.c_str(), result.objects.size());
    VISION_DEBUG("=== Analysis complete ===");
    
    LUCID_DEBUG("[VisionScanner] Analyzed %s in %ldms, %d tokens",
                image_path.c_str(), processing_time, m_tokens_generated);
    
    return result;
}

// ============================================================================
// Batch Processing
// ============================================================================

std::vector<VisionScanResult> VisionScanner::analyze_batch(
    const std::vector<std::pair<std::string, int64_t>>& images,
    int batch_size,
    std::function<void(int, int)> progress_callback) {
    
    std::vector<VisionScanResult> results;
    
    if (!is_loaded()) {
        LUCID_WARN("[VisionScanner] Scanner not loaded");
        return results;
    }
    
    int total = static_cast<int>(images.size());
    int processed = 0;
    
    for (size_t i = 0; i < images.size(); i += batch_size) {
        size_t batch_end = std::min(i + static_cast<size_t>(batch_size), images.size());
        
        LUCID_DEBUG("[VisionScanner] Processing batch %d-%d of %d",
                    (int)i, (int)batch_end, total);
        
        for (size_t j = i; j < batch_end; ++j) {
            const auto& [image_path, timestamp_ms] = images[j];
            
            auto result_opt = analyze_image(image_path, "");
            if (result_opt) {
                VisionScanResult result = *result_opt;
                result.timestamp_ms = timestamp_ms;
                results.push_back(result);
            }
            
            processed++;
            if (progress_callback) {
                progress_callback(processed, total);
            }
        }
        
        m_n_past = 0;
    }
    
    LUCID_INFO("[VisionScanner] Batch complete: %zu/%d images analyzed",
               results.size(), total);
    
    return results;
}

// ============================================================================
// Response Generation
// ============================================================================

std::string VisionScanner::generate_response(int max_tokens) {
    std::string response;
    m_tokens_generated = 0;
    m_start_time = std::chrono::steady_clock::now();
    
    auto model = static_cast<llama_model*>(m_model);
    auto ctx = static_cast<llama_context*>(m_context);
    const llama_vocab* vocab = llama_model_get_vocab(model);
    
    while (m_tokens_generated < max_tokens) {
        float* logits = llama_get_logits_ith(ctx, -1);
        
        // Safety check: ensure logits are available
        if (!logits) {
            LUCID_ERROR("[VisionScanner] Failed to get logits");
            break;
        }
        
        int n_vocab = llama_vocab_n_tokens(vocab);
        llama_token new_token = 0;
        float max_logit = logits[0];
        
        for (int i = 1; i < n_vocab; ++i) {
            if (logits[i] > max_logit) {
                max_logit = logits[i];
                new_token = i;
            }
        }
        
        if (llama_vocab_is_eog(vocab, new_token)) {
            break;
        }
        
        char buf[256];
        int n = llama_token_to_piece(vocab, new_token, buf, sizeof(buf)-1, 0, true);
        if (n > 0) {
            buf[n] = '\0';
            response += buf;
        }
        
        struct llama_batch batch = llama_batch_init(m_n_batch, 0, 1);
        batch.n_tokens = 1;
        batch.token[0] = new_token;
        batch.pos[0] = m_n_past;  // Use tracked position
        batch.n_seq_id[0] = 1;
        batch.seq_id[0][0] = 0;
        batch.logits[0] = true;
        
        if (llama_decode(ctx, batch)) {
            LUCID_ERROR("[VisionScanner] llama_decode failed");
            llama_batch_free(batch);
            break;
        }
        
        llama_batch_free(batch);
        m_n_past++;  // Increment position after successful decode
        m_tokens_generated++;
    }
    
    return response;
}

// ============================================================================
// Response Parsing
// ============================================================================

VisionScanResult VisionScanner::parse_and_format_response(
    const std::string& image_path,
    int64_t timestamp_ms,
    const std::string& response) {
    
    VisionScanResult result;
    result.thumbnail_path = image_path;
    result.timestamp_ms = timestamp_ms;
    result.description = response;
    result.confidence = 0.8f;
    
    std::string lower_response = response;
    std::transform(lower_response.begin(), lower_response.end(), lower_response.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    
    if (lower_response.find("screen") != std::string::npos || 
        lower_response.find("monitor") != std::string::npos ||
        lower_response.find("desktop") != std::string::npos) {
        result.scene_type = "screen_capture";
    } else if (lower_response.find("outdoor") != std::string::npos ||
               lower_response.find("outside") != std::string::npos ||
               lower_response.find("sky") != std::string::npos) {
        result.scene_type = "outdoor";
    } else if (lower_response.find("indoor") != std::string::npos ||
               lower_response.find("inside") != std::string::npos ||
               lower_response.find("room") != std::string::npos) {
        result.scene_type = "indoor";
    } else if (lower_response.find("studio") != std::string::npos) {
        result.scene_type = "studio";
    } else if (lower_response.find("motion") != std::string::npos ||
               lower_response.find("animation") != std::string::npos) {
        result.scene_type = "motion_graphics";
    } else {
        result.scene_type = "unknown";
    }
    
    std::vector<std::string> text_indicators = {"text:", "overlay:", "caption:", "subtitle:"};
    for (const auto& indicator : text_indicators) {
        auto pos = lower_response.find(indicator);
        if (pos != std::string::npos) {
            size_t start = pos + indicator.length();
            size_t end = lower_response.find_first_of(".,;\n", start);
            if (end != std::string::npos && end > start) {
                std::string text = response.substr(start, end - start);
                while (!text.empty() && std::isspace(text[0])) text.erase(0, 1);
                while (!text.empty() && std::isspace(text.back())) text.pop_back();
                if (!text.empty()) {
                    result.text_content = text;
                    break;
                }
            }
        }
    }
    
    std::vector<std::pair<std::string, std::vector<std::string>>> object_patterns = {
        {"person", {"person", "people", "human", "man", "woman", "face", "person's"}},
        {"screen", {"screen", "monitor", "display", "laptop", "tv", "television"}},
        {"text", {"text", "writing", "letter", "word", "label"}},
        {"chart", {"chart", "graph", "diagram", "visualization"}},
        {"code", {"code", "programming", "IDE", "terminal", "command"}},
        {"logo", {"logo", "brand", "icon", "symbol"}}
    };
    
    for (const auto& [object_name, patterns] : object_patterns) {
        for (const auto& pattern : patterns) {
            if (lower_response.find(pattern) != std::string::npos) {
                size_t pos = lower_response.find(pattern);
                bool is_negated = false;
                // Check up to 5 characters before the match for negation indicators
                if (pos >= 5) {
                    std::string before = lower_response.substr(pos - 5, 5);
                    if (before.find(" no ") != std::string::npos || 
                        before.find("not ") != std::string::npos ||
                        before.find("n't ") != std::string::npos) {
                        is_negated = true;
                    }
                }
                if (!is_negated) {
                    result.objects.push_back(object_name);
                    break;
                }
            }
        }
    }
    
    return result;
}

// ============================================================================
// Shutdown
// ============================================================================

void VisionScanner::shutdown() {
    LUCID_DEBUG("[VisionScanner] Shutting down");
    
    if (m_current_bitmap) {
        mtmd_bitmap_free(static_cast<mtmd_bitmap*>(m_current_bitmap));
        m_current_bitmap = nullptr;
    }
    
    if (m_ctx_vision) {
        mtmd_free(static_cast<mtmd_context*>(m_ctx_vision));
        m_ctx_vision = nullptr;
    }
    
    if (m_context) {
        llama_free(static_cast<llama_context*>(m_context));
        m_context = nullptr;
    }
    
    if (m_model) {
        llama_model_free(static_cast<llama_model*>(m_model));
        m_model = nullptr;
    }
    
    m_n_past = 0;
    
    LUCID_DEBUG("[VisionScanner] Shutdown complete");
}

} // namespace lucid