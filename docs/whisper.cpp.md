# Whisper.cpp Documentation

## Overview

Whisper.cpp is a high-performance, portable C/C++ implementation of OpenAI's Whisper automatic speech recognition (ASR) model. It provides a lightweight, dependency-free solution for running Whisper inference locally on various platforms.

**Repository**: [github.com/ggml-org/whisper.cpp](https://github.com/ggml-org/whisper.cpp)

### Key Features

- **Plain C/C++ implementation** - No Python or external ML framework dependencies
- **Zero memory allocations at runtime** - Predictable memory usage
- **Cross-platform support** - Linux, macOS, Windows, WebAssembly, iOS, Android
- **Multiple backend options** - CPU, Apple Metal, CUDA, Vulkan
- **Quantized models** - Support for INT8, INT16, FLOAT16, FLOAT32 precision
- **Multilingual support** - 100+ languages supported
- **Real-time transcription** - Streaming audio from microphone
- **Translation capability** - Translate speech to English

---

## Building whisper.cpp

### Prerequisites

- **CMake** (recommended for modern build system)
- **C compiler** (GCC, Clang, MSVC)
- **Make** (optional, for Makefile-based builds)

### Clone the Repository

```bash
git clone https://github.com/ggml-org/whisper.cpp.git
cd whisper.cpp
```

### Build with CMake (Recommended)

```bash
# Create build directory
cmake -B build

# Build the project
cmake --build build -j --config Release

# The binaries will be in: build/bin/
```

### Build with Make

```bash
# Build all examples
make -j

# Or build only the main example
make main

# Quick start - downloads base.en model and runs demo
make base.en
```

### GPU Acceleration Options

#### Apple Silicon (Metal)

Metal support is enabled by default on macOS with Apple Silicon. No additional configuration needed.

#### NVIDIA CUDA

```bash
cmake -B build -DGGML_CUDA=ON
cmake --build build -j --config Release
```

#### Vulkan (Cross-platform GPU)

```bash
cmake -B build -DGGML_VULKAN=ON
cmake --build build -j --config Release
```

---

## Downloading Models

### Available Models

Whisper.cpp uses models converted to GGML format. Models are available in various sizes and quantization levels:

| Model | Parameters | English-Only | Quantized (Q5) | Full Precision |
|-------|------------|--------------|----------------|----------------|
| tiny | 39M | `ggml-tiny.en.bin` | `ggml-tiny-q5_1.bin` | `ggml-tiny.bin` |
| base | 74M | `ggml-base.en.bin` | `ggml-base-q5_1.bin` | `ggml-base.bin` |
| small | 244M | `ggml-small.en.bin` | `ggml-small-q5_1.bin` | `ggml-small.bin` |
| medium | 769M | `ggml-medium.en.bin` | `ggml-medium-q5_1.bin` | `ggml-medium.bin` |
| large | 1550M | N/A | `ggml-large-q5_1.bin` | `ggml-large.bin` |
| large-v3 | 1550M | N/A | `ggml-large-v3-q5_1.bin` | `ggml-large-v3.bin` |
| turbo | ~809M | N/A | `ggml-turbo-q5_1.bin` | `ggml-turbo.bin` |

### Model Size Comparison

| Model | Relative Speed | Memory Usage | Accuracy |
|-------|---------------|--------------|----------|
| tiny | 32x | ~1 GB | Basic |
| base | 16x | ~1 GB | Good |
| small | 7x | ~2 GB | Better |
| medium | 2x | ~5 GB | High |
| large | 1x | ~10 GB | Highest |

### Method 1: Download Script (Recommended)

```bash
# Download a specific model
./models/download-ggml-model.sh base.en

# Available models: tiny, tiny.en, base, base.en, small, small.en,
#                   medium, medium.en, large, large-v3, turbo
```

### Method 2: Hugging Face

Download directly from Hugging Face:

```
https://huggingface.co/ggerganov/whisper.cpp/tree/main
```

Direct links for common models:

```bash
# Using wget or curl
wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin

# Or clone the full repository
git lfs install
git clone https://huggingface.co/ggerganov/whisper.cpp
```

### Method 3: Runtime Download in Code

You can download models programmatically at runtime:

```cpp
#include <curl/curl.h>
#include <fstream>

bool download_model(const std::string& url, const std::string& path) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    
    std::ofstream file(path, std::ios::binary);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    return res == CURLE_OK;
}

// Usage
download_model(
    "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin",
    "./models/ggml-base.en.bin"
);
```

---

## C++ API Documentation

### Header File

```cpp
#include "whisper.h"
```

### Core API Functions

#### Initialize Context from File

```cpp
struct whisper_context* whisper_init_from_file(const char* path_model);
```

Creates a whisper context by loading a GGML model file. The context is required for all transcription operations.

**Parameters:**
- `path_model` - Path to the GGML model file (e.g., `ggml-base.en.bin`)

**Returns:** Pointer to `whisper_context` on success, `nullptr` on failure

**Example:**
```cpp
struct whisper_context* ctx = whisper_init_from_file("./models/ggml-base.en.bin");
if (!ctx) {
    fprintf(stderr, "Failed to load model\n");
    return 1;
}
```

#### Initialize Context from Bytes

```cpp
struct whisper_context* whisper_init_from_buffer(
    const char* buffer,
    size_t buffer_size
);
```

Load a model from a memory buffer instead of a file.

#### Free Context

```cpp
void whisper_free(struct whisper_context* ctx);
```

Releases all resources associated with the whisper context.

```cpp
whisper_free(ctx);
```

#### Full Transcription

```cpp
int whisper_full(
    struct whisper_context* ctx,
    struct whisper_full_params params,
    const float* samples,
    const int n_samples
);
```

Transcribe audio samples to text.

**Parameters:**
- `ctx` - Whisper context
- `params` - Transcription parameters (see below)
- `samples` - Audio samples as float32 array
- `n_samples` - Number of audio samples

**Returns:** 0 on success, non-zero on failure

#### Get Number of Segments

```cpp
int whisper_full_n_segments(struct whisper_context* ctx);
```

Returns the number of text segments in the transcription result.

#### Get Segment Text

```cpp
const char* whisper_full_get_segment_text(
    struct whisper_context* ctx,
    int i_segment
);
```

Get the text content of a specific segment.

**Parameters:**
- `i_segment` - Segment index (0 to n_segments-1)

**Returns:** C-string with segment text

#### Get Segment Timing

```cpp
const char* whisper_full_get_segment_text(
    struct whisper_context* ctx,
    int i_segment
);

struct whisper_segment {
    int64_t t0;  // Start time in milliseconds
    int64_t t1;  // End time in milliseconds
};
```

Get the timing information for a segment.

```cpp
// Get timing
int64_t t0 = whisper_full_get_segment_t0(ctx, i_segment);
int64_t t1 = whisper_full_get_segment_t1(ctx, i_segment);

// Convert to seconds
double start_sec = t0 / 1000.0;
double end_sec = t1 / 1000.0;
```

#### Language Detection

```cpp
int whisper_full_detect_language(
    struct whisper_context* ctx,
    struct whisper_full_params params,
    const float* samples,
    const int n_samples,
    int* lang_probs  // Output array of language probabilities
);
```

Detect the language of the audio. Returns the detected language index.

---

### Parameter Structure

```cpp
struct whisper_full_params {
    // Decoding strategy: Greedy or Beam Search
    enum whisper_sampling_strategy {
        WHISPER_SAMPLING_GREEDY,
        WHISPER_SAMPLING_BEAM_SEARCH
    } strategy;

    int n_threads;           // Number of threads to use
    int n_max_text_ctx;      // Max text context size
    int offset_ms;           // Audio offset in milliseconds
    int duration_ms;         // Audio duration to process

    // Language settings
    bool translate;           // Translate to English
    const char* language;    // Source language (NULL = auto-detect)
    const char* initial_prompt;  // Initial prompt for better context

    // Sampling parameters
    int grabber_size;         // Audio chunk size for processing
    float voice_threshold;   // Voice activity detection threshold

    // Output settings
    bool output_txt;          // Output as plain text
    bool output_json;        // Output as JSON
    bool output_wts;          // Output word-level timestamps
    bool output_csv;         // Output as CSV

    // Performance settings
    bool speed_t2t;          // Show speed (time to transcribe)
    bool enable_special_chars; // Enable special characters
    bool no_context;        // Don't use previous transcription as context
    bool single_segment;    // Force single output segment
    bool print_special;      // Print special tokens
    bool print_progress;     // Print progress information
    bool print_realtime;     // Print results in real-time
    bool print_timestamps;   // Print timestamps

    // Callbacks
    void (*progress_callback)(struct whisper_context* ctx, 
                              struct whisper_state* state,
                              void* user_data);
    void (*new_segment_callback)(struct whisper_context* ctx,
                                 struct whisper_state* state,
                                 void* user_data);
};
```

### Default Parameters

```cpp
struct whisper_full_params whisper_default_params(
    enum whisper_sampling_strategy strategy
);

// Usage
struct whisper_full_params params = whisper_default_params(
    WHISPER_SAMPLING_GREEDY
);
params.language = "en";           // English
params.n_threads = 4;            // Use 4 threads
params.print_progress = true;    // Show progress
```

---

## Complete C++ Code Examples

### Example 1: Basic File Transcription

```cpp
#include "whisper.h"
#include <iostream>
#include <vector>
#include <cmath>

// Simple WAV reader (for demo purposes)
struct WavData {
    std::vector<float> samples;
    int sample_rate = 0;
};

bool read_wav(const char* filename, WavData& wav) {
    // Open file and read WAV header
    // This is a simplified version - use a proper library in production
    // For a complete implementation, see examples in whisper.cpp repo
    
    FILE* file = fopen(filename, "rb");
    if (!file) return false;
    
    // Skip WAV header (44 bytes) and read samples
    fseek(file, 44, SEEK_SET);
    
    // Read as 16-bit PCM and convert to float
    short sample16;
    while (fread(&sample16, sizeof(short), 1, file) == 1) {
        wav.samples.push_back(sample16 / 32768.0f);
    }
    
    wav.sample_rate = 16000;  // Whisper expects 16kHz
    fclose(file);
    return true;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <model path> <audio.wav>\n";
        return 1;
    }

    const char* model_path = argv[1];
    const char* audio_path = argv[2];

    // Initialize whisper context
    struct whisper_context* ctx = whisper_init_from_file(model_path);
    if (!ctx) {
        std::cerr << "Failed to load model from: " << model_path << "\n";
        return 1;
    }

    // Load audio
    WavData wav;
    if (!read_wav(audio_path, wav)) {
        std::cerr << "Failed to load audio from: " << audio_path << "\n";
        whisper_free(ctx);
        return 1;
    }

    // Configure parameters
    struct whisper_full_params params = whisper_default_params(
        WHISPER_SAMPLING_GREEDY
    );
    params.language = "en";
    params.n_threads = std::thread::hardware_concurrency();
    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;

    // Run transcription
    if (whisper_full(ctx, params, wav.samples.data(), wav.samples.size()) != 0) {
        std::cerr << "Transcription failed\n";
        whisper_free(ctx);
        return 1;
    }

    // Print results
    const int n_segments = whisper_full_n_segments(ctx);
    for (int i = 0; i < n_segments; i++) {
        const char* text = whisper_full_get_segment_text(ctx, i);
        printf("%s", text);
    }
    printf("\n");

    whisper_free(ctx);
    return 0;
}
```

### Example 2: Real-time Microphone Transcription

```cpp
#include "whisper.h"
#include <iostream>
#include <vector>
#include <portaudio.h>

// Configuration
constexpr int SAMPLE_RATE = 16000;
constexpr int FRAMES_PER_BUFFER = (SAMPLE_RATE / 10);  // 100ms chunks

struct TranscriptionState {
    struct whisper_context* ctx;
    struct whisper_full_params params;
    std::vector<float> audio_buffer;
    bool running;
};

int audio_callback(const void* input_buffer,
                   void* output_buffer,
                   unsigned long frames_per_buffer,
                   const PaStreamCallbackTimeInfo* time_info,
                   PaStreamCallbackFlags status_flags,
                   void* user_data) {
    
    auto* state = static_cast<TranscriptionState*>(user_data);
    const float* input = static_cast<const float*>(input_buffer);
    
    // Add samples to buffer
    state->audio_buffer.insert(state->audio_buffer.end(),
                               input, input + frames_per_buffer);
    
    // Process when we have enough audio (5 seconds)
    if (state->audio_buffer.size() >= SAMPLE_RATE * 5) {
        // Run transcription
        if (whisper_full(state->ctx, state->params,
                         state->audio_buffer.data(),
                         state->audio_buffer.size()) == 0) {
            
            // Print new segments
            int n_segments = whisper_full_n_segments(state->ctx);
            for (int i = 0; i < n_segments; i++) {
                const char* text = whisper_full_get_segment_text(state->ctx, i);
                printf("%s", text);
                fflush(stdout);
            }
        }
        
        // Keep last 2 seconds for context
        size_t keep_samples = SAMPLE_RATE * 2;
        if (state->audio_buffer.size() > keep_samples) {
            state->audio_buffer.erase(
                state->audio_buffer.begin(),
                state->audio_buffer.end() - keep_samples
            );
        }
    }
    
    return paContinue;
}

int main() {
    // Initialize PortAudio
    Pa_Initialize();
    
    // Open microphone stream
    PaStream* stream;
    PaStreamParameters params;
    params.device = Pa_GetDefaultInputDevice();
    params.channels = 1;
    params.sampleFormat = paFloat32;
    params.suggestedLatency = 0.1;
    
    // Initialize whisper
    struct whisper_context* ctx = whisper_init_from_file("./models/ggml-base.en.bin");
    if (!ctx) {
        std::cerr << "Failed to load model\n";
        return 1;
    }
    
    struct whisper_full_params whisper_params = whisper_default_params(
        WHISPER_SAMPLING_GREEDY
    );
    whisper_params.language = "en";
    whisper_params.single_segment = true;
    
    TranscriptionState state = {
        .ctx = ctx,
        .params = whisper_params,
        .running = true
    };
    
    // Open stream
    Pa_OpenStream(&stream, &params, nullptr,
                  SAMPLE_RATE, FRAMES_PER_BUFFER,
                  paClipOff, audio_callback, &state);
    
    printf("Listening... Press Ctrl+C to stop.\n");
    Pa_StartStream(stream);
    
    // Wait for interrupt
    while (state.running) {
        Pa_Sleep(100);
    }
    
    // Cleanup
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    whisper_free(ctx);
    
    return 0;
}
```

### Example 3: With Language Detection

```cpp
#include "whisper.h"
#include <iostream>
#include <unordered_map>

const char* LANGUAGES[] = {
    "en", "zh", "de", "es", "ru", "ko", "fr", "ja", "pt", "tr",
    "pl", "ca", "nl", "ar", "sv", "it", "id", "hi", "fi", "vi",
    "uk", "el", "ms", "cs", "ro", "da", "hu", "ta", "no", "th",
    "ur", "hr", "bg", "lt", "la", "mi", "ml", "cy", "sk", "te",
    "fa", "lv", "bn", "sr", "az", "sl", "kn", "et", "mk", "br",
    "eu", "is", "hy", "ne", "mn", "bs", "kk", "mk", "si", "km",
    "sn", "yo", "so", "af", "oc", "ka", "be", "tg", "sd", "gu",
    "am", "yi", "uz", "fo", "ht", "ps", "tk", "nn", "mt", "sa",
    "lb", "my", "eo", "ky", "sw", "gl", "mr", "pa", "ne", "si"
};

const char* detect_and_transcribe(struct whisper_context* ctx,
                                  const float* samples,
                                  int n_samples) {
    struct whisper_full_params params = whisper_default_params(
        WHISPER_SAMPLING_GREEDY
    );
    params.language = nullptr;  // Auto-detect
    params.n_threads = 4;
    
    // First detect language
    float lang_probs[sizeof(LANGUAGES) / sizeof(LANGUAGES[0])];
    int lang_idx = whisper_full_detect_language(ctx, params,
                                                 samples, n_samples,
                                                 lang_probs);
    
    if (lang_idx < 0 || lang_idx >= (int)(sizeof(LANGUAGES)/sizeof(LANGUAGES[0]))) {
        std::cerr << "Language detection failed\n";
        return "";
    }
    
    const char* detected_lang = LANGUAGES[lang_idx];
    printf("Detected language: %s (prob: %.2f)\n",
           detected_lang, lang_probs[lang_idx]);
    
    // Now transcribe with detected language
    params.language = detected_lang;
    whisper_full(ctx, params, samples, n_samples);
    
    // Return full transcription
    static std::string result;
    result.clear();
    
    int n_segments = whisper_full_n_segments(ctx);
    for (int i = 0; i < n_segments; i++) {
        result += whisper_full_get_segment_text(ctx, i);
        result += " ";
    }
    
    return result.c_str();
}
```

---

## Command-Line Usage

### Basic Transcription

```bash
# Transcribe an audio file
./build/bin/main -m ./models/ggml-base.en.bin -f audio.wav

# With automatic language detection
./build/bin/main -m ./models/ggml-base.bin -f audio.wav

# Translate to English
./build/bin/main -m ./models/ggml-base.bin -f audio.wav --translate
```

### Common Options

| Option | Description |
|--------|-------------|
| `-m <path>` | Path to model file |
| `-f <path>` | Path to audio file |
| `-l <lang>` | Language code (en, de, fr, etc.) |
| `-t <threads>` | Number of threads |
| `--translate` | Translate to English |
| `-otxt` | Output as plain text |
| `-oj` | Output as JSON |
| `-ocsv` | Output as CSV with timestamps |
| `-ovtt` | Output as WebVTT subtitles |
| `--no-timestamps` | Don't print timestamps |
| `--max-len <n>` | Max segment length in characters |
| `--split-on-word` | Split on word boundaries |

### Output Formats

```bash
# Plain text
./build/bin/main -m models/ggml-base.en.bin -f audio.wav -otxt

# JSON with timestamps
./build/bin/main -m models/ggml-base.en.bin -f audio.wav -oj > result.json

# CSV with timing
./build/bin/main -m models/ggml-base.en.bin -f audio.wav -ocsv > result.csv
```

---

## Additional Tools and Examples

### whisper-cli

The main command-line tool with full feature support:

```bash
./build/bin/main [options]
```

### whisper-stream (Real-time Microphone)

Real-time transcription from microphone:

```bash
# Build it
make stream

# Run with microphone input
./build/bin/stream -m ./models/ggml-base.en.bin -t 8
```

### whisper-timer

Time-based transcription for long audio files:

```bash
./build/bin/timer -m ./models/ggml-base.en.bin -f audio.wav
```

### whisper-wasm (WebAssembly)

Run whisper.cpp directly in the browser:

- Demo: [ggml.ai/whisper.cpp](https://ggml.ai/whisper.cpp/)
- Load ggml model files (recommended: tiny or base)

---

## Platform-Specific Notes

### Linux

```bash
# Install dependencies
sudo apt-get install build-essential cmake libportaudio-dev

# Build with CUDA
cmake -B build -DGGML_CUDA=ON
cmake --build build -j

# Or with Vulkan
sudo apt-get install libvulkan-dev
cmake -B build -DGGML_VULKAN=ON
cmake --build build -j
```

### macOS

Apple Silicon (M1/M2/M3) has Metal GPU acceleration enabled by default:

```bash
# Build
cmake -B build
cmake --build build -j
```

Intel Mac uses CPU-only by default.

### Windows

Using MSVC or MinGW:

```powershell
# Using CMake with MSVC
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release

# Or with MinGW
cmake -B build -G "MinGW Makefiles"
cmake --build build
```

---

## API Documentation Links

- **Main Repository**: [github.com/ggml-org/whisper.cpp](https://github.com/ggml-org/whisper.cpp)
- **Header File**: [include/whisper.h](https://github.com/ggml-org/whisper.cpp/blob/master/include/whisper.h)
- **Models on Hugging Face**: [hf.co/ggerganov/whisper.cpp](https://huggingface.co/ggerganov/whisper.cpp)
- **Official Whisper Paper**: [arXiv:2212.04356](https://arxiv.org/abs/2212.04356)
- **GGML Library**: [github.com/ggerganov/ggml](https://github.com/ggerganov/ggml)

---

## Best Practices

### Performance Optimization

1. **Choose the right model size** - Balance accuracy vs. speed for your use case
2. **Use quantized models** - Q5 quantized models offer good accuracy with smaller size
3. **Enable GPU acceleration** - Use Metal (macOS), CUDA (NVIDIA), or Vulkan (cross-platform)
4. **Match thread count** - Use `std::thread::hardware_concurrency()` for optimal threading

### Audio Quality

1. **Sample rate** - Whisper expects 16kHz mono audio
2. **Pre-processing** - Remove background noise for better accuracy
3. **Chunk size** - For real-time, use 30-second chunks with overlap

### Memory Usage

| Model | RAM Usage | VRAM (Metal) |
|-------|-----------|--------------|
| tiny | ~1 GB | ~500 MB |
| base | ~1 GB | ~500 MB |
| small | ~2 GB | ~1 GB |
| medium | ~5 GB | ~2.5 GB |
| large | ~10 GB | ~5 GB |

---

## License

Whisper.cpp is released under the MIT License.

The underlying Whisper model from OpenAI is also released under the MIT License.

---

*Document generated for Lucid project*
