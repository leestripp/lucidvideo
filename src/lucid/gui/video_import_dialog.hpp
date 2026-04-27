// video_import_dialog.hpp - Video import dialog
#ifndef LUCID_VIDEO_IMPORT_DIALOG_HPP
#define LUCID_VIDEO_IMPORT_DIALOG_HPP

#include <gtk/gtk.h>
#include <string>
#include <vector>
#include <functional>

namespace lucid {

struct VideoImportInfo {
    std::string file_path;
    std::string title;
    std::string description;
    bool generate_transcription = true;
    bool generate_embeddings = true;
};

class VideoImportDialog {
public:
    VideoImportDialog();
    ~VideoImportDialog();
    
    // Show the dialog asynchronously
    // Returns false immediately - use set_finished_callback to be notified
    // When the dialog completes, m_finished_callback will be called with true if files were selected
    bool run(GtkWindow* parent);
    
    // Synchronous version using Gtkmm FileChooserDialog
    // Properly handles the main loop without blocking
    bool run_sync(GtkWindow* parent);
    
    // Set callback for async completion notification
    void set_finished_callback(std::function<void(bool)> callback);
    
    // Get the list of selected files
    std::vector<VideoImportInfo> get_selected_files() const { return m_files; }
    
    // Set progress callback for batch imports
    void set_progress_callback(std::function<void(int, int)> callback);
    
    GtkWidget* get_widget() const { return nullptr; }
    
    // Check if dialog has finished (for async version)
    bool is_finished() const { return m_finished; }
    
    // Check result after finished
    bool get_result() const { return m_result; }

private:
    void on_dialog_response(int response_id);
    static void on_file_ready(GObject* source, GAsyncResult* result, gpointer user_data);
    
    GtkFileDialog* m_dialog;
    std::vector<VideoImportInfo> m_files;
    std::function<void(bool)> m_finished_callback;
    std::function<void(int, int)> m_progress_callback;
    bool m_finished;
    bool m_result;
};

} // namespace lucid

#endif // LUCID_VIDEO_IMPORT_DIALOG_HPP
