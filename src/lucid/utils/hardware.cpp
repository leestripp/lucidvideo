// hardware.cpp - Hardware Detection Implementation
//
// Runtime detection of CPU, GPU, and memory capabilities

#include "hardware.hpp"
#include "../logging.hpp"

#include <fstream>
#include <sstream>
#include <thread>
#include <filesystem>
#include <cstdlib>

#ifdef __linux__
#include <unistd.h>
#include <sys/sysinfo.h>
#endif

namespace lucid {
namespace hardware {

// ============================================================================
// GPU Detection (Linux)
// ============================================================================

GPUInfo detect_gpu() {
    GPUInfo info;
    
#ifdef __linux__
    // Try to read GPU info from /proc
    std::ifstream gpu_file("/proc/driver/nvidia/gpus/0/information");
    if (gpu_file.is_open()) {
        std::stringstream ss;
        ss << gpu_file.rdbuf();
        std::string content = ss.str();
        
        info.available = true;
        info.supports_cuda = true;
        
        // Parse NVIDIA GPU info
        if (content.find("Model:") != std::string::npos) {
            size_t pos = content.find("Model:");
            size_t start = content.find(':', pos) + 2;
            size_t end = content.find('\n', start);
            info.name = content.substr(start, end - start);
        }
        
        if (content.find("VBIOS Version:") != std::string::npos) {
            info.supports_vulkan = true;  // NVIDIA Vulkan drivers are standard
        }
        
        gpu_file.close();
    }
    
    // Check for Vulkan via vulkaninfo or direct detection
    if (!info.available) {
        // Check /dev/dri for GPU devices
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator("/dev/dri", ec)) {
            std::string name = entry.path().filename().string();
            if (name.substr(0, 4) == "card" && name != "card0") {
                info.available = true;
                info.name = "GPU " + name;
                info.supports_vulkan = true;
                break;
            }
        }
    }
    
    // Try to read VRAM from sysfs (NVIDIA or AMD)
    if (info.available) {
        std::string mem_path;
        std::error_code ec;  // Declare error_code for filesystem operations
        
        // Try NVIDIA first
        if (std::filesystem::exists("/sys/class/drm/card0/device/mem_info_vram_total", ec)) {
            mem_path = "/sys/class/drm/card0/device/mem_info_vram_total";
        }
        // Try AMD
        else if (std::filesystem::exists("/sys/class/drm/card0/device/mem_info_vram", ec)) {
            mem_path = "/sys/class/drm/card0/device/mem_info_vram_total";
        }
        
        if (!mem_path.empty()) {
            std::ifstream mem_file(mem_path);
            if (mem_file.is_open()) {
                int64_t vram_kb;
                if (mem_file >> vram_kb) {
                    info.vram_bytes = vram_kb * 1024;
                }
                mem_file.close();
            }
        }
        
        // Get vendor from PCI IDs
        std::ifstream vendor_file("/sys/class/drm/card0/device/vendor");
        if (vendor_file.is_open()) {
            std::string vendor_id;
            if (vendor_file >> vendor_id) {
                if (vendor_id == "0x10de") info.vendor = "NVIDIA";
                else if (vendor_id == "0x1002") info.vendor = "AMD";
                else if (vendor_id == "0x8086") info.vendor = "Intel";
            }
            vendor_file.close();
        }
    }
    
    // Fallback: check for Vulkan ICDs
    if (!info.available) {
        std::string icd_path = std::getenv("VULKAN_ICD_FILENAMES") ? 
                               std::getenv("VULKAN_ICD_FILENAMES") : "";
        
        if (!icd_path.empty()) {
            info.available = true;
            info.supports_vulkan = true;
            info.name = "Vulkan Compatible GPU";
        }
    }
#else
    // Non-Linux: assume no GPU for now
    info.available = false;
#endif
    
