// model_hub_page.hpp - Model Hub UI
#pragma once

#include <gtk/gtk.h>
#include "../model/model_manager.hpp"

#include <string>
#include <vector>
#include <unordered_map>

namespace lucid {

class ModelHubPage {
public:
    ModelHubPage();
    ~ModelHubPage();
    
    void initialize(GtkWidget* parent = nullptr);
    
    GtkWidget* get_widget() { return m_scrolled_window; }
    
    void refresh_models();
    
    // Internal callback for ModelManager status updates
    void on_model_status_changed(const ModelStatus& status);
    
private:
    GtkWidget* create_model_card(const std::string& model_id);
    void start_download(const std::string& model_id);
    
    // GTK callbacks
    static void on_download_clicked(GtkButton* button, gpointer data);
    static void on_cancel_clicked(GtkButton* button, gpointer data);
    static void on_delete_clicked(GtkButton* button, gpointer data);
    
    // Widgets
    GtkWidget* m_scrolled_window = nullptr;
    GtkWidget* m_model_list = nullptr;
    GtkWidget* m_embedding_list = nullptr;
    GtkWidget* m_llm_list = nullptr;
    GtkWidget* m_status_bar = nullptr;
    
    // Model cards indexed by model_id
    std::unordered_map<std::string, GtkWidget*> m_cards;
    std::vector<std::string> m_model_ids;
    
    // Progress bars indexed by model_id - for direct updates during download
    std::unordered_map<std::string, GtkWidget*> m_progress_bars;
    
    // Last received status
    ModelStatus m_last_status;
};

} // namespace lucid
