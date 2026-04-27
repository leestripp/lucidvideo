// thread_pool.cpp - Dual-queue worker thread pool implementation
#include "thread_pool.hpp"
#include "../logging.hpp"

namespace lucid {

ThreadPool::ThreadPool(int n_io_threads, int n_compute_threads)
    : m_n_io_threads(n_io_threads)
    , m_n_compute_threads(n_compute_threads) {
    
    LUCID_INFO("Creating ThreadPool: %d I/O threads, %d compute threads", 
               n_io_threads, n_compute_threads);
    
    // Create I/O workers
    for (int i = 0; i < n_io_threads; ++i) {
        m_io_workers.emplace_back([this, i]() {
            LUCID_DEBUG("I/O worker thread %d started", i);
            io_worker_loop();
            LUCID_DEBUG("I/O worker thread %d stopped", i);
        });
    }
    
    // Create compute workers
    for (int i = 0; i < n_compute_threads; ++i) {
        m_compute_workers.emplace_back([this, i]() {
            LUCID_DEBUG("Compute worker thread %d started", i);
            compute_worker_loop();
            LUCID_DEBUG("Compute worker thread %d stopped", i);
        });
    }
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

} // namespace lucid