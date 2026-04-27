# Llama.cpp Documentation

## Overview

Llama.cpp is a high-performance, pure C/C++ implementation for LLM (Large Language Model) inference. It enables running GGUF-format language models locally with minimal dependencies and state-of-the-art performance across a wide range of hardware.

**Repository**: [github.com/ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp)

### Key Features

- **Pure C/C++ implementation** - No Python or external ML framework dependencies
- **GGUF format support** - Universal model format with efficient loading
- **Cross-platform support** - Linux, macOS, Windows, WebAssembly, iOS, Android
- **Multiple backend options** - CPU, Apple Metal, CUDA, Vulkan, OpenCL, BLAS
- **Advanced quantization** - Q2_K to Q8_0 with various quality/speed tradeoffs
- **Embedding models** - Support for text embedding generation (e.g., nomic-embed-text)
- **JSON grammars** - Constrain output to valid JSON or custom formats
- **Prompt caching** - Reduce latency for repeated contexts
- **Batch inference** - Process multiple requests simultaneously

---

## Building llama.cpp

### Prerequisites

- **CMake** (recommended for modern build system)
- **C compiler** (GCC, Clang, MSVC)
- **Make** (optional, for Makefile-based builds)
- **Git LFS** (for downloading large model files)

### Clone the Repository

```bash
git clone https://github.com/ggml-org/llama.cpp.git
cd llama.cpp
git submodule update --init --recursive
```

### Build with CMake (Recommended)

```bash
# Create build directory
cmake -B build

# Build all examples
cmake --build build -j --config Release

# The binaries will be in: build/bin/
```

### Build with Make

```bash
# Build all examples
make -j

# Build specific targets
make llama-cli llama-server
```

### GPU Acceleration Options

#### Apple Silicon (Metal)

Metal support is enabled by default on macOS with Apple Silicon. No additional configuration needed.

```bash
cmake -B build
cmake --build build -j --config Release
```

#### NVIDIA CUDA

```bash
cmake -B build -DGGML_CUDA=ON
cmake --build build -j --config Release
```

#### Vulkan (Cross-platform GPU)

```bash
# Linux/macOS/Windows - Install Vulkan SDK first
cmake -B build -DGGML_VULKAN=ON
cmake --build build -j --config Release
```

#### Multiple Backends

You can build with multiple backend support:

```bash
cmake -B build -DGGML_CUDA=ON -DGGML_VULKAN=ON
cmake --build build -j --config Release
```

At runtime, llama.cpp will automatically select the best available backend.

#### CPU-only Optimizations

For systems without GPU, enable CPU optimizations:

```bash
cmake -B build \
    -DGGML_NATIVE=ON \
    -DGGML_OPENBLAS=ON \
    -DGGML_BLAS=ON
cmake --build build -j --config Release
```

### CMake Build Options

| Option | Description | Default |
|--------|-------------|---------|
| `GGML_NATIVE` | Enable native CPU optimizations | OFF |
| `GGML_CUDA` | NVIDIA CUDA support | OFF |
| `GGML_VULKAN` | Vulkan GPU support | OFF |
| `GGML_OPENCL` | OpenCL support | OFF |
| `GGML_BLAS` | BLAS acceleration | OFF |
| `GGML_OPENBLAS` | OpenBLAS support | OFF |
| `GGML_MPI` | MPI support for distributed inference | OFF |
| `BUILD_SHARED_LIBS` | Build as shared library | OFF |
| `LLAMA_BUILD_SERVER` | Build llama-server | ON |

---

## GGUF Models

### What is GGUF?

GGUF (General General Unified Format) is a binary format for storing quantized LLM weights. It provides:
- Efficient memory-mapped loading
- Built-in metadata (tokenizer, architecture, quantization info)
- Support for custom model architectures
- Fast loading without full model parsing

### Available Quantization Types

