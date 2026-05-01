// settings_page.cpp - Application settings management implementation
#pragma GCC diagnostic push
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include "settings_page.hpp"
#include "main_window.hpp"
#include "../model/model_manager.hpp"
#include "../logging.hpp"

#include <gtk/gtk.h>
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <json/json.h>  // For get_settings_state_json()

namespace lucid {

SettingsPage::SettingsPage() = default;
SettingsPage::~SettingsPage() = default;

void SettingsPage::initialize(GtkWidget* /*parent*/) {
    LUCID_INFO("[SettingsPage] initialize() called - Creating notebook");
    
    LUCID_INFO("[SettingsPage] initialize - About to call gtk_notebook_new()");
    fprintf(stderr, "[DEBUG] SettingsPage::initialize - about to call gtk_notebook_new()\n");
    fflush(stderr);
    
    // Create notebook for tabs
    m_notebook = gtk_notebook_new();
    
    fprintf(stderr, "[DEBUG] SettingsPage::initialize - gtk_notebook_new() returned, m_notebook=%p\n", (void*)m_notebook);
    fflush(stderr);
    
    LUCID_INFO("[SettingsPage] initialize - gtk_notebook_new() returned");
    
    gtk_widget_set_margin_start(m_notebook, 12);
    gtk_widget_set_margin_end(m_notebook, 12);
    gtk_widget_set_margin_top(m_notebook, 12);
    gtk_widget_set_margin_bottom(m_notebook, 12);
    LUCID_INFO("[SettingsPage] initialize - margins set on notebook");
    
    // CRITICAL: Load settings from ModelManager FIRST, before creating any widgets
    // This ensures member variables have the correct values when widgets are created
    LUCID_INFO("[SettingsPage] initialize() - Calling load_settings()");
    load_settings();
    LUCID_INFO("[SettingsPage] initialize() - load_settings() returned");
    
    // Create tabs - ONLY non-model settings are editable at runtime
    LUCID_INFO("[SettingsPage] initialize() - Setting up general page");
    setup_general_page();
    LUCID_INFO("[SettingsPage] initialize() - Setting up folders page");
    setup_folders_page();
    LUCID_INFO("[SettingsPage] initialize() - Setting up performance page");
    setup_performance_page();
    LUCID_INFO("[SettingsPage] initialize() - DONE");
    
    fprintf(stderr, "[DEBUG] SettingsPage::initialize - exiting\n");
    fflush(stderr);
}

void SettingsPage::setup_general_page() {
    GtkWidget* page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(page, 12);
    gtk_widget_set_margin_end(page, 12);
    gtk_widget_set_margin_top(page, 12);
    gtk_widget_set_margin_bottom(page, 12);
    
    // Page title
    GtkWidget* title = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(title), "<span size='large' weight='bold'>General</span>");
    gtk_box_append(GTK_BOX(page), title);
    
    // Description
    GtkWidget* desc = gtk_label_new(
        "Basic settings for video browsing. Note: AI model settings cannot be "
        "changed while the app is running - they require a restart."
    );
    gtk_label_set_wrap(GTK_LABEL(desc), TRUE);
    gtk_label_set_justify(GTK_LABEL(desc), GTK_JUSTIFY_LEFT);
    gtk_box_append(GTK_BOX(page), desc);
    
    // Thumbnail size selection
    GtkWidget* thumb_size_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* thumb_size_label = gtk_label_new("Thumbnail display size");
    gtk_widget_set_hexpand(thumb_size_label, TRUE);
    gtk_box_append(GTK_BOX(thumb_size_box), thumb_size_label);
    
