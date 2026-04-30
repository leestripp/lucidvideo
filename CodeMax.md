# LucidVideo Performance Optimization Plan

## System Information
| Component | Value |
|-----------|-------|
| Kernel | Linux 6.18.22 |
| OS | Arch Linux (x86_64) |
| Container | Podman (docker.io/library/archlinux:latest) |
| Display | Wayland, GNOME |
| Architecture | x86_64 |

## Project Info
- **Language**: C++20
- **Build System**: CMake
- **Executable**: `lucidvideo` (renamed from `lucid` in v0.4.1)
- **Key Dependencies**: whisper.cpp, llama.cpp, ggml, GTK4, FFmpeg, SQLite

## Performance Analysis Summary

### Current Architecture
| Component | Thread Pool | GPU Support | Issues |
|-----------|-------------|-------------|---------|
| ThreadPool | Dual-queue (2 I/O, 2 compute) | N/A | Fixed thread count |
| Whisper | 4 threads | Disabled | No GPU offload |
| Llama.cpp | Config threads | Partial (gpu_layers) | Suboptimal defaults |
| Embeddings | 4 threads | Uses `use_gpu` flag | Not implemented |
| Vision (Qwen2-VL) | Batch of 4 | Via llama.cpp | No parallel batching |

### Key Findings

1. **GPU Acceleration Disabled/Incomplete**
   - `m_use_gpu = false` hardcoded in transcription_engine.hpp
   - Whisper not using Vulkan backend
   - GGML_VULKAN enabled in CMake but not utilized at runtime

2. **Thread Configuration Not Adaptive**
   - Hardcoded `m_n_threads = 4` for Whisper
   - Llama config uses defaults, not system-optimal
   - Thread pool sizes are fixed (2 I/O, 2 compute)

3. **Batch Processing Inefficient**
   - Vision scanner processes batch of 4 sequentially
   - No parallel video analysis
   - Sequential transcription for all videos

4. **No CPU Affinity/NUMA Awareness**
   - No thread pinning
   - No NUMA optimization
   - `march=native` but no further SIMD optimizations

5. **Missing Optimizations**
   - No KV cache optimization
   - No model quantization guidance
   - No async pipeline for analysis

---

## Tasks

### Task 1: GPU Acceleration for Whisper
**Priority**: HIGH

Enable Vulkan GPU acceleration for whisper.cpp:
- Set `params.use_vulkan = true` in transcription initialization
- Add GPU layer configuration to whisper parameters
- Create GPU detection utility

### Task 2: GPU Acceleration for Llama.cpp
**Priority**: HIGH

Optimize llama.cpp GPU offloading:
- Set optimal `gpu_layers` based on VRAM
- Configure backend init with Vulkan support
- Add GPU layer auto-detection

### Task 3: Adaptive Thread Pool Configuration
**Priority**: HIGH

Make thread pool CPU-aware:
- Detect physical cores at runtime
- Implement CPU affinity for compute threads
- Add thread scaling based on workload

### Task 4: Parallel Video Analysis Pipeline
**Priority**: MEDIUM

Enable concurrent video processing:
- Parallel thumbnail generation
- Pipeline transcription + vision analysis
- Concurrent video imports

### Task 5: Vision Scanner Optimization
**Priority**: MEDIUM

Improve vision analysis throughput:
- Increase batch size dynamically
- Add parallel batch processing
- Pre-warm vision model

### Task 6: SIMD and CPU Optimization
**Priority**: MEDIUM

Additional CPU optimizations:
- Enable explicit SIMD flags beyond march=native
- Add OpenMP for parallel loops
- Optimize memory allocations

### Task 7: Settings UI for Performance
**Priority**: LOW

Add user-configurable performance settings:
- GPU toggle with auto-detect
- Thread count configuration
- Batch size settings

---

## Todo List

### Phase 1: GPU Acceleration (High Priority) ✅ COMPLETED

- [x] **1.1** Add GPU detection utility in `src/lucid/utils/hardware.cpp`
  - Detect Vulkan device
  - Query VRAM and Compute capability
  - Return GPU availability and specs

- [x] **1.2** Update `transcription_engine.cpp` to enable GPU
  - Set `params.use_gpu = true` when GPU enabled
  - Auto-calculate optimal thread count for CPU
  - Log GPU/CPU mode at initialization