| Type | Bits | Quality | Speed | Memory | Use Case |
|------|------|---------|-------|--------|----------|
| Q2_K | 2 | Low | Fast | ~4GB/7B | Very constrained hardware |
| Q3_K_M | 3 | Fair | Fast | ~5GB/7B | Low memory systems |
| Q4_0 | 4 | Good | Very Fast | ~6GB/7B | Balanced option |
| Q4_K_M | 4 | Good+ | Very Fast | ~6GB/7B | Recommended |
| Q5_0 | 5 | Better | Moderate | ~7GB/7B | Higher quality |
| Q5_K_M | 5 | Better+ | Moderate | ~7GB/7B | Better quality |
| Q6_K | 6 | Near-fp16 | Slower | ~8GB/7B | Quality critical |
| Q8_0 | 8 | Very Good | Slower | ~10GB/7B | Quality focus |
| F16 | 16 | High | Baseline | ~14GB/7B | Reference |
| F32 | 32 | Full | Baseline | ~28GB/7B | Full precision |

### Model Size Reference (7B Parameter Model)

| Quantization | File Size | RAM/VRAM |
|--------------|-----------|----------|
| Q2_K | ~2.9 GB | ~4 GB |
| Q3_K_M | ~3.6 GB | ~5 GB |
| Q4_0 | ~4.1 GB | ~6 GB |
| Q4_K_M | ~4.4 GB | ~6 GB |
| Q5_0 | ~4.9 GB | ~7 GB |
| Q5_K_M | ~5.1 GB | ~7 GB |
| Q6_K | ~5.8 GB | ~8 GB |
| Q8_0 | ~7.0 GB | ~10 GB |
| F16 | ~13.5 GB | ~14 GB |

### Model Size Reference (70B Parameter Model)

| Quantization | File Size | RAM/VRAM |
|--------------|-----------|----------|
| Q2_K | ~27 GB | ~35 GB |
| Q4_K_M | ~40 GB | ~50 GB |
| Q5_K_M | ~46 GB | ~55 GB |
| Q6_K | ~53 GB | ~60 GB |
| Q8_0 | ~70 GB | ~75 GB |
| F16 | ~140 GB | ~145 GB |

### Popular GGUF Models

#### Instruction-Tuned Models

| Model | Size | Description |
|-------|------|-------------|
| llama-3.2-1b | 1.3 GB | Lightweight, fast |
| llama-3.2-3b | 2.0 GB | Balanced performance |
| llama-3.1-8b | 4.9 GB | Popular 8B model |
| qwen2.5-7b | 4.4 GB | Strong coding abilities |
| phi-3.5-mini | 2.4 GB | Microsoft model |
| hermes-3-8b | 4.9 GB | Excellent instruction following |

#### Embedding Models

| Model | Size | Dimensions | Description |
|-------|------|------------|-------------|
| nomic-embed-text-v1.5 | 274 MB | 768 | General purpose embeddings |
| nomic-embed-text-v2 | ~500 MB | 64-768 | Variable dimensions, matryoshka |
| embed-json-3b | 1.9 GB | 1024 | JSON-focused embeddings |

### Downloading Models

#### Method 1: Hugging Face (Recommended)

```bash
# Using huggingface-cli
huggingface-cli download \
  --local-dir ./models \
  --local-dir-use-symlinks False \
  TheBloke/Llama-3.2-1B-Instruct-GGUF \
  llama-3.2-1b-instruct-q4_k_m.gguf

# Direct download with wget
wget https://huggingface.co/TheBloke/Llama-3.2-1B-Instruct-GGUF/resolve/main/llama-3.2-1b-instruct-q4_k_m.gguf

# Clone entire repo with LFS
git lfs install
git clone https://huggingface.co/m横gpt/llama-3.2-1b-instruct-GGUF
```

#### Method 2: Model Download Script (llama.cpp)

```bash
# Download via CLI
./build/bin/llama-cli -m "meta-llama/Llama-3.2-1B-Instruct-GGUF" \
                      --hf-download \
                      -p "Hello, how are you?"
```

#### Method 3: Runtime Download in Code

```cpp
#include <curl/curl.h>
#include <fstream>

bool download_model(const std::string& repo,
                    const std::string& filename,
                    const std::string& output_path) {
    std::string url = "https://huggingface.co/" + repo + "/resolve/main/" + filename;
    
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    
    std::ofstream file(output_path, std::ios::binary);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
    
    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    
    return res == CURLE_OK && http_code == 200;
}
```

---

## C++ API Documentation

### Header File

```cpp
#include "llama.h"
```

### Core Data Structures

#### llama_model

```cpp
struct llama_model;
```

Represents the loaded model. Contains all model weights and architecture information.

#### llama_context

```cpp
struct llama_context;
```