    GtkWidget* thumb_size_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(thumb_size_combo), "0", "Small (80x45)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(thumb_size_combo), "1", "Medium (160x90)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(thumb_size_combo), "2", "Large (240x135)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(thumb_size_combo), static_cast<int>(m_thumbnail_size));
    gtk_widget_set_size_request(thumb_size_combo, 150, -1);
    g_signal_connect(thumb_size_combo, "changed", G_CALLBACK(on_thumbnail_size_changed), this);
    gtk_box_append(GTK_BOX(thumb_size_box), thumb_size_combo);
    gtk_box_append(GTK_BOX(page), thumb_size_box);
    m_thumbnail_size_combo = thumb_size_combo;
    
    // Spacer
    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(page), spacer);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(m_notebook), page, gtk_label_new("General"));
}

void SettingsPage::setup_folders_page() {
    LUCID_INFO("[SettingsPage] setup_folders_page - Starting");
    GtkWidget* page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(page, 12);
    gtk_widget_set_margin_end(page, 12);
    gtk_widget_set_margin_top(page, 12);
    gtk_widget_set_margin_bottom(page, 12);
    
    // Page title
    GtkWidget* title = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(title), "<span size='large' weight='bold'>Watched Folders</span>");
    gtk_box_append(GTK_BOX(page), title);
    
    GtkWidget* desc = gtk_label_new(
        "Add folders to scan for videos. Videos will be automatically "
        "transcribed and indexed for search."
    );
    gtk_label_set_wrap(GTK_LABEL(desc), TRUE);
    gtk_label_set_justify(GTK_LABEL(desc), GTK_JUSTIFY_LEFT);
    gtk_box_append(GTK_BOX(page), desc);
    
    // Folders list container
    GtkWidget* scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_box_append(GTK_BOX(page), scroll);
    
    m_folders_list = gtk_list_box_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), m_folders_list);
    
    // Add folder button
    GtkWidget* add_btn = gtk_button_new_with_label("Add Folder");
    gtk_button_set_icon_name(GTK_BUTTON(add_btn), "folder-open");
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_folder_clicked), this);
    gtk_box_append(GTK_BOX(page), add_btn);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(m_notebook), page, gtk_label_new("Folders"));
    LUCID_INFO("[SettingsPage] setup_folders_page - DONE");
}

void SettingsPage::setup_models_page() {
    // Models tab is no longer shown - models are hardcoded and cannot be changed at runtime
    // The Models tab was removed to prevent UI confusion and app crashes
}

