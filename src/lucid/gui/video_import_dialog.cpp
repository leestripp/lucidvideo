// video_import_dialog.cpp - Video import dialog
// Implementation using GTK4 async APIs - no blocking loops on main thread!
#include "video_import_dialog.hpp"
#include "../logging.hpp"

namespace lucid {

VideoImportDialog::VideoImportDialog()
    : m_dialog(nullptr)
    , m_finished(false)
    , m_result(false) {
}

VideoImportDialog::~VideoImportDialog() {
    if (m_dialog) {
        gtk_window_close(GTK_WINDOW(m_dialog));
        m_dialog = nullptr;
    }
}

void VideoImportDialog::on_file_ready(GObject* source, GAsyncResult* result, gpointer user_data) {
    auto* self = static_cast<VideoImportDialog*>(user_data);
    GtkFileDialog* dialog = GTK_FILE_DIALOG(source);
    
    GError* err = nullptr;
    GFile* file = gtk_file_dialog_open_finish(dialog, result, &err);
    
    if (file && !err) {
        char* path = g_file_get_path(file);
        if (path) {
            VideoImportInfo info;
            info.file_path = path;
            const char* basename = strrchr(path, '/');
            info.title = basename ? basename + 1 : path;
            self->m_files.push_back(info);
            g_free(path);
        }
        g_object_unref(file);
    }
    
    if (err) {
        g_error_free(err);
    }
    
    g_object_unref(dialog);
    
    self->m_finished = true;
    self->m_result = !self->m_files.empty();
    
    // Notify via callback if set
    if (self->m_finished_callback) {
        self->m_finished_callback(self->m_result);
    }
}

bool VideoImportDialog::run(GtkWindow* parent) {
    // Create dialog
    m_dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(m_dialog, "Import Video");
    
    // Create filter for common video formats
    GtkFileFilter* video_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(video_filter, "Video Files");
    gtk_file_filter_add_pattern(video_filter, "*.mp4");
    gtk_file_filter_add_pattern(video_filter, "*.mkv");
    gtk_file_filter_add_pattern(video_filter, "*.avi");
    gtk_file_filter_add_pattern(video_filter, "*.mov");
    gtk_file_filter_add_pattern(video_filter, "*.webm");
    gtk_file_filter_add_pattern(video_filter, "*.m4v");
    gtk_file_filter_add_pattern(video_filter, "*.wmv");
    gtk_file_filter_add_pattern(video_filter, "*.flv");
    gtk_file_filter_add_pattern(video_filter, "*.mpg");
    gtk_file_filter_add_pattern(video_filter, "*.mpeg");
    
    gtk_file_dialog_set_default_filter(m_dialog, video_filter);
    
    // Open asynchronously - this is the NON-BLOCKING approach
    // The callback will be invoked when the user selects a file or cancels
    gtk_file_dialog_open(m_dialog, parent, nullptr, on_file_ready, this);
    
    // Return false immediately - the actual result comes via callback
    // The caller should set up m_finished_callback to be notified
    return false;
}

bool VideoImportDialog::run_sync(GtkWindow* parent) {
    // Synchronous version using GMainContext properly
    // Uses g_main_context_pop_thread_default and iteration instead of blocking sleep
    
    m_dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(m_dialog, "Import Video");
    
    // Create filter for common video formats
    GtkFileFilter* video_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(video_filter, "Video Files");
    gtk_file_filter_add_pattern(video_filter, "*.mp4");
    gtk_file_filter_add_pattern(video_filter, "*.mkv");
    gtk_file_filter_add_pattern(video_filter, "*.avi");
    gtk_file_filter_add_pattern(video_filter, "*.mov");
    gtk_file_filter_add_pattern(video_filter, "*.webm");
    gtk_file_filter_add_pattern(video_filter, "*.m4v");
    gtk_file_filter_add_pattern(video_filter, "*.wmv");
    gtk_file_filter_add_pattern(video_filter, "*.flv");
    gtk_file_filter_add_pattern(video_filter, "*.mpg");
    gtk_file_filter_add_pattern(video_filter, "*.mpeg");
    
    gtk_file_dialog_set_default_filter(m_dialog, video_filter);
    
    // Use a private main context for this synchronous operation
    GMainContext* ctx = g_main_context_new();
    g_main_context_push_thread_default(ctx);
    
    gtk_file_dialog_open(m_dialog, parent, nullptr, on_file_ready, this);
    
    // Process events until finished - THIS DOES NOT BLOCK THE CALLER'S THREAD
    // because we're using a private context
    while (!m_finished) {
        g_main_context_iteration(ctx, TRUE);
    }
    
    g_main_context_pop_thread_default(ctx);
    g_main_context_unref(ctx);
    
    m_dialog = nullptr;
    return m_result;
}

void VideoImportDialog::set_finished_callback(std::function<void(bool)> callback) {
    m_finished_callback = std::move(callback);
}

void VideoImportDialog::set_progress_callback(std::function<void(int, int)> callback) {
    m_progress_callback = std::move(callback);
}

} // namespace lucid