Represents an inference context. Multiple contexts can be created from one model.

#### llama_token

```cpp
typedef int32_t llama_token;
```

Token IDs used for tokenization. Positive values are normal tokens, negative values are special tokens.

### Model Initialization

#### From File

```cpp
struct llama_model* llama_load_model_from_file(
    const char * path_model,
    struct llama_model_params params
);
```

Load a GGUF model from disk.

**Parameters:**
- `path_model` - Path to GGUF model file
- `params` - Model loading parameters (see below)

**Returns:** Pointer to `llama_model` on success, `nullptr` on failure

```cpp
// Example
struct llama_model_params params = llama_model_default_params();
params.n_gpu_layers = 99;  // Offload all layers to GPU

struct llama_model* model = llama_load_model_from_file(
    "./models/llama-3.2-1b-instruct-q4_k_m.gguf",
    params
);

if (!model) {
    fprintf(stderr, "Failed to load model\n");
    return 1;
}
```

#### Model Parameters

```cpp
struct llama_model_params {
    int n_gpu_layers;        // Layers to offload to GPU (0 = CPU only)
    bool use_mmap;           // Use memory-mapping (default: true)
    bool use_mlock;          // Lock model in RAM (default: false)
    bool vae_decode_only;    // Only decode VAE (for multimodal)
    bool auto_peak;          // Auto-tune memory based on context size
    
    // Callbacks
    void (*progress_callback)(float progress, void * user_data);
    void * progress_callback_user_data;
};
```

### Context Initialization

```cpp
struct llama_context* llama_new_context_with_model(
    struct llama_model * model,
    struct llama_context_params params
);
```

Create a new inference context from a model.

**Parameters:**
- `model` - Previously loaded model
- `params` - Context parameters

**Returns:** Pointer to `llama_context`

```cpp
// Example
struct llama_context_params params = llama_context_default_params();
params.n_ctx = 4096;           // Context window size
params.n_batch = 512;          // Batch size for prompt processing
params.n_ubatch = 512;         // Physical batch size
params.n_threads = 4;          // CPU threads
params.n_threads_batch = 4;    // CPU threads for batch processing

struct llama_context* ctx = llama_new_context_with_model(model, params);
```

#### Context Parameters

```cpp
struct llama_context_params {
    uint32_t n_ctx;           // Context window size (default: 512)
    uint32_t n_batch;         // Batch size for prompt processing
    uint32_t n_ubatch;        // Physical batch size
    uint32_t n_threads;       // CPU threads for inference
    uint32_t n_threads_batch; // CPU threads for batch processing
    enum llama_rope_scaling_type rope_scaling_type;
    enum llama_rope_freq_base rope_freq_base;
    float rope_scale;         // RoPE scaling factor
    
    // Seed for random number generator
    uint64_t seed;            // 0 = random seed
};
```

### Cleanup

```cpp
void llama_free_model(struct llama_model * model);
void llama_free(struct llama_context * ctx);
```

Free resources. Context must be freed before model.

```cpp
// Correct order
llama_free(ctx);
llama_free_model(model);
```

---

## Tokenization

### Tokenize Text

```cpp
int llama_tokenize(
    struct llama_model * model,
    const char * text,
    llama_token * tokens,
    int n_max_tokens,
    bool add_bos,
    bool add_eos
);
```

Convert text to token IDs.

**Parameters:**
- `model` - Model (contains tokenizer)
- `text` - Input text
- `tokens` - Output buffer for token IDs
- `n_max_tokens` - Maximum tokens to output
- `add_bos` - Add beginning-of-sequence token
- `add_eos` - Add end-of-sequence token

**Returns:** Number of tokens generated (negative on error)

```cpp
// Example
std::vector<llama_token> tokens(256);
int n_tokens = llama_tokenize(
    model,
    "Hello, how are you today?",
    tokens.data(),
    tokens.size(),
    true,   // add BOS
    false   // don't add EOS
);

if (n_tokens < 0) {
    fprintf(stderr, "Tokenization failed\n");
}
```

### Helper Functions