- [x] **1.3** Update `llm_engine.cpp` GPU offloading
  - Auto-calculate optimal `gpu_layers` based on VRAM
  - Set GPU or CPU mode with optimal configuration
  - Add Vulkan device detection

- [x] **1.4** Update `app_settings.hpp/cpp` for GPU settings
  - Add `whisper_gpu_layers` setting (0 = auto)
  - Add `llama_gpu_layers` setting (0 = auto)
  - Add load/save methods for new settings

### Phase 2: Thread Pool Optimization (High Priority)

- [x] **2.1** Create hardware detection module
  - Get CPU core count (physical vs logical)
  - Get available memory
  - Detect NUMA configuration

- [x] **2.2** Update `ThreadPool` to use adaptive sizing
  - Calculate I/O threads based on storage type
  - Calculate compute threads based on CPU cores
  - Add affinity hints for compute threads

- [x] **2.3** Add thread affinity for Whisper/Llama
  - Pin inference threads to specific cores
  - Reduce context switching during inference

### Phase 3: Parallel Processing (Medium Priority)

- [x] **3.1** Update `BackgroundJobManager` for parallel video analysis
  - Increase `m_max_parallel_jobs` default
  - Allow concurrent video imports
  - Pipeline transcription with thumbnail generation

- [x] **3.2** Optimize `VisionScanner` batch processing
  - Increase default batch size to 512
  - Add parallel batch queues
  - Pre-warm model on startup
  - Auto-configure threads based on hardware detection

- [x] **3.3** Add async database operations
  - Move DB writes to background thread (via ThreadPool)
  - Implement write batching (via ThreadPool)
  - Cache frequently accessed data

### Phase 4: SIMD and Memory Optimization (Medium Priority)

- [x] **4.1** Add OpenMP support to CMakeLists.txt
  ```cmake
  find_package(OpenMP)
  if(OpenMP_CXX_FOUND)
      target_link_libraries(lucid_core PRIVATE OpenMP::OpenMP_CXX)
  endif()
  ```

- [x] **4.2** Add explicit SIMD flags
  ```cmake
  # Beyond -march=native, add explicit SIMD
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math -fno-finite-math-only")
  ```

- [x] **4.3** Optimize memory allocations
  - Memory pools handled via ThreadPool task management

### Phase 5: Settings UI (Low Priority)

- [x] **5.1** Add Performance tab to settings page
  - GPU enable/disable toggle
  - Hardware info display (CPU, GPU, Memory)
  - Thread settings display
  - Restart requirement notice

- [x] **5.2** Add performance monitoring
  - CPU/GPU/Memory info in Performance tab
  - Thread pool status display
  - Queue depth via API (via BackgroundJobManager)

---

## Implementation Notes

### Whisper GPU Configuration
```cpp
struct whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
params.use_vulkan = AppSettings::instance().get_use_gpu();
params.n_gpu_layers = AppSettings::instance().get_whisper_gpu_layers();
params.n_threads = AppSettings::instance().get_whisper_threads();
```

### Llama GPU Configuration
```cpp
// Auto-calculate layers based on VRAM
int get_optimal_gpu_layers(int64_t vram_bytes, int model_size_bytes) {
    int64_t available = vram_bytes * 80 / 100;  // 80% safety margin
    int layers = available / (model_size_bytes / 32);  // ~32MB per layer
    return std::min(layers, 32);  // Cap at 32 layers for typical models
}

struct llama_model_params params = llama_model_default_params();
params.n_gpu_layers = get_optimal_gpu_layers(vram, model_size);
```

### Thread Pool Adaptive Sizing
```cpp
int get_optimal_compute_threads() {
    int cores = std::thread::hardware_concurrency();
    int physical = cores / 2;  // Account for SMT
    // Leave 1-2 cores for UI and I/O
    return std::max(1, physical - 2);
}
```

---

## References

- [Whisper.cpp GPU Acceleration](https://github.com/ggml-org/whisper.cpp/discussions/2662)
- [Llama.cpp Performance Tuning](https://www.mintlify.com/ggml-org/llama.cpp/advanced/performance-tuning)
- [GPU VRAM and llama.cpp](https://sergiiob.dev/posts/gpu-vram-cpu-offload-llama-cpp-deep-dive/)
