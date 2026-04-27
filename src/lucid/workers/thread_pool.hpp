// thread_pool.hpp - Dual-queue worker thread pool
#pragma once

#include <glib.h>
#include <vector>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <future>

namespace lucid {

enum class TaskPriority {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2
};

struct Task {
    std::function<void()> func;
    TaskPriority priority = TaskPriority::NORMAL;
    std::chrono::steady_clock::time_point submit_time;
    std::string name;  // For debugging
    
    // For ordering in priority queue (higher priority first, then earlier submit time)
    bool operator<(const Task& other) const {
        if (priority != other.priority) {
            return priority < other.priority;  // Lower enum value = lower priority
        }
        return submit_time > other.submit_time;  // Earlier = higher priority
    }
};

class ThreadPool {
public:
    // Constructor: n_io_threads for I/O bound tasks, n_compute_threads for CPU-bound
    ThreadPool(int n_io_threads = 2, int n_compute_threads = 2);
    ~ThreadPool();
    
    // Submit I/O bound task (FFmpeg, file I/O, downloads)
    // Returns future for result
    template<typename F>
    auto push_io_task(F&& task, TaskPriority priority = TaskPriority::NORMAL, 
                      const std::string& name = "") -> std::future<std::result_of_t<F()>>;
    
    // Submit compute bound task (Whisper, Llama inference)
    template<typename F>
    auto push_compute_task(F&& task, TaskPriority priority = TaskPriority::NORMAL,
                           const std::string& name = "") -> std::future<std::result_of_t<F()>>;
    
    // Convenience: submit without return value
    void push_io_task_void(std::function<void()> task, 
                           TaskPriority priority = TaskPriority::NORMAL,
                           const std::string& name = "");
    
    void push_compute_task_void(std::function<void()> task,
                                TaskPriority priority = TaskPriority::NORMAL,
                                const std::string& name = "");
    
    // Shutdown: wait for all tasks to complete
    void shutdown();
    
    // Wait for all pending tasks
    void wait_all();
    
    // Get queue sizes (for monitoring)
    size_t io_queue_size() const;
    size_t compute_queue_size() const;
    
    // Check if shutdown
    bool is_shutdown() const { return m_shutdown.load(); }

private:
    void io_worker_loop();
    void compute_worker_loop();
    
    std::vector<std::thread> m_io_workers;
    std::vector<std::thread> m_compute_workers;
    
    std::priority_queue<Task> m_io_queue;
    std::priority_queue<Task> m_compute_queue;
    
    mutable std::mutex m_io_mutex;
    mutable std::mutex m_compute_mutex;
    
    std::condition_variable m_io_cv;
    std::condition_variable m_compute_cv;
    
    std::atomic<bool> m_shutdown{false};
    std::atomic<bool> m_waiting{false};
    
    int m_n_io_threads;
    int m_n_compute_threads;
};

// Template implementations must be in header

template<typename F>
auto ThreadPool::push_io_task(F&& task, TaskPriority priority, const std::string& name) 
    -> std::future<std::result_of_t<F()>> {
    
    using ResultType = std::result_of_t<F()>;
    auto packaged_task = std::make_shared<std::packaged_task<ResultType()>>(std::forward<F>(task));
    auto future = packaged_task->get_future();
    
    {
        std::lock_guard<std::mutex> lock(m_io_mutex);
        
        if (m_shutdown.load()) {
            // Return an already-ready future with an error
            throw std::runtime_error("ThreadPool is shutting down");
        }
        
        Task t;
        t.func = [packaged_task]() { (*packaged_task)(); };
        t.priority = priority;
        t.submit_time = std::chrono::steady_clock::now();
        t.name = name;
        
        m_io_queue.push(std::move(t));
    }
    
    m_io_cv.notify_one();
    return future;
}

template<typename F>
auto ThreadPool::push_compute_task(F&& task, TaskPriority priority, const std::string& name)
    -> std::future<std::result_of_t<F()>> {
    
    using ResultType = std::result_of_t<F()>;
    auto packaged_task = std::make_shared<std::packaged_task<ResultType()>>(std::forward<F>(task));
    auto future = packaged_task->get_future();
    
    {
        std::lock_guard<std::mutex> lock(m_compute_mutex);
        
        if (m_shutdown.load()) {
            throw std::runtime_error("ThreadPool is shutting down");
        }
        
        Task t;
        t.func = [packaged_task]() { (*packaged_task)(); };
        t.priority = priority;
        t.submit_time = std::chrono::steady_clock::now();
        t.name = name;
        
        m_compute_queue.push(std::move(t));
    }
    
    m_compute_cv.notify_one();
    return future;
}

} // namespace lucid