```cpp
// Get special token IDs
llama_token llama_token_bos(struct llama_model * model);
llama_token llama_token_eos(struct llama_model * model);
llama_token llama_token_cls(struct llama_model * model);
llama_token llama_token_sep(struct llama_model * model);
llama_token llama_token_nl(struct llama_model * model);

// Get token as string
const char* llama_token_to_piece(
    struct llama_model * model,
    llama_token token
);

// Convert token to string (for decoding)
std::string llama_token_to_str(struct llama_context * ctx, llama_token token);
```

---

## Inference

### Main Inference Loop

```cpp
int llama_decode(
    struct llama_context * ctx,
    struct llama_batch batch
);
```

Process a batch of tokens.

```cpp
// Example inference loop
std::vector<llama_token> tokens = ...;  // Tokenize prompt first

// Prefill phase (process prompt)
struct llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());
if (llama_decode(ctx, batch) != 0) {
    fprintf(stderr, "Decode failed\n");
}

// Generation phase
llama_token new_token = tokens.back();
while (new_token != llama_token_eos(model) && output_length < max_length) {
    // Add new token to batch
    batch = llama_batch_get_one(&new_token, 1);
    
    if (llama_decode(ctx, batch) != 0) {
        fprintf(stderr, "Decode failed\n");
        break;
    }
    
    // Get logits for next token
    float* logits = llama_get_logits_ith(ctx, -1);
    
    // Sample next token (use top-k, top-p, temperature, etc.)
    new_token = sample_token(logits, temperature);
    
    // Print token
    printf("%s", llama_token_to_piece(model, new_token).c_str());
    
    // Add to context
    llama_token_data_array candidates = ...;
    llama_token_probability(googles);
    new_token = llama_sample_token_...(ctx, &candidates);
}
```

### Batch Structure

```cpp
struct llama_batch {
    int32_t n_tokens;        // Number of tokens in batch
    int32_t n_max_tokens;    // Max tokens allocated
    llama_token* token;       // Token IDs [n_tokens]
    float*  embd;            // Embeddings (for non-token input)
    
    // Position information
    int32_t* pos;            // Positions [n_tokens]
    
    // Attention
    int32_t  n_seq_id;       // Number of sequences
    int32_t* seq_id;         // Sequence IDs
    int32_t* pos_seq_id;     // Position in sequence
    
    // Logits (output)
    float* logits;           // Logits for last token
};
```

### Sampling Helpers

Llama.cpp provides built-in sampling functions:

```cpp
// Greedy sampling
llama_token llama_sample_token_greedy(
    struct llama_context * ctx,
    llama_token_data_array * candidates
);

// Temperature sampling
void llama_sample_temperature(
    struct llama_context * ctx,
    llama_token_data_array * candidates,
    float temp
);
llama_token llama_sample_token(
    struct llama_context * ctx,
    llama_token_data_array * candidates
);

// Top-k sampling
void llama_sample_top_k(
    struct llama_context * ctx,
    llama_token_data_array * candidates,
    int k
);

// Top-p (nucleus) sampling
void llama_sample_top_p(
    struct llama_context * ctx,
    llama_token_data_array * candidates,
    float p
);

// Min P sampling
void llama_sample_min_p(
    struct llama_context * ctx,
    llama_token_data_array * candidates,
    float p
);

// Repeat penalty
void llama_sample_repetition_penalties(
    struct llama_context * ctx,
    llama_token_data_array * candidates,
    const llama_token* last_tokens,
    size_t last_tokens_size,
    float penalty,
    float alpha_frequency,
    float alpha_presence
);

// Combined pipeline
llama_token llama_sample_token_simple(
    struct llama_context * ctx,
    const struct llama_sampling_params* params
);
```

---

## Complete C++ Code Examples

### Example 1: Basic Text Completion