void SettingsPage::setup_performance_page() {
    fprintf(stderr, "[DEBUG] setup_performance_page - starting\n");
    fflush(stderr);
    
    GtkWidget* page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    fprintf(stderr, "[DEBUG] setup_performance_page - created page box\n");
    fflush(stderr);
    
    gtk_widget_set_margin_start(page, 12);
    gtk_widget_set_margin_end(page, 12);
    gtk_widget_set_margin_top(page, 12);
    gtk_widget_set_margin_bottom(page, 12);
    
    // Page title
    GtkWidget* title = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(title), "<span size='large' weight='bold'>Performance</span>");
    gtk_box_append(GTK_BOX(page), title);
    
    // Description
    GtkWidget* desc = gtk_label_new(
        "Hardware performance settings. Changes require app restart to take effect."
    );
    gtk_label_set_wrap(GTK_LABEL(desc), TRUE);
    gtk_label_set_justify(GTK_LABEL(desc), GTK_JUSTIFY_LEFT);
    gtk_box_append(GTK_BOX(page), desc);
    
    // Separator
    GtkWidget* sep1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(page), sep1);
    
    // Hardware Info Section
    GtkWidget* hw_title = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(hw_title), "<span size='medium' weight='bold'>Hardware</span>");
    gtk_widget_set_halign(hw_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(page), hw_title);
    
    fprintf(stderr, "[DEBUG] setup_performance_page - detecting CPU\n");
    fflush(stderr);
    
    // CPU Info
    lucid::hardware::CPUInfo cpu = lucid::hardware::detect_cpu();
    fprintf(stderr, "[DEBUG] setup_performance_page - CPU detected: %s\n", cpu.model_name.c_str());
    fflush(stderr);
    
    std::string cpu_text = "CPU: " + cpu.model_name + 
                          " (" + std::to_string(cpu.physical_cores) + " cores, " +
                          std::to_string(cpu.logical_cores) + " threads)";
    if (cpu.has_smt) cpu_text += " [SMT]";
    GtkWidget* cpu_label = gtk_label_new(cpu_text.c_str());
    gtk_widget_set_halign(cpu_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(page), cpu_label);
    m_perf_cpu_label = cpu_label;
    
    fprintf(stderr, "[DEBUG] setup_performance_page - detecting GPU\n");
    fflush(stderr);
    
    // GPU Info
    lucid::hardware::GPUInfo gpu = lucid::hardware::detect_gpu();
    fprintf(stderr, "[DEBUG] setup_performance_page - GPU detected: available=%d\n", gpu.available);
    fflush(stderr);
    std::string gpu_text = "GPU: ";
    if (gpu.available) {
        gpu_text += gpu.name + " (" + gpu.vendor + ")";
        if (gpu.vram_bytes > 0) {
            gpu_text += " - " + std::to_string(gpu.vram_bytes / (1024*1024*1024)) + " GB VRAM";
        }
        if (gpu.supports_vulkan) gpu_text += " [Vulkan]";
    } else {
        gpu_text += "None detected (CPU-only)";
    }
    GtkWidget* gpu_label = gtk_label_new(gpu_text.c_str());
    gtk_widget_set_halign(gpu_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(page), gpu_label);
    m_perf_gpu_label = gpu_label;
    
    // Memory Info
    lucid::hardware::MemoryInfo mem = lucid::hardware::detect_memory();
    std::string mem_text = "Memory: " + 
                          std::to_string(mem.total_bytes / (1024*1024*1024)) + " GB total, " +
                          std::to_string(mem.available_bytes / (1024*1024*1024)) + " GB available";
    GtkWidget* mem_label = gtk_label_new(mem_text.c_str());
    gtk_widget_set_halign(mem_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(page), mem_label);
    m_perf_memory_label = mem_label;
    
    // Separator
    GtkWidget* sep2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(page), sep2);
    
    // GPU Settings Section
    GtkWidget* gpu_settings_title = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(gpu_settings_title), "<span size='medium' weight='bold'>GPU Settings</span>");
    gtk_widget_set_halign(gpu_settings_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(page), gpu_settings_title);
    
    // GPU Enable Row
    GtkWidget* gpu_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* gpu_label_setting = gtk_label_new("Enable GPU acceleration");
    gtk_widget_set_hexpand(gpu_label_setting, TRUE);
    gtk_box_append(GTK_BOX(gpu_row), gpu_label_setting);
    
    GtkWidget* gpu_switch = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(gpu_switch), m_use_gpu);
    g_signal_connect(gpu_switch, "state-set", G_CALLBACK(on_perf_gpu_toggled), this);
    gtk_box_append(GTK_BOX(gpu_row), gpu_switch);
    gtk_box_append(GTK_BOX(page), gpu_row);
    m_perf_gpu_switch = gpu_switch;
    
    // Separator
    GtkWidget* sep3 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(page), sep3);
    
    // Thread Settings Section
    GtkWidget* thread_title = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(thread_title), "<span size='medium' weight='bold'>Thread Settings</span>");
    gtk_widget_set_halign(thread_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(page), thread_title);
    
    // Compute threads info
    int compute_threads = lucid::hardware::get_optimal_compute_threads();
    int io_threads = lucid::hardware::get_optimal_io_threads();
    std::string thread_text = "Compute threads: " + std::to_string(compute_threads) + 
                              " (I/O threads: " + std::to_string(io_threads) + ")";
    GtkWidget* thread_label = gtk_label_new(thread_text.c_str());
    gtk_widget_set_halign(thread_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(page), thread_label);
    m_perf_thread_count_label = thread_label;
    
    // Max parallel jobs info
    std::string jobs_text = "Max parallel jobs: " + std::to_string(m_max_parallel_jobs);
    GtkWidget* jobs_label = gtk_label_new(jobs_text.c_str());
    gtk_widget_set_halign(jobs_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(page), jobs_label);
    
    // Separator
    GtkWidget* sep4 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(page), sep4);
    
    // Note about restart
    GtkWidget* note = gtk_label_new(
        "Note: Thread and GPU settings are auto-detected based on your hardware.\n"
        "For manual configuration, edit ~/.config/lucid/settings.json"
    );
    gtk_label_set_wrap(GTK_LABEL(note), TRUE);
    gtk_label_set_justify(GTK_LABEL(note), GTK_JUSTIFY_LEFT);
    gtk_box_append(GTK_BOX(page), note);
    
    // Spacer
    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(page), spacer);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(m_notebook), page, gtk_label_new("Performance"));
}