    return info;
}

// ============================================================================
// CPU Detection
// ============================================================================

CPUInfo detect_cpu() {
    CPUInfo info;
    
#ifdef __linux__
    // Get CPU info from /proc/cpuinfo
    std::ifstream cpu_file("/proc/cpuinfo");
    if (cpu_file.is_open()) {
        std::string content((std::istreambuf_iterator<char>(cpu_file)),
                           std::istreambuf_iterator<char>());
        cpu_file.close();
        
        // Count processor entries (physical cores)
        size_t processor_count = 0;
        size_t pos = 0;
        while ((pos = content.find("processor", pos)) != std::string::npos) {
            size_t newline = content.find('\n', pos);
            if (newline != std::string::npos && content.substr(pos, 11) == "processor") {
                processor_count++;
                pos = newline;
            }
        }
        
        info.logical_cores = processor_count;
        
        // Check for SMT (hyperthreading) by comparing to siblings
        int siblings = 0;
        int cores = 0;
        
        size_t siblings_pos = content.find("siblings");
        if (siblings_pos != std::string::npos) {
            size_t colon = content.find(':', siblings_pos);
            size_t newline = content.find('\n', colon);
            std::string siblings_str = content.substr(colon + 1, newline - colon - 1);
            siblings = std::stoi(siblings_str);
        }
        
        size_t cores_pos = content.find("cpu cores");
        if (cores_pos != std::string::npos) {
            size_t colon = content.find(':', cores_pos);
            size_t newline = content.find('\n', colon);
            std::string cores_str = content.substr(colon + 1, newline - colon - 1);
            cores = std::stoi(cores_str);
        }
        
        if (siblings > 0 && cores > 0 && siblings != cores) {
            info.has_smt = true;
            info.physical_cores = cores;
        } else {
            info.physical_cores = info.logical_cores;
            info.has_smt = false;
        }
        
        // Get model name
        size_t model_pos = content.find("model name");
        if (model_pos != std::string::npos) {
            size_t colon = content.find(':', model_pos);
            size_t newline = content.find('\n', colon);
            info.model_name = content.substr(colon + 2, newline - colon - 2);
        }
        
        // Get vendor
        size_t vendor_pos = content.find("vendor_id");
        if (vendor_pos != std::string::npos) {
            size_t colon = content.find(':', vendor_pos);
            size_t newline = content.find('\n', colon);
            info.vendor = content.substr(colon + 2, newline - colon - 2);
        }
    }
    
    // Fallback to sysconf if /proc/cpuinfo parsing failed
    if (info.logical_cores == 0) {
        info.logical_cores = std::thread::hardware_concurrency();
        info.physical_cores = info.logical_cores / 2;
        if (info.physical_cores == 0) info.physical_cores = info.logical_cores;
    }
#else
    info.logical_cores = std::thread::hardware_concurrency();
    info.physical_cores = info.logical_cores;
#endif
    
    return info;
}

// ============================================================================
// Memory Detection
// ============================================================================

MemoryInfo detect_memory() {
    MemoryInfo info;
    
#ifdef __linux__
    struct sysinfo sys;
    if (sysinfo(&sys) == 0) {
        info.total_bytes = sys.totalram;
        info.available_bytes = sys.freeram + sys.bufferram;
        info.cache_bytes = sys.freeram;  // Approximate cache
    }
    
    // Also check /proc/meminfo for more accurate available memory
    std::ifstream mem_file("/proc/meminfo");
    if (mem_file.is_open()) {
        std::string line;
        while (std::getline(mem_file, line)) {
            if (line.substr(0, 10) == "MemAvailable") {
                std::istringstream iss(line);
                std::string label;
                int64_t kb;
                iss >> label >> kb;
                info.available_bytes = kb * 1024;
            }
        }
        mem_file.close();
    }
#else
    // Fallback: use sysconf
    info.total_bytes = 8LL * 1024 * 1024 * 1024;  // 8 GB default
    info.available_bytes = 4LL * 1024 * 1024 * 1024;  // 4 GB default
#endif
    
    return info;
}

// ============================================================================
// Utility Functions
// ============================================================================

int get_optimal_compute_threads() {
    CPUInfo cpu = detect_cpu();
    
    // Calculate optimal threads leaving room for UI/I/O
    int optimal = cpu.physical_cores;
    
    // If SMT is enabled, don't use all logical cores
    if (cpu.has_smt) {
        optimal = cpu.physical_cores;
    }
    
    // Leave 1-2 cores for UI and system
    optimal = std::max(1, optimal - 1);
    
    // Clamp to reasonable range
    return std::min(std::max(1, optimal), 16);
}

int get_optimal_io_threads() {
    // I/O bound tasks benefit from having more threads than cores
    // since threads block on I/O
    CPUInfo cpu = detect_cpu();
    return std::min(cpu.logical_cores, 8);
}

int get_optimal_gpu_layers(int64_t model_size_bytes, const GPUInfo& gpu) {
    if (!gpu.available || gpu.vram_bytes == 0) {
        return 0;  // CPU only
    }
    
    // Calculate how many layers can fit in VRAM
    // Typical layer size: model_size / total_layers (32 for most models)
    int total_layers = 32;
    int64_t bytes_per_layer = model_size_bytes / total_layers;
    
    // Use 80% of available VRAM (safety margin)
    int64_t usable_vram = gpu.vram_bytes * 80 / 100;
    
    int layers = static_cast<int>(usable_vram / bytes_per_layer);
    
    // Cap at total layers
    layers = std::min(layers, total_layers);
    
    // Minimum: at least try to offload something
    if (layers == 0 && model_size_bytes < gpu.vram_bytes) {
        layers = 1;
    }
    
    LUCID_DEBUG("[Hardware] GPU layers: %d (model: %ld MB, VRAM: %ld MB)", 
                layers, bytes_to_mb(model_size_bytes), bytes_to_mb(gpu.vram_bytes));
    
    return layers;
}

bool is_gpu_recommended() {
    GPUInfo gpu = detect_gpu();
    MemoryInfo mem = detect_memory();
    
    // Recommend GPU if:
    // 1. GPU is available with Vulkan support
    // 2. Has at least 2GB VRAM
    // 3. Has at least 4GB available system memory
    
    return gpu.available && 
           gpu.supports_vulkan && 
           gpu.vram_bytes >= 2LL * 1024 * 1024 * 1024 &&
           mem.available_bytes >= 4LL * 1024 * 1024 * 1024;
}

PerformanceProfile get_performance_profile() {
    // Check for laptop/battery mode
    std::ifstream power_file("/sys/class/power_supply/BAT0/status");
    if (power_file.is_open()) {
        std::string status;
        std::getline(power_file, status);
        power_file.close();
        if (status == "Discharging" || status == "Unknown") {
            return PerformanceProfile::LOW_POWER;
        }
    }
    
    // Check for performance governor
    std::ifstream gov_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
    if (gov_file.is_open()) {
        std::string governor;
        std::getline(gov_file, governor);
        gov_file.close();
        if (governor == "performance") {
            return PerformanceProfile::PERFORMANCE;
        }
    }
    
    return PerformanceProfile::BALANCED;
}

// ============================================================================
// Initialization
// ============================================================================

void initialize() {
    LUCID_INFO("[Hardware] Initializing hardware detection...");
    
    CPUInfo cpu = detect_cpu();
    LUCID_INFO("[Hardware] CPU: %s", cpu.model_name.c_str());
    LUCID_INFO("[Hardware]   Cores: %d physical, %d logical (SMT: %s)",
               cpu.physical_cores, cpu.logical_cores, cpu.has_smt ? "yes" : "no");
    
    GPUInfo gpu = detect_gpu();
    if (gpu.available) {
        LUCID_INFO("[Hardware] GPU: %s (%s)", gpu.name.c_str(), gpu.vendor.c_str());
        LUCID_INFO("[Hardware]   VRAM: %ld GB, Vulkan: %s, CUDA: %s",
                   bytes_to_gb(gpu.vram_bytes),
                   gpu.supports_vulkan ? "yes" : "no",
                   gpu.supports_cuda ? "yes" : "no");
    } else {
        LUCID_INFO("[Hardware] GPU: None detected (CPU-only mode)");
    }
    
    MemoryInfo mem = detect_memory();
    LUCID_INFO("[Hardware] Memory: %ld GB total, %ld GB available",
               bytes_to_gb(mem.total_bytes), bytes_to_gb(mem.available_bytes));
    
    PerformanceProfile profile = get_performance_profile();
    const char* profile_str[] = {"Low Power", "Balanced", "Performance"};
    LUCID_INFO("[Hardware] Performance profile: %s", profile_str[static_cast<int>(profile)]);
    
    LUCID_INFO("[Hardware] Optimal threads: %d compute, %d I/O",
               get_optimal_compute_threads(), get_optimal_io_threads());
    LUCID_INFO("[Hardware] GPU recommended: %s", is_gpu_recommended() ? "yes" : "no");
}

} // namespace hardware
} // namespace lucid
