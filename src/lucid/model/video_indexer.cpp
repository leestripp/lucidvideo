// video_indexer.cpp - Lucid Video Indexer Implementation
//
// GFileMonitor for directory watching and async video indexing

#include "video_indexer.hpp"

#include <giomm.h>
#include <iostream>
#include <algorithm>
#include <thread>
#include <future>

namespace lucid {

// ============================================================================
// Video File Detection
// ============================================================================

bool VideoIndexer::is_video_file(const std::string& path) {
    static const char* video_extensions[] = {
        ".mp4", ".mkv", ".avi", ".mov", ".webm", ".wmv", ".flv", ".m4v", ".mpg", ".mpeg"
    };
    
    std::string lower_path = path;
    std::transform(lower_path.begin(), lower_path.end(), lower_path.begin(), ::tolower);
    
    for (const auto& ext : video_extensions) {
        if (lower_path.length() >= strlen(ext) &&
            lower_path.compare(lower_path.length() - strlen(ext), strlen(ext), ext) == 0) {
            return true;
        }
    }
    
    return false;
}

// ============================================================================
// UUID Generation
// ============================================================================

std::string VideoIndexer::generate_uuid() {
    static const char* hex = "0123456789abcdef";
    
    std::string uuid;
    uuid.reserve(36);
    
    for (int i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            uuid += '-';
        } else {
            int r = g_random_int() % 16;
            uuid += hex[r];
        }
    }
    
    uuid[14] = '4';
    uuid[19] = hex[g_random_int() % 4 + 8];
    
    return uuid;
}

// ============================================================================
// Video Indexer Implementation
// ============================================================================

VideoIndexer::VideoIndexer(Database& db)
    : m_db(db) {
}

VideoIndexer::~VideoIndexer() {
    for (auto& monitor : m_monitors) {
        monitor->cancel();
    }
    // Stop all background threads
    stop_all_threads();
}

void VideoIndexer::watch_directory(const std::string& path) {
    auto file = Gio::File::create_for_path(path);
    
    if (!file->query_exists()) {
        std::cerr << "[ERROR] Directory does not exist: " << path << std::endl;
        return;
    }
    
    try {
        auto monitor = file->monitor_directory();
        
        monitor->signal_changed().connect(
            sigc::mem_fun(*this, &VideoIndexer::on_directory_changed)
        );
        
        m_monitors.push_back(monitor);
        m_watched_dirs.push_back(path);
        std::cout << "[INFO] Now watching directory: " << path << std::endl;
        
    } catch (const Glib::Error& e) {
        std::cerr << "[ERROR] Failed to watch directory: " << e.what() << std::endl;
    }
}

void VideoIndexer::on_directory_changed(const Glib::RefPtr<Gio::File>& file,
                                       const Glib::RefPtr<Gio::File>& /* other_file */,
                                       Gio::FileMonitor::Event event_type) {
    
    if (event_type == Gio::FileMonitor::Event::CREATED ||
        event_type == Gio::FileMonitor::Event::MOVED_IN) {
        
        auto file_path = file->get_path();
        if (!file_path.empty() && is_video_file(file_path)) {
            std::cout << "[INFO] New video detected: " << file_path << std::endl;
            index_video_async(file_path);
        }
    }
}

void VideoIndexer::unwatch_directory(const std::string& path) {
    for (size_t i = 0; i < m_watched_dirs.size(); ++i) {
        if (m_watched_dirs[i] == path) {
            m_monitors[i]->cancel();
            m_monitors.erase(m_monitors.begin() + i);
            m_watched_dirs.erase(m_watched_dirs.begin() + i);
            break;
        }
    }
}

std::vector<std::string> VideoIndexer::get_watched_directories() const {
    return m_watched_dirs;
}

// ============================================================================
// Indexing
// ============================================================================

void VideoIndexer::index_video_async(const std::string& path) {
    // Track thread for proper cleanup
    std::thread t([this, path]() {
        try {
            VideoEntry video;
            video.id = generate_uuid();
            video.path = path;
            video.title = Glib::path_get_basename(path);
            video.source = "local";
            
            m_db.add_video(video);
            std::cout << "[INFO] Indexed video: " << video.title << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Failed to index video: " << e.what() << std::endl;
        }
    });
    
    // Track thread instead of detaching
    std::lock_guard<std::mutex> lock(m_threads_mutex);
    m_threads.push_back(std::move(t));
}

void VideoIndexer::index_directory_async(const std::string& path) {
    std::thread t([this, path]() {
        try {
            auto dir = Gio::File::create_for_path(path);
            auto enumerator = dir->enumerate_children(
                "standard::name,standard::type",
                Gio::FileQueryInfoFlags::NONE
            );
            
            std::vector<std::string> video_paths;
            Glib::RefPtr<Gio::FileInfo> info;
            
            while ((info = enumerator->next_file())) {
                if (info->get_file_type() == Gio::FileType::REGULAR) {
                    auto file_path = Glib::build_filename(path, info->get_name());
                    if (is_video_file(file_path)) {
                        video_paths.push_back(file_path);
                    }
                }
            }
            
            std::cout << "[INFO] Found " << video_paths.size() << " videos in " << path << std::endl;
            
            for (const auto& video_path : video_paths) {
                index_video_async(video_path);
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Failed to index directory: " << e.what() << std::endl;
        }
    });
    
    // Track thread instead of detaching
    std::lock_guard<std::mutex> lock(m_threads_mutex);
    m_threads.push_back(std::move(t));
}

void VideoIndexer::download_youtube_async(const std::string& url) {
    std::thread t([url]() {
        std::cout << "[INFO] Downloading YouTube video: " << url << std::endl;
    });
    
    // Track thread instead of detaching
    std::lock_guard<std::mutex> lock(m_threads_mutex);
    m_threads.push_back(std::move(t));
}

void VideoIndexer::stop_all_threads() {
    std::lock_guard<std::mutex> lock(m_threads_mutex);
    for (auto& t : m_threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    m_threads.clear();
}

std::string VideoIndexer::get_video_metadata(const std::string& path) {
    return path;
}

} // namespace lucid