void SettingsPage::load_settings() {
    LUCID_INFO("[SettingsPage] load_settings - Starting");
    // Only sync member variables from ModelManager (which loaded config from file)
    // Widgets may not exist yet - UI will be updated when widgets are created
    sync_from_model_manager();
    LUCID_INFO("[SettingsPage] load_settings - sync_from_model_manager returned");
}

void SettingsPage::add_folder_row(const std::string& path) {
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(row, 8);
    gtk_widget_set_margin_end(row, 8);
    gtk_widget_set_margin_top(row, 4);
    gtk_widget_set_margin_bottom(row, 4);
    
    GtkWidget* icon = gtk_image_new_from_icon_name("folder");
    gtk_box_append(GTK_BOX(row), icon);
    
    GtkWidget* label = gtk_label_new(path.c_str());
    gtk_widget_set_hexpand(label, TRUE);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(row), label);
    
    GtkWidget* remove_btn = gtk_button_new_from_icon_name("window-close");
    gtk_widget_set_tooltip_text(remove_btn, "Remove folder");
    g_signal_connect(remove_btn, "clicked", G_CALLBACK(on_remove_folder_clicked), this);
    gtk_box_append(GTK_BOX(row), remove_btn);
    
    gtk_list_box_insert(GTK_LIST_BOX(m_folders_list), row, -1);
    m_watched_folders.push_back(path);
    
    // Notify main window
    if (m_folder_added_callback) {
        m_folder_added_callback(path);
    }
}

void SettingsPage::remove_folder_row(const std::string& path) {
    // Find and remove the row from the list
    GtkWidget* row_to_remove = nullptr;
    int num_rows = 0;
    
    // Count rows first (GTK4 compatible approach)
    GtkListBoxRow* row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(m_folders_list), num_rows);
    while (row != nullptr) {
        num_rows++;
        row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(m_folders_list), num_rows);
    }
    
    for (int i = 0; i < num_rows; i++) {
        GtkListBoxRow* list_row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(m_folders_list), i);
        GtkWidget* row_widget = GTK_WIDGET(list_row);
        GtkWidget* child = gtk_widget_get_first_child(row_widget);
        
        // Skip icon
        if (GTK_IS_IMAGE(child)) {
            child = gtk_widget_get_next_sibling(child);
        }
        
        if (child && GTK_IS_LABEL(child)) {
            const char* text = gtk_label_get_text(GTK_LABEL(child));
            if (text == path) {
                row_to_remove = row_widget;
                break;
            }
        }
    }
    
    if (row_to_remove) {
        gtk_list_box_remove(GTK_LIST_BOX(m_folders_list), row_to_remove);
        
        // Remove from vector
        m_watched_folders.erase(
            std::remove(m_watched_folders.begin(), m_watched_folders.end(), path),
            m_watched_folders.end()
        );
        
        // Save the updated list
        ModelManager::instance().set_watched_folders(m_watched_folders);
        
        // Notify main window
        if (m_folder_removed_callback) {
            m_folder_removed_callback(path);
        }
    }
}

