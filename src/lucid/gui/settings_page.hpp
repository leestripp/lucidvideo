// settings_page.hpp - Application settings management
#pragma once

#include <gtk/gtk.h>
#include <string>
#include <vector>
#include <functional>

// Use ThumbnailSize from AppSettings (single source of truth)
#include "../settings/app_settings.hpp"
#include "../utils/hardware.hpp"

namespace lucid {

// Forward declaration
class MainWindow;
class ModelManager;

// AppSettings struct removed - settings are now saved immediately via ModelManager
// setters in each callback. No need for batch save via AppSettings.

class SettingsPage {
public:
    SettingsPage();
    ~SettingsPage();
    
    // Initialize with parent window
    void initialize(GtkWidget* parent);
    
    // Load current settings
    void load_settings();
    
    // Save current settings
    bool save_settings();
    
    // Refresh model dropdowns from ModelManager catalog
    void refresh_model_dropdowns();
    
    // Sync settings values from ModelManager (call after ModelManager is initialized)
    void sync_from_model_manager();
    
    // Sync member variables to actual widgets (call after UI is created)
    void sync_to_widgets();
    
    // Get settings widget
    GtkWidget* get_widget() const { return m_notebook; }
    
    // Callbacks
    void set_on_folder_added(void (*callback)(const std::string&));
    void set_on_folder_removed(void (*callback)(const std::string&));
    void set_on_thumbnail_size_changed(std::function<void(ThumbnailSize)> callback);
    void set_on_vision_extract_size_changed(std::function<void(VisionExtractSize)> callback);
    
    // Set the parent MainWindow to notify when buttons are clicked
    void set_parent_window(void* window);
    
    // Get the notebook widget for tab navigation
    GtkWidget* get_notebook() const { return m_notebook; }
    
    // Navigate to a specific tab by name
    // Valid tab names: "general", "folders", "models", "advanced"
    void set_active_tab(const std::string& tab_name);
    
    // Get current tab index
    int get_active_tab() const {
        if (m_notebook) {
            return gtk_notebook_get_current_page(GTK_NOTEBOOK(m_notebook));
        }
        return -1;
    }
    
    // Get current tab name
    std::string get_active_tab_name() const {
        int page = get_active_tab();
        if (page < 0 || page > 2) return "unknown";
        const char* tab_names[] = {"general", "folders", "performance"};
        return tab_names[page];
    }
    
    // Simulate widget interaction for testing
    // widget_name: "whisper_combo", "embedding_combo", "gpu_switch", etc.
    void simulate_widget_interaction(const std::string& widget_name, const std::string& value);
    
    // Get settings state as JSON for debugging
    std::string get_settings_state_json() const;

private:
    void setup_general_page();
    void setup_folders_page();
    void setup_models_page();
    void setup_performance_page();
    
    void add_folder_row(const std::string& path);
    void remove_folder_row(const std::string& path);
    
    static void on_add_folder_clicked(GtkButton* button, gpointer data);
    static void on_remove_folder_clicked(GtkButton* button, gpointer data);
    static void on_gpu_toggled(GtkSwitch* switch_widget, gpointer data);
    static void on_perf_gpu_toggled(GtkSwitch* switch_widget, gpointer data);
    static void on_parallel_jobs_changed(GtkSpinButton* button, gpointer data);
    static void on_embedding_context_changed(GtkSpinButton* button, gpointer data);
    static void on_whisper_context_changed(GtkSpinButton* button, gpointer data);
    static void on_whisper_model_changed(GtkComboBox* combo, gpointer data);
    static void on_embedding_model_changed(GtkComboBox* combo, gpointer data);
    static void on_llm_model_changed(GtkComboBox* combo, gpointer data);
    static void on_thumbnail_size_changed(GtkComboBox* combo, gpointer data);
    static void on_browse_clicked(GtkButton* button, gpointer data);
    static void on_vision_toggled(GtkSwitch* switch_widget, gpointer data);
    static void on_vision_interval_changed(GtkSpinButton* button, gpointer data);
    static void on_vision_max_changed(GtkSpinButton* button, gpointer data);
    static void on_vision_extract_size_changed(GtkComboBox* combo, gpointer data);
    
    GtkWidget* m_notebook = nullptr;
    GtkWidget* m_folders_list = nullptr;
    GtkWidget* m_whisper_combo = nullptr;
    GtkWidget* m_embedding_combo = nullptr;
    GtkWidget* m_llm_combo = nullptr;
    std::vector<std::string> m_watched_folders;
    
    // Settings values
    std::string m_whisper_model = "whisper-tiny";
    std::string m_embedding_model = "nomic-embed-text";
    std::string m_llm_model = "";
    bool m_use_gpu = true;
    int m_max_parallel_jobs = 4;
    int m_embedding_context_size = 2048;
    int m_whisper_context_size = 16384;
    ThumbnailSize m_thumbnail_size = ThumbnailSize::MEDIUM;
    GtkWidget* m_thumbnail_size_combo = nullptr;
    
    // Vision scan settings (always enabled - no toggle)
    int m_vision_scan_interval = 10;  // Analyze every Nth thumbnail
    int m_vision_max_per_video = 25;  // Max thumbnails to analyze per video
    VisionExtractSize m_vision_extract_size = VisionExtractSize::MEDIUM;  // 512px for AI analysis
    GtkWidget* m_vision_interval_spin = nullptr;
    GtkWidget* m_vision_max_spin = nullptr;
    GtkWidget* m_vision_extract_combo = nullptr;
    
    // Performance page widgets (for monitoring)
    GtkWidget* m_perf_cpu_label = nullptr;
    GtkWidget* m_perf_gpu_label = nullptr;
    GtkWidget* m_perf_memory_label = nullptr;
    GtkWidget* m_perf_queue_label = nullptr;
    GtkWidget* m_perf_gpu_switch = nullptr;
    GtkWidget* m_perf_thread_count_label = nullptr;
    
    // Callbacks
    void (*m_folder_added_callback)(const std::string&) = nullptr;
    void (*m_folder_removed_callback)(const std::string&) = nullptr;
    std::function<void(ThumbnailSize)> m_thumbnail_size_changed_callback = nullptr;
    std::function<void(VisionExtractSize)> m_vision_extract_size_changed_callback = nullptr;
    
    // Parent window for navigation callbacks
    void* m_parent_window = nullptr;
    
    // Initialization guard to prevent callbacks during setup
    bool m_initialized = false;
};

} // namespace lucid