```cpp
#include "llama.h"
#include <iostream>
#include <vector>
#include <string>
#include <random>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <model.gguf>\n";
        return 1;
    }

    const char* model_path = argv[1];

    // Load model
    struct llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = 99;  // Use GPU if available

    struct llama_model* model = llama_load_model_from_file(model_path, model_params);
    if (!model) {
        std::cerr << "Failed to load model: " << model_path << "\n";
        return 1;
    }

    // Create context
    struct llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 2048;
    ctx_params.n_threads = 4;
    ctx_params.n_threads_batch = 4;

    struct llama_context* ctx = llama_new_context_with_model(model, ctx_params);
    if (!ctx) {
        std::cerr << "Failed to create context\n";
        llama_free_model(model);
        return 1;
    }

    // Prompt
    const char* prompt = "The capital of France is";
    std::vector<llama_token> tokens(256);
    
    int n_tokens = llama_tokenize(model, prompt, tokens.data(), 
                                   tokens.size(), true, false);
    if (n_tokens < 0) {
        std::cerr << "Tokenization failed\n";
        llama_free(ctx);
        llama_free_model(model);
        return 1;
    }
    tokens.resize(n_tokens);

    // Evaluate prompt
    struct llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());
    if (llama_decode(ctx, batch) != 0) {
        std::cerr << "Decode failed\n";
        llama_free(ctx);
        llama_free_model(model);
        return 1;
    }

    // Generate tokens
    std::cout << prompt;
    llama_token token = tokens.back();
    int max_new_tokens = 50;

    for (int i = 0; i < max_new_tokens; ++i) {
        // Get logits
        float* logits = llama_get_logits_ith(ctx, -1);
        
        // Create candidates
        std::vector<llama_token_data> candidates_data;
        candidates_data.reserve(llama_n_vocab(model));
        for (int i = 0; i < llama_n_vocab(model); ++i) {
            candidates_data.push_back({i, logits[i], 0.0f});
        }
        llama_token_data_array candidates = {
            candidates_data.data(),
            candidates_data.size(),
            false
        };

        // Sample with temperature
        llama_sample_temperature(ctx, &candidates, 0.7f);
        llama_sample_top_p(ctx, &candidates, 0.9f);
        
        token = llama_sample_token(ctx, &candidates);

        // Check for end
        if (token == llama_token_eos(model)) {
            break;
        }

        // Print token
        std::string piece = llama_token_to_piece(model, token);
        std::cout << piece;

        // Prepare for next iteration
        batch = llama_batch_get_one(&token, 1);
        if (llama_decode(ctx, batch) != 0) {
            break;
        }
    }
    
    std::cout << "\n";

    // Cleanup
    llama_free(ctx);
    llama_free_model(model);
    
    return 0;
}
```

### Example 2: Chat Completion with System Prompt

```cpp
#include "llama.h"
#include <iostream>
#include <vector>
#include <sstream>

struct Message {
    std::string role;    // "system", "user", "assistant"
    std::string content;
};

// Simple chat template (adjust based on model)
std::string format_chat(const std::vector<Message>& messages, 
                        const std::string& model_name) {
    std::ostringstream oss;
    
    // Llama 3.1 format
    oss << "<|begin_of_text|>";
    
    for (const auto& msg : messages) {
        if (msg.role == "system") {
            oss << "<|start_header_id|>system<|end_header_id|>\n\n";
            oss << msg.content << "<|eot_id|>";
        } else if (msg.role == "user") {
            oss << "<|start_header_id|>user<|end_header_id|>\n\n";
            oss << msg.content << "<|eot_id|>";
        } else if (msg.role == "assistant") {
            oss << "<|start_header_id|>assistant<|end_header_id|>\n\n";
            oss << msg.content << "<|eot_id|>";
        }
    }
    
    // Add assistant prefix
    oss << "<|start_header_id|>assistant<|end_header_id|>\n\n";
    
    return oss.str();
}

std::string chat_completion(struct llama_model* model,
                            struct llama_context* ctx,
                            const std::vector<Message>& messages,
                            int max_tokens = 256,
                            float temperature = 0.7f) {
    
    // Format prompt
    std::string prompt = format_chat(messages, "llama");
    
    // Tokenize
    std::vector<llama_token> tokens(4096);
    int n_tokens = llama_tokenize(model, prompt.c_str(), tokens.data(),
                                    tokens.size(), true, false);
    if (n_tokens < 0) return "";
    tokens.resize(n_tokens);

    // Evaluate prompt
    struct llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());
    llama_decode(ctx, batch);

    // Generate response
    std::string response;
    llama_token token;
    int generated = 0;

    while (generated < max_tokens) {
        float* logits = llama_get_logits_ith(ctx, -1);
        
        // Sample
        std::vector<llama_token_data> candidates;
        candidates.reserve(llama_n_vocab(model));
        for (int i = 0; i < llama_n_vocab(model); ++i) {
            candidates.push_back({i, logits[i], 0.0f});
        }
        llama_token_data_array cand = {candidates.data(), 
                                        candidates.size(), false};
        
        llama_sample_temperature(ctx, &cand, temperature);
        token = llama_sample_token(ctx, &cand);

        // Check for end
        if (token == llama_token_eos(model)) {
            break;
        }

        // Add token to response
        std::string piece = llama_token_to_piece(model, token);
        response += piece;

        // Decode next
        batch = llama_batch_get_one(&token, 1);
        if (llama_decode(ctx, batch) != 0) break;
        
        generated++;
    }

    return response;
}

int main() {
    // Load model
    struct llama_model* model = llama_load_model_from_file(
        "./models/llama-3.2-1b-instruct-q4_k_m.gguf",
        llama_model_default_params()
    );

    struct llama_context* ctx = llama_new_context_with_model(
        model,
        llama_context_default_params()
    );

    // Chat messages
    std::vector<Message> messages = {
        {"system", "You are a helpful AI assistant."},
        {"user", "What is the meaning of life?"}
    };

    std::string response = chat_completion(model, ctx, messages);
    std::cout << "Assistant: " << response << "\n";

    llama_free(ctx);
    llama_free_model(model);
    
    return 0;
}
```