void SettingsPage::set_on_folder_added(void (*callback)(const std::string&)) {
    m_folder_added_callback = callback;
}

void SettingsPage::set_on_folder_removed(void (*callback)(const std::string&)) {
    m_folder_removed_callback = callback;
}

void SettingsPage::set_on_thumbnail_size_changed(std::function<void(ThumbnailSize)> callback) {
    m_thumbnail_size_changed_callback = std::move(callback);
}

void SettingsPage::set_on_vision_extract_size_changed(std::function<void(VisionExtractSize)> callback) {
    m_vision_extract_size_changed_callback = std::move(callback);
}

void SettingsPage::set_parent_window(void* window) {
    m_parent_window = window;
}

// Callbacks
void SettingsPage::on_add_folder_clicked(GtkButton* /*button*/, gpointer /*data*/) {
    // Create file chooser dialog
    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        "Select Folder",
        nullptr,
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Open", GTK_RESPONSE_ACCEPT,
        nullptr
    );
    
    // Use gtk_popup for modal dialog in GTK4 - run synchronously via context
    gtk_widget_set_visible(dialog, TRUE);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    
    // Process events until dialog closes
    GtkEventController* controller = gtk_event_controller_key_new();
    gtk_widget_add_controller(dialog, controller);
    
    // Process events synchronously
    while (gtk_widget_get_visible(dialog)) {
        g_main_context_iteration(nullptr, TRUE);
    }
    
    // Check response - for now, just log that we got the click
    // File picking will be handled when we can properly implement GTK4 file dialog
    LUCID_INFO("[SettingsPage] Add folder dialog clicked - file picking will be added later");
    
    gtk_window_destroy(GTK_WINDOW(dialog));
}

void SettingsPage::on_gpu_toggled(GtkSwitch*, gpointer) {
    // GPU settings removed - cannot change while app is running
    LUCID_WARN("[SettingsPage] GPU toggle ignored - cannot change while app is running");
}

void SettingsPage::on_remove_folder_clicked(GtkButton* button, gpointer data) {
    auto* self = static_cast<SettingsPage*>(data);
    
    GtkWidget* row = gtk_widget_get_parent(GTK_WIDGET(button));
    GtkWidget* label = gtk_widget_get_first_child(row);
    
    // Skip icon
    if (GTK_IS_IMAGE(label)) {
        label = gtk_widget_get_next_sibling(label);
    }
    
    if (label && GTK_IS_LABEL(label)) {
        const char* text = gtk_label_get_text(GTK_LABEL(label));
        self->remove_folder_row(text);
    }
}

void SettingsPage::on_thumbnail_size_changed(GtkComboBox* combo, gpointer data) {
    auto* self = static_cast<SettingsPage*>(data);
    if (!self->m_initialized) {
        LUCID_DEBUG("on_thumbnail_size_changed: ignored (not initialized)");
        return;
    }
    
    int active = gtk_combo_box_get_active(combo);
    ThumbnailSize new_size = static_cast<ThumbnailSize>(active);
    
    if (self->m_thumbnail_size != new_size) {
        self->m_thumbnail_size = new_size;
        LUCID_DEBUG("on_thumbnail_size_changed: size=%d", active);
        
        // Save to ModelManager and notify callback
        try {
            auto& mm = ModelManager::instance();
            mm.set_thumbnail_size(static_cast<int>(new_size));
            
            if (self->m_thumbnail_size_changed_callback) {
                self->m_thumbnail_size_changed_callback(new_size);
            }
        } catch (const std::exception& e) {
            LUCID_ERROR("on_thumbnail_size_changed: Exception: %s", e.what());
        }
    }
}

void SettingsPage::on_parallel_jobs_changed(GtkSpinButton*, gpointer) {
    // Parallel jobs settings removed - cannot change while app is running
    LUCID_WARN("[SettingsPage] Parallel jobs changed ignored - cannot change while app is running");
}

