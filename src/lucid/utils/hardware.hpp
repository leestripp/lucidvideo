// hardware.hpp - Hardware Detection and Performance Utilities
//
// Provides runtime detection of CPU, GPU, and memory capabilities
// for adaptive performance optimization

#pragma once

#include <string>
#include <cstdint>

namespace lucid {
namespace hardware {

// ============================================================================
// GPU Information
// ============================================================================

struct GPUInfo {
    bool available = false;
    std::string name;
    std::string vendor;
    int64_t vram_bytes = 0;
    int compute_units = 0;
    bool supports_vulkan = false;
    bool supports_cuda = false;
};

// ============================================================================
// CPU Information
// ============================================================================

struct CPUInfo {
    int physical_cores = 0;
    int logical_cores = 0;
    bool has_smt = false;
    std::string model_name;
    std::string vendor;
};

// ============================================================================
// Memory Information
// ============================================================================

struct MemoryInfo {
    int64_t total_bytes = 0;
    int64_t available_bytes = 0;
    int64_t cache_bytes = 0;
};

// ============================================================================
// Hardware Detection Functions
// ============================================================================

// Detect GPU capabilities (Vulkan, VRAM, etc.)
GPUInfo detect_gpu();

// Detect CPU cores and features
CPUInfo detect_cpu();

// Detect memory availability
MemoryInfo detect_memory();

// ============================================================================
// Utility Functions
// ============================================================================

// Get optimal thread count for compute tasks (AI inference)
int get_optimal_compute_threads();

// Get optimal thread count for I/O tasks
int get_optimal_io_threads();

// Get optimal GPU layers for a model given available VRAM
// model_size_bytes: Size of the model file in bytes
// returns: Number of layers to offload to GPU
int get_optimal_gpu_layers(int64_t model_size_bytes, const GPUInfo& gpu);

// Check if GPU acceleration is recommended for given workload
bool is_gpu_recommended();

// Get system performance profile
enum class PerformanceProfile {
    LOW_POWER,    // Laptop, battery
    BALANCED,      // Desktop, balanced
    PERFORMANCE    // Workstation, max performance
};

PerformanceProfile get_performance_profile();

// Initialize hardware detection (call once at startup)
void initialize();

// ============================================================================
// Inline Helpers
// ============================================================================

inline int64_t bytes_to_mb(int64_t bytes) { return bytes / (1024 * 1024); }
inline int64_t bytes_to_gb(int64_t bytes) { return bytes / (1024 * 1024 * 1024); }

} // namespace hardware
} // namespace lucid