### Example 3: Text Embeddings

```cpp
#include "llama.h"
#include <iostream>
#include <vector>
#include <cmath>

struct EmbeddingResult {
    std::vector<float> embedding;
    int n_tokens;
};

// Get embeddings for text using an embedding model
EmbeddingResult get_embeddings(struct llama_model* model,
                                struct llama_context* ctx,
                                const std::string& text,
                                const std::string& instruction = "search_document") {
    
    // Format with instruction prefix (nomic-embed-text format)
    std::string formatted = instruction + text;
    
    // Tokenize
    std::vector<llama_token> tokens(2048);
    int n_tokens = llama_tokenize(model, formatted.c_str(), tokens.data(),
                                   tokens.size(), true, false);
    if (n_tokens < 0) return {{}, 0};
    tokens.resize(n_tokens);

    // Evaluate
    struct llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());
    llama_decode(ctx, batch);

    // Get embeddings from last token
    float* logits = llama_get_logits_ith(ctx, -1);
    
    // For embedding models, logits represent the embedding
    // Size depends on model (e.g., 768 for nomic-embed-text)
    std::vector<float> embedding(logits, logits + llama_n_vocab(model));

    return {embedding, n_tokens};
}

// Compute cosine similarity between two embeddings
float cosine_similarity(const std::vector<float>& a, 
                        const std::vector<float>& b) {
    float dot = 0.0f, mag_a = 0.0f, mag_b = 0.0f;
    
    for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
        dot += a[i] * b[i];
        mag_a += a[i] * a[i];
        mag_b += b[i] * b[i];
    }
    
    return dot / (std::sqrt(mag_a) * std::sqrt(mag_b) + 1e-9f);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <embedding-model.gguf>\n";
        return 1;
    }

    // Load embedding model
    struct llama_model* model = llama_load_model_from_file(
        argv[1],
        llama_model_default_params()
    );

    struct llama_context* ctx = llama_new_context_with_model(
        model,
        llama_context_default_params()
    );

    // Get embeddings
    auto result1 = get_embeddings(model, ctx, "The cat sat on the mat", "search_document: ");
    auto result2 = get_embeddings(model, ctx, "A feline is resting on a rug", "search_document: ");
    auto result3 = get_embeddings(model, ctx, "I love playing football", "search_document: ");

    // Compute similarities
    float sim_12 = cosine_similarity(result1.embedding, result2.embedding);
    float sim_13 = cosine_similarity(result1.embedding, result3.embedding);

    std::cout << "Similarity (cat sentences): " << sim_12 << "\n";
    std::cout << "Similarity (cat vs football): " << sim_13 << "\n";

    llama_free(ctx);
    llama_free_model(model);
    
    return 0;
}
```

### Example 4: JSON Output with Grammar