void SettingsPage::on_embedding_context_changed(GtkSpinButton*, gpointer) {
    // Embedding context settings removed - cannot change while app is running
    LUCID_WARN("[SettingsPage] Embedding context changed ignored - cannot change while app is running");
}

void SettingsPage::on_whisper_context_changed(GtkSpinButton*, gpointer) {
    // Whisper context settings removed - cannot change while app is running
    LUCID_WARN("[SettingsPage] Whisper context changed ignored - cannot change while app is running");
}

void SettingsPage::on_vision_toggled(GtkSwitch*, gpointer) {
    // Vision settings removed - cannot change while app is running
    LUCID_WARN("[SettingsPage] Vision toggle ignored - cannot change while app is running");
}

void SettingsPage::on_vision_interval_changed(GtkSpinButton*, gpointer) {
    // Vision interval settings removed - cannot change while app is running
    LUCID_WARN("[SettingsPage] Vision interval changed ignored - cannot change while app is running");
}

void SettingsPage::on_vision_max_changed(GtkSpinButton*, gpointer) {
    // Vision max settings removed - cannot change while app is running
    LUCID_WARN("[SettingsPage] Vision max changed ignored - cannot change while app is running");
}

void SettingsPage::on_vision_extract_size_changed(GtkComboBox*, gpointer) {
    // Vision extract size settings removed - cannot change while app is running
    LUCID_WARN("[SettingsPage] Vision extract size changed ignored - cannot change while app is running");
}

void SettingsPage::on_perf_gpu_toggled(GtkSwitch* switch_widget, gpointer data) {
    auto* self = static_cast<SettingsPage*>(data);
    if (!self->m_initialized) {
        LUCID_DEBUG("on_perf_gpu_toggled: ignored (not initialized)");
        return;
    }
    
    bool enabled = gtk_switch_get_active(switch_widget);
    LUCID_INFO("[SettingsPage] GPU toggle: %s (restart required for effect)", enabled ? "ON" : "OFF");
    
    // Save to ModelManager
    try {
        auto& mm = ModelManager::instance();
        mm.set_use_gpu(enabled);
        self->m_use_gpu = enabled;
    } catch (const std::exception& e) {
        LUCID_ERROR("[SettingsPage] on_perf_gpu_toggled: Exception: %s", e.what());
    }
}

void SettingsPage::sync_from_model_manager() {
    // Sync all settings from ModelManager (loaded from config on startup)
    auto& mm = ModelManager::instance();
    
    // Model selections (hardcoded - cannot be changed at runtime)
    m_whisper_model = mm.get_selected_whisper_model();
    m_embedding_model = mm.get_selected_embedding_model();
    m_llm_model = mm.get_selected_llm_model();
    
    // General settings
    m_use_gpu = mm.get_use_gpu();
    m_max_parallel_jobs = mm.get_max_parallel_jobs();
    m_embedding_context_size = mm.get_embedding_context_size();
    m_whisper_context_size = mm.get_whisper_context_size();
    m_thumbnail_size = static_cast<ThumbnailSize>(mm.get_thumbnail_size());
    m_watched_folders = mm.get_watched_folders();
    
    // Vision scan settings (always enabled)
    m_vision_scan_interval = mm.get_vision_scan_interval();
    m_vision_max_per_video = mm.get_vision_max_per_video();
    m_vision_extract_size = mm.get_vision_extract_size();
    
    LUCID_INFO("[SettingsPage] sync_from_model_manager completed");
    LUCID_INFO("  - thumbnail_size=%d, watched_folders=%zu",
               static_cast<int>(m_thumbnail_size), m_watched_folders.size());
}

void SettingsPage::refresh_model_dropdowns() {
    // All model settings removed - no dropdowns to refresh
    LUCID_DEBUG("[SettingsPage] refresh_model_dropdowns: all model settings removed");
}

void SettingsPage::sync_to_widgets() {
    // Sync member variables to actual widgets to ensure UI reflects correct state
    LUCID_DEBUG("[SettingsPage] sync_to_widgets() called - no model widgets to sync");
}

void SettingsPage::set_active_tab(const std::string& tab_name) {
    if (!m_notebook) {
        LUCID_WARN("[SettingsPage] Notebook not initialized");
        return;
    }
    
    // Map tab name to page index
    std::unordered_map<std::string, int> tab_map = {
        {"general", 0},
        {"folders", 1},
        {"performance", 2}
    };
    
    auto it = tab_map.find(tab_name);
    if (it == tab_map.end()) {
        LUCID_WARN("[SettingsPage] Unknown tab name: %s", tab_name.c_str());
        return;
    }
    
    gtk_notebook_set_current_page(GTK_NOTEBOOK(m_notebook), it->second);
}

void SettingsPage::simulate_widget_interaction(const std::string& widget_name, const std::string& value) {
    LUCID_INFO("[SettingsPage] Simulating widget interaction: %s = %s", widget_name.c_str(), value.c_str());
    
    // Model settings have been removed from UI - ignore any simulation attempts
    // Vision scan is always enabled - only config settings remain
    if (widget_name == "whisper_combo" || widget_name == "embedding_combo" ||
        widget_name == "llm_combo" || widget_name == "gpu_switch" ||
        widget_name == "vision_switch") {
        LUCID_INFO("[SettingsPage] Settings removed from UI, ignoring simulation");
        return;
    }
    else if (widget_name == "thumbnail_size" && m_thumbnail_size_combo) {
        int size = std::stoi(value);
        gtk_combo_box_set_active(GTK_COMBO_BOX(m_thumbnail_size_combo), size);
        LUCID_INFO("[SettingsPage] Thumbnail size set to: %d", size);
    }
    else if (widget_name == "vision_interval") {
        // Vision scan is always enabled - just update the interval setting
        int interval = std::stoi(value);
        ModelManager::instance().set_vision_scan_interval(interval);
        LUCID_INFO("[SettingsPage] Vision scan interval set to: %d", interval);
    }
    else if (widget_name == "vision_max") {
        int max_per_video = std::stoi(value);
        ModelManager::instance().set_vision_max_per_video(max_per_video);
        LUCID_INFO("[SettingsPage] Vision max per video set to: %d", max_per_video);
    }
    else if (widget_name == "vision_extract_size") {
        int size = std::stoi(value);
        ModelManager::instance().set_vision_extract_size(static_cast<VisionExtractSize>(size));
        LUCID_INFO("[SettingsPage] Vision extract size set to: %d", size);
    }
    else {
        LUCID_WARN("[SettingsPage] Unknown widget: %s", widget_name.c_str());
    }
}

std::string SettingsPage::get_settings_state_json() const {
    Json::Value state;
    state["current_tab"] = get_active_tab_name();
    state["whisper_model"] = m_whisper_model;
    state["embedding_model"] = m_embedding_model;
    state["llm_model"] = m_llm_model;
    state["use_gpu"] = m_use_gpu;
    state["max_parallel_jobs"] = m_max_parallel_jobs;
    state["embedding_context_size"] = m_embedding_context_size;
    state["whisper_context_size"] = m_whisper_context_size;
    state["thumbnail_size"] = static_cast<int>(m_thumbnail_size);
    state["vision_scan_enabled"] = true;  // Always enabled
    // Get current values from ModelManager (may have been changed via API)
    state["vision_scan_interval"] = ModelManager::instance().get_vision_scan_interval();
    state["vision_max_per_video"] = ModelManager::instance().get_vision_max_per_video();
    state["vision_extract_size"] = static_cast<int>(ModelManager::instance().get_vision_extract_size());
    
    // Add folder list
    state["watched_folders"] = Json::arrayValue;
    for (const auto& folder : m_watched_folders) {
        state["watched_folders"].append(folder);
    }
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, state);
}

} // namespace lucid

#pragma GCC diagnostic pop