// folder_browser.cpp - Folder selection and video scanning
#include "folder_browser.hpp"
#include "../logging.hpp"

#include <glib.h>
#include <algorithm>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>

namespace lucid {

FolderBrowser::FolderBrowser() = default;

FolderBrowser::~FolderBrowser() = default;

static void on_folder_ready(GObject* source, GAsyncResult* result, gpointer user_data) {
    auto* async_data = static_cast<FolderBrowser::AsyncData*>(user_data);
    GtkFileDialog* dialog = GTK_FILE_DIALOG(source);
    
    GError* err = nullptr;
    GFile* folder = gtk_file_dialog_select_folder_finish(dialog, result, &err);
    
    if (folder) {
        char* path = g_file_get_path(folder);
        if (path) {
            std::lock_guard<std::mutex> lock(async_data->mutex);
            async_data->folder_path = path;
            g_free(path);
        }
        g_object_unref(folder);
    }
    
    if (err) {
        g_error_free(err);
    }
    
    g_object_unref(dialog);
    
    // Signal that we're done
    std::lock_guard<std::mutex> lock(async_data->mutex);
    async_data->ready = true;
    async_data->cv.notify_one();
}

bool FolderBrowser::run(GtkWindow* parent) {
    auto async_data = std::make_shared<AsyncData>();
    
    GtkFileDialog* dialog = GTK_FILE_DIALOG(gtk_file_dialog_new());
    gtk_file_dialog_set_title(dialog, "Select Video Folder");
    
    // Start async operation - pass shared_ptr to keep it alive
    gtk_file_dialog_select_folder(dialog, parent, nullptr, on_folder_ready, async_data.get());
    
    // Run GLib's main loop on the default main context
    // This allows GTK to process events and invoke the callback
    GMainLoop* loop = g_main_loop_new(nullptr, FALSE);
    
    // Schedule quit when async operation completes
    std::thread([async_data, loop]() {
        std::unique_lock<std::mutex> lock(async_data->mutex);
        async_data->cv.wait(lock, [async_data]() { return async_data->ready; });
        g_main_loop_quit(loop);
    }).detach();
    
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
    
    // Copy result
    {
        std::lock_guard<std::mutex> lock(async_data->mutex);
        m_folder_path = async_data->folder_path;
    }
    
    return !m_folder_path.empty();
}

const std::vector<std::string>& FolderBrowser::get_video_extensions() {
    static std::vector<std::string> extensions = {
        ".mp4", ".mkv", ".avi", ".mov", ".webm", ".m4v", 
        ".wmv", ".flv", ".f4v", ".3gp", ".ogv", ".dv",
        ".mpg", ".mpeg", ".mts", ".m2ts"
    };
    return extensions;
}

bool FolderBrowser::is_video_file(const std::string& path) {
    std::string lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    for (const auto& ext : get_video_extensions()) {
        if (lower.length() >= ext.length() && 
            lower.compare(lower.length() - ext.length(), ext.length(), ext) == 0) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> FolderBrowser::scan_for_videos(
    std::function<void(int found)> progress_callback) {
    
    std::vector<std::string> results;
    
    if (m_folder_path.empty()) {
        LUCID_WARN("No folder selected for scanning");
        return results;
    }
    
    LUCID_INFO("Scanning folder: %s", m_folder_path.c_str());
    
    std::filesystem::path dir(m_folder_path);
    scan_directory(dir, results, progress_callback);
    
    LUCID_INFO("Found %lu videos in folder", (unsigned long)results.size());
    return results;
}

void FolderBrowser::scan_directory(
    const std::filesystem::path& dir,
    std::vector<std::string>& results,
    std::function<void(int found)> progress_callback) {
    
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file()) {
                std::string path = entry.path().string();
                
                if (is_video_file(path)) {
                    results.push_back(path);
                    
                    if (progress_callback) {
                        progress_callback((int)results.size());
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        LUCID_ERROR("Error scanning directory: %s", e.what());
    }
}

} // namespace lucid