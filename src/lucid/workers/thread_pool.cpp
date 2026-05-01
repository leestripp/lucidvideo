// thread_pool.cpp - Dual-queue worker thread pool implementation
#include "thread_pool.hpp"
#include "../logging.hpp"
#include "../utils/hardware.hpp"

namespace lucid {

ThreadPool::ThreadPool(int n_io_threads, int n_compute_threads)
    : m_n_io_threads(n_io_threads)
    , m_n_compute_threads(n_compute_threads) {
    
    fprintf(stderr, "[ThreadPool] Constructor: n_io_threads=%d, n_compute_threads=%d\n", n_io_threads, n_compute_threads);
    fflush(stderr);
    
    // If default values (0), use hardware-optimized counts
    if (m_n_io_threads <= 0) {
        fprintf(stderr, "[ThreadPool] Detecting optimal I/O threads...\n");
        fflush(stderr);
        m_n_io_threads = hardware::get_optimal_io_threads();
        fprintf(stderr, "[ThreadPool] Optimal I/O threads: %d\n", m_n_io_threads);
        fflush(stderr);
    }
    if (m_n_compute_threads <= 0) {
        fprintf(stderr, "[ThreadPool] Detecting optimal compute threads...\n");
        fflush(stderr);
        m_n_compute_threads = hardware::get_optimal_compute_threads();
        fprintf(stderr, "[ThreadPool] Optimal compute threads: %d\n", m_n_compute_threads);
        fflush(stderr);
    }
    
    // Clamp to reasonable limits
    m_n_io_threads = std::max(1, std::min(8, m_n_io_threads));
    m_n_compute_threads = std::max(1, std::min(16, m_n_compute_threads));
    
    fprintf(stderr, "[ThreadPool] Final: %d I/O threads, %d compute threads\n", m_n_io_threads, m_n_compute_threads);
    fflush(stderr);
    
    LUCID_INFO("Creating ThreadPool: %d I/O threads, %d compute threads (adaptive)", 
               n_io_threads, n_compute_threads);
    
    // Create I/O workers
    fprintf(stderr, "[ThreadPool] Creating %d I/O worker threads...\n", m_n_io_threads);
    fflush(stderr);
    for (int i = 0; i < m_n_io_threads; ++i) {
        m_io_workers.emplace_back([this, i]() {
            LUCID_DEBUG("I/O worker thread %d started", i);
            io_worker_loop();
            LUCID_DEBUG("I/O worker thread %d stopped", i);
        });
    }
    fprintf(stderr, "[ThreadPool] I/O workers created\n");
    fflush(stderr);
    
    // Create compute workers
    fprintf(stderr, "[ThreadPool] Creating %d compute worker threads...\n", m_n_compute_threads);
    fflush(stderr);
    for (int i = 0; i < m_n_compute_threads; ++i) {
        m_compute_workers.emplace_back([this, i]() {
            LUCID_DEBUG("Compute worker thread %d started", i);
            compute_worker_loop();
            LUCID_DEBUG("Compute worker thread %d stopped", i);
        });
    }
    fprintf(stderr, "[ThreadPool] Compute workers created\n");
    fflush(stderr);
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::io_worker_loop() {
    while (!m_shutdown.load()) {
        Task task;
        
        {
            std::unique_lock<std::mutex> lock(m_io_mutex);
            
            // Wait for tasks or shutdown signal
            m_io_cv.wait(lock, [this]() {
                return !m_io_queue.empty() || m_shutdown.load();
            });
            
            if (m_shutdown.load() && m_io_queue.empty()) {
                break;
            }
            
            if (!m_io_queue.empty()) {
                task = std::move(const_cast<Task&>(m_io_queue.top()));
                m_io_queue.pop();
            }
        }
        
        if (task.func) {
            if (!task.name.empty()) {
                LUCID_DEBUG("I/O task started: %s", task.name.c_str());
            }
            try {
                task.func();
            } catch (const std::exception& e) {
                LUCID_ERROR("I/O task exception: %s", e.what());
            }
            if (!task.name.empty()) {
                LUCID_DEBUG("I/O task completed: %s", task.name.c_str());
            }
        }
    }
}

void ThreadPool::compute_worker_loop() {
    while (!m_shutdown.load()) {
        Task task;
        
        {
            std::unique_lock<std::mutex> lock(m_compute_mutex);
            
            m_compute_cv.wait(lock, [this]() {
                return !m_compute_queue.empty() || m_shutdown.load();
            });
            
            if (m_shutdown.load() && m_compute_queue.empty()) {
                break;
            }
            
            if (!m_compute_queue.empty()) {
                task = std::move(const_cast<Task&>(m_compute_queue.top()));
                m_compute_queue.pop();
            }
        }
        
        if (task.func) {
            if (!task.name.empty()) {
                LUCID_DEBUG("Compute task started: %s", task.name.c_str());
            }
            try {
                task.func();
            } catch (const std::exception& e) {
                LUCID_ERROR("Compute task exception: %s", e.what());
            }
            if (!task.name.empty()) {
                LUCID_DEBUG("Compute task completed: %s", task.name.c_str());
            }
        }
    }
}

void ThreadPool::push_io_task_void(std::function<void()> task, 
                                   TaskPriority priority,
                                   const std::string& name) {
    if (m_shutdown.load()) return;
    
    {
        std::lock_guard<std::mutex> lock(m_io_mutex);
        
        Task t;
        t.func = std::move(task);
        t.priority = priority;
        t.submit_time = std::chrono::steady_clock::now();
        t.name = name;
        
        m_io_queue.push(std::move(t));
    }
    
    m_io_cv.notify_one();
}

void ThreadPool::push_compute_task_void(std::function<void()> task,
                                       TaskPriority priority,
                                       const std::string& name) {
    if (m_shutdown.load()) return;
    
    {
        std::lock_guard<std::mutex> lock(m_compute_mutex);
        
        Task t;
        t.func = std::move(task);
        t.priority = priority;
        t.submit_time = std::chrono::steady_clock::now();
        t.name = name;
        
        m_compute_queue.push(std::move(t));
    }
    
    m_compute_cv.notify_one();
}

void ThreadPool::shutdown() {
    if (m_shutdown.exchange(true)) {
        return;  // Already shutdown
    }
    
    LUCID_INFO("ThreadPool shutting down...");
    
    // Wake up all waiting threads
    m_io_cv.notify_all();
    m_compute_cv.notify_all();
    
    // Wait for threads to finish
    for (auto& t : m_io_workers) {
        if (t.joinable()) {
            t.join();
        }
    }
    
    for (auto& t : m_compute_workers) {
        if (t.joinable()) {
            t.join();
        }
    }
    
    LUCID_INFO("ThreadPool shutdown complete");
}

void ThreadPool::wait_all() {
    m_waiting = true;
    
    // Wait until both queues are empty
    while (true) {
        size_t io_size, compute_size;
        {
            std::lock_guard<std::mutex> lock_io(m_io_mutex);
            std::lock_guard<std::mutex> lock_compute(m_compute_mutex);
            io_size = m_io_queue.size();
            compute_size = m_compute_queue.size();
        }
        
        if (io_size == 0 && compute_size == 0) {
            break;
        }
        
        // Sleep a bit and check again
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    m_waiting = false;
}

size_t ThreadPool::io_queue_size() const {
    std::lock_guard<std::mutex> lock(m_io_mutex);
    return m_io_queue.size();
}

size_t ThreadPool::compute_queue_size() const {
    std::lock_guard<std::mutex> lock(m_compute_mutex);
    return m_compute_queue.size();
}

void ThreadPool::set_io_thread_affinity(const std::vector<int>& cpu_cores) {
#ifdef __linux__
    std::lock_guard<std::mutex> lock(m_io_mutex);
    for (size_t i = 0; i < m_io_workers.size() && i < cpu_cores.size(); ++i) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_cores[i], &cpuset);
        pthread_t thread = m_io_workers[i].native_handle();
        if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) == 0) {
            LUCID_DEBUG("I/O thread %zu pinned to core %d", i, cpu_cores[i]);
        }
    }
#else
    (void)cpu_cores;  // Not supported on other platforms
#endif
}

void ThreadPool::set_compute_thread_affinity(const std::vector<int>& cpu_cores) {
#ifdef __linux__
    std::lock_guard<std::mutex> lock(m_compute_mutex);
    for (size_t i = 0; i < m_compute_workers.size() && i < cpu_cores.size(); ++i) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_cores[i], &cpuset);
        pthread_t thread = m_compute_workers[i].native_handle();
        if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) == 0) {
            LUCID_DEBUG("Compute thread %zu pinned to core %d", i, cpu_cores[i]);
        }
    }
#else
    (void)cpu_cores;  // Not supported on other platforms
#endif
}

} // namespace lucid