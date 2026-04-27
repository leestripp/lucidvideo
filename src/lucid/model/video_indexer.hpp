// video_indexer.hpp - Lucid Video Indexer
//
// GFileMonitor for directory watching and async video indexing

#pragma once

#include "database.hpp"

#include <gtkmm.h>
#include <vector>
#include <string>
#include <thread>
#include <mutex>

namespace lucid {

// ============================================================================
// Video Indexer
// ============================================================================

class VideoIndexer {
public:
    explicit VideoIndexer(Database& db);
    ~VideoIndexer();
    
    // Directory monitoring
    void watch_directory(const std::string& path);
    void unwatch_directory(const std::string& path);
    std::vector<std::string> get_watched_directories() const;
    
    // Async indexing operations
    void index_video_async(const std::string& path);
    void index_directory_async(const std::string& path);
    void download_youtube_async(const std::string& url);
    
    // Stop all background threads - call this when analysis is complete
    void stop_all_threads();
    
    // Metadata extraction
    std::string get_video_metadata(const std::string& path);

private:
    // Internal methods
    bool is_video_file(const std::string& path);
    std::string generate_uuid();
    
    // File monitor callback
    void on_directory_changed(const Glib::RefPtr<Gio::File>& file,
                              const Glib::RefPtr<Gio::File>& other_file,
                              Gio::FileMonitor::Event event_type);
    
    Database& m_db;
    std::vector<Glib::RefPtr<Gio::FileMonitor>> m_monitors;
    std::vector<std::string> m_watched_dirs;
    
    // Thread tracking for proper cleanup
    std::vector<std::thread> m_threads;
    std::mutex m_threads_mutex;
};

} // namespace lucid