```cpp
#include "llama.h"
#include <iostream>
#include <vector>

// JSON grammar for structured output
// This constrains the model to output valid JSON
const char* JSON_GRAMMAR = R"(
    root ::= object
    object ::= "{" ws (pair (", " pair)*)? ws "}"
    pair ::= string ": " value
    array ::= "[" ws (value (", " value)*)? ws "]"
    value ::= string | number | "true" | "false" | "null" | object | array
    ws ::= " "*
    string ::= '"' ([^"]*) '"'
    number ::= [0-9]+ ("." [0-9]+)?
)";

// Parse grammar into llama_grammar
struct llama_grammar* init_json_grammar() {
    // For actual implementation, use llama_grammar_init_from_string
    // This is simplified for demonstration
    return nullptr;  // See llama.cpp examples for full implementation
}

std::string generate_json_response(struct llama_model* model,
                                   struct llama_context* ctx,
                                   const std::string& prompt) {
    
    // Tokenize prompt
    std::vector<llama_token> tokens(2048);
    int n_tokens = llama_tokenize(model, prompt.c_str(), tokens.data(),
                                   tokens.size(), true, false);
    if (n_tokens < 0) return "";
    tokens.resize(n_tokens);

    // Evaluate prompt
    struct llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());
    llama_decode(ctx, batch);

    // Generate with grammar constraints
    // Note: Full grammar implementation requires llama_grammar APIs
    // See examples in llama.cpp for complete grammar support
    
    std::string result;
    llama_token token;
    int max_tokens = 500;

    for (int i = 0; i < max_tokens; ++i) {
        float* logits = llama_get_logits_ith(ctx, -1);
        
        // Sample with temperature
        std::vector<llama_token_data> candidates;
        candidates.reserve(llama_n_vocab(model));
        for (int j = 0; j < llama_n_vocab(model); ++j) {
            candidates.push_back({j, logits[j], 0.0f});
        }
        llama_token_data_array cand = {candidates.data(), 
                                        candidates.size(), false};
        
        llama_sample_temperature(ctx, &cand, 0.3f);  // Low temp for JSON
        token = llama_sample_token(ctx, &cand);

        // Check for end
        if (token == llama_token_eos(model)) {
            break;
        }

        result += llama_token_to_piece(model, token);

        batch = llama_batch_get_one(&token, 1);
        if (llama_decode(ctx, batch) != 0) break;
    }

    return result;
}

int main() {
    // Example prompt for JSON output
    const char* prompt = R"(
[INST]Extract information into JSON:
Extract: "John is 25 years old and works as a software engineer"
[/INST]Output JSON: )";

    struct llama_model* model = llama_load_model_from_file(
        "./models/llama-3.2-1b-instruct-q4_k_m.gguf",
        llama_model_default_params()
    );

    struct llama_context* ctx = llama_new_context_with_model(
        model,
        llama_context_default_params()
    );

    std::string json_response = generate_json_response(model, ctx, prompt);
    std::cout << "Generated JSON:\n" << json_response << "\n";

    llama_free(ctx);
    llama_free_model(model);
    
    return 0;
}
```

---

## Command-Line Tools

### llama-cli (Interactive CLI)

```bash
# Basic completion
./build/bin/llama-cli -m ./models/llama-3.2-1b.q4_k_m.gguf -p "Hello"

# Interactive mode
./build/bin/llama-cli -m ./models/llama-3.2-1b.q4_k_m.gguf -i

# With system prompt
./build/bin/llama-cli -m ./models/llama-3.2-1b.q4_k_m.gguf \
    -p "You are a helpful assistant" \
    -r "User: " \
    --in-prefix " "

# Interactive chat
./build/bin/llama-cli -m ./models/llama-3.2-1b.q4_k_m.gguf -cml
```

### llama-server (HTTP API Server)

```bash
# Start server
./build/bin/llama-server -m ./models/llama-3.2-1b.q4_k_m.gguf -c 2048

# With authentication
./build/bin/llama-server -m ./models/llama-3.2-1b.q4_k_m.gguf \
    --http-port 8080 \
    --http-auth

# Multi-backend
./build/bin/llama-server -m ./models/llama-3.2-1b.q4_k_m.gguf \
    --gpu \
    --nvidiagrid
```

#### Server Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/v1/completions` | POST | Text completion |
| `/v1/chat/completions` | POST | Chat completion |
| `/v1/embeddings` | POST | Generate embeddings |
| `/health` | GET | Health check |
| `/slot/infill` | POST | Infill task |

### Common CLI Options

| Option | Description |
|--------|-------------|
| `-m <path>` | Path to model file |
| `-p <text>` | Prompt text |
| `-i` | Interactive mode |
| `-c <n>` | Context size |
| `-t <n>` | Number of threads |
| `-ngl <n>` | GPU layers to offload |
| `--temp <f>` | Temperature |
| `-n <n>` | Max tokens to generate |
| `-top_k <n>` | Top-k sampling |
| `-top_p <f>` | Top-p sampling |
| `--cache-prompt` | Enable prompt caching |

---

## Performance Optimization

### Memory Configuration

```cpp
struct llama_model_params params = llama_model_default_params();
params.use_mmap = true;    // Memory-map model (default)
params.use_mlock = false;  // Don't lock in RAM
params.n_gpu_layers = 99;  // Offload to GPU
```

### Thread Configuration

```cpp
struct llama_context_params params = llama_context_default_params();
params.n_threads = std::thread::hardware_concurrency();      // Inference
params.n_threads_batch = std::thread::hardware_concurrency(); // Batch processing
```

### GPU Offloading

```cpp
// Offload all layers to GPU
struct llama_model_params params = llama_model_default_params();
params.n_gpu_layers = 99;

// Offload only attention layers
params.n_gpu_layers = 32;  // Adjust based on model size

// CPU-only (for large models that don't fit in VRAM)
params.n_gpu_layers = 0;
```

### KV Cache Configuration

```cpp
// Enable KV cache
ctx_params.n_ctx = 4096;

// Use persistent KV cache (saves memory for repeated prompts)
ctx_params.no_kv_offload = false;
```

---

## Platform-Specific Notes

### Linux

```bash
# Install Vulkan SDK (for GPU acceleration)
sudo apt-get install vulkan-tools libvulkan-dev glslang

# Build with Vulkan
cmake -B build -DGGML_VULKAN=ON
cmake --build build -j

# Build with CUDA
cmake -B build -DGGML_CUDA=ON
cmake --build build -j
```

### macOS

Apple Silicon has Metal GPU acceleration enabled by default:

```bash
# Build (Metal auto-detected)
cmake -B build
cmake --build build -j
```

### Windows

Using CMake with MSVC or MinGW:

```powershell
# Using CMake with MSVC
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release

# With CUDA
cmake -B build -G "Visual Studio 17 2022" -A x64 -DGGML_CUDA=ON
cmake --build build --config Release
```

---

## API Documentation Links

- **Main Repository**: [github.com/ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp)
- **Header File**: [include/llama.h](https://github.com/ggml-org/llama.cpp/blob/master/include/llama.h)
- **C API Reference**: [llama.cpp Mintlify](https://mintlify.com/ggml-org/llama.cpp/api/model-loading)
- **GGUF Format**: [Hugging Face GGUF Guide](https://huggingface.co/docs/hub/gguf)
- **Embedding Models**: [nomic-embed-text-v1.5-GGUF](https://huggingface.co/nomic-ai/nomic-embed-text-v1.5-GGUF)
- **Model Quantization**: [llama.cpp Quantization Guide](https://llama.cpp/docs/quantization)

---

## Best Practices

### Model Selection

| Use Case | Recommended Model | Quantization |
|----------|------------------|--------------|
| Fast chat/QA | 1B-3B models | Q4_K_M |
| Code generation | 7B-8B models | Q4_K_M |
| Long context | 7B-8B with RoPE | Q6_K |
| Embeddings | nomic-embed-text | Q4_K_M |
| JSON output | Any instruct model | Q5_K_M |

### Memory Estimation

For a 7B parameter model:
- **Q4_K_M**: ~4.4 GB (good balance)
- **Q5_K_M**: ~5.1 GB (better quality)
- **Q6_K**: ~5.8 GB (near full quality)

For 70B parameter model:
- **Q4_K_M**: ~40 GB (minimum for consumer GPU)
- **Q6_K**: ~53 GB (recommended)

### Prompt Engineering

1. **Use appropriate templates** - Different models use different chat formats
2. **System prompt first** - Set context before user message
3. **Clear instructions** - Be explicit about output format
4. **Temperature tuning** - Lower (0.1-0.3) for factual, higher (0.7-1.0) for creative

---

## License

Llama.cpp is released under the MIT License.

The underlying models (LLaMA, etc.) have their own licenses - typically research use only for the base model, but permissive licenses for quantized GGUF files from community quantizers.

---

*Document generated for Lucid project*