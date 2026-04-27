// folder_browser.hpp - Folder selection and scanning
#pragma once

#include <gtk/gtk.h>
#include <string>
#include <vector>
#include <functional>
#include <filesystem>
#include <mutex>
#include <condition_variable>

namespace lucid {

class FolderBrowser {
public:
    // Opaque structure for async operation data
    struct AsyncData {
        std::string folder_path;
        std::mutex mutex;
        std::condition_variable cv;
        bool ready = false;
    };
    
    FolderBrowser();
    ~FolderBrowser();
    
    // Open folder picker dialog
    bool run(GtkWindow* parent);
    
    // Get selected folder path
    std::string get_folder_path() const { return m_folder_path; }
    
    // Scan folder recursively for videos
    // Returns vector of file paths
    std::vector<std::string> scan_for_videos(std::function<void(int found)> progress_callback = nullptr);
    
    // Check if path is a video file
    static bool is_video_file(const std::string& path);
    
    // Get supported video extensions
    static const std::vector<std::string>& get_video_extensions();
    
    // Used by async callback
    void set_folder_path(const std::string& path) { m_folder_path = path; }
    
private:
    std::string m_folder_path;
    std::vector<std::string> m_video_extensions = {
        ".mp4", ".mkv", ".avi", ".mov", ".webm", ".m4v", 
        ".wmv", ".flv", ".f4v", ".3gp", ".ogv", ".dv",
        ".mpg", ".mpeg", ".mts", ".m2ts"
    };
    
    void scan_directory(const std::filesystem::path& dir, 
                        std::vector<std::string>& results,
                        std::function<void(int found)> progress_callback = nullptr);
};

} // namespace lucid