// model_hub_page.cpp - Model download and management page
#include "model_hub_page.hpp"
#include "../model/model_manager.hpp"
#include "../logging.hpp"

#include <gtk/gtk.h>
#include <cstdio>

namespace lucid {

// ============================================================================
// ModelHubPage Implementation
// ============================================================================

ModelHubPage::ModelHubPage() = default;
ModelHubPage::~ModelHubPage() = default;

void ModelHubPage::initialize(GtkWidget* /*parent*/) {
    LUCID_DEBUG("[ModelHubPage] initialize() starting");
    
    // Main scrolled window
    m_scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(m_scrolled_window),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_margin_start(m_scrolled_window, 12);
    gtk_widget_set_margin_end(m_scrolled_window, 12);
    gtk_widget_set_margin_top(m_scrolled_window, 12);
    gtk_widget_set_margin_bottom(m_scrolled_window, 12);
    
    // Main container
    GtkWidget* main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(m_scrolled_window), main_box);
    
    // Header
    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* title = gtk_label_new("Model Hub");
    gtk_label_set_markup(GTK_LABEL(title), "<span size='large' weight='bold'>Model Hub</span>");
    gtk_widget_set_hexpand(title, TRUE);
    gtk_box_append(GTK_BOX(header), title);
    
    GtkWidget* refresh_btn = gtk_button_new_from_icon_name("view-refresh");
    gtk_widget_set_tooltip_text(refresh_btn, "Refresh");
    g_signal_connect(refresh_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer p) {
        static_cast<ModelHubPage*>(p)->refresh_models();
    }), this);
    gtk_box_append(GTK_BOX(header), refresh_btn);
    gtk_box_append(GTK_BOX(main_box), header);
    
    // Status bar
    m_status_bar = gtk_label_new("Loading...");
    gtk_widget_set_halign(m_status_bar, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(main_box), m_status_bar);
    
    // Separator
    gtk_box_append(GTK_BOX(main_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    
    // Whisper section header
    GtkWidget* whisper_header = gtk_label_new("Speech Recognition (Whisper)");
    gtk_label_set_markup(GTK_LABEL(whisper_header), "<span weight='bold'>Speech Recognition (Whisper)</span>");
    gtk_widget_set_halign(whisper_header, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(main_box), whisper_header);
    
    // Whisper models list
    m_model_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_vexpand(m_model_list, TRUE);
    gtk_box_append(GTK_BOX(main_box), m_model_list);
    
    // Separator
    gtk_box_append(GTK_BOX(main_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    
    // Embedding section header
    GtkWidget* embed_header = gtk_label_new("Text Embeddings");
    gtk_label_set_markup(GTK_LABEL(embed_header), "<span weight='bold'>Text Embeddings</span>");
    gtk_widget_set_halign(embed_header, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(main_box), embed_header);
    
    // Embedding models list
    m_embedding_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_vexpand(m_embedding_list, TRUE);
    gtk_box_append(GTK_BOX(main_box), m_embedding_list);
    
    // Separator
    gtk_box_append(GTK_BOX(main_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    
    // LLM section header
    GtkWidget* llm_header = gtk_label_new("LLM + Vision Models");
    gtk_label_set_markup(GTK_LABEL(llm_header), "<span weight='bold'>LLM + Vision Models</span>");
    gtk_widget_set_halign(llm_header, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(main_box), llm_header);
    
    // LLM models list
    m_llm_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_vexpand(m_llm_list, TRUE);
    gtk_box_append(GTK_BOX(main_box), m_llm_list);
    
    // Register callback with ModelManager
    // IMPORTANT: Schedule UI updates on main thread using g_idle_add
    // because GTK is not thread-safe - callbacks may come from download thread
    auto self = this;
    ModelManager::instance().set_on_status_change(
        [self](const ModelStatus& status) {
            LUCID_DEBUG("[ModelHubPage] Status callback from thread: model=%s, state=%d, progress=%.1f%%",
                       status.model_id.empty() ? "(empty)" : status.model_id.c_str(), 
                       (int)status.state, status.progress * 100);
            self->m_last_status = status;
            
            // Schedule UI update on main thread
            g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                +[](gpointer data) -> gboolean {
                    auto* self = static_cast<ModelHubPage*>(data);
                    const auto& status = self->m_last_status;
                    
                    // Only update progress bar if we have a model_id and progress
                    if (!status.model_id.empty()) {
                        auto it = self->m_progress_bars.find(status.model_id);
                        if (it != self->m_progress_bars.end() && status.state == ModelState::DOWNLOADING) {
                            // Direct update of progress bar
                            GtkWidget* progress_bar = it->second;
                            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), status.progress);
                            char progress_text[64];
                            snprintf(progress_text, sizeof(progress_text), "Downloading... %.0f%%", status.progress * 100);
                            gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), progress_text);
                            LUCID_DEBUG("[ModelHubPage] Updated progress bar: %.0f%%", status.progress * 100);
                            return G_SOURCE_REMOVE;
                        }
                    }
                    
                    // For other state changes (DOWNLOADED, ERROR), refresh the whole view
                    self->refresh_models();
                    return G_SOURCE_REMOVE;
                }, self, nullptr);
        }
    );
    
    LUCID_DEBUG("[ModelHubPage] initialize() calling refresh_models()");
    refresh_models();
    LUCID_DEBUG("[ModelHubPage] initialize() complete");
}

void ModelHubPage::refresh_models() {
    LUCID_DEBUG("[ModelHubPage] refresh_models()");
    
    if (!m_model_list || !m_status_bar) {
        LUCID_ERROR("[ModelHubPage] Widgets not initialized!");
        return;
    }
    
    // Clear lists
    auto clear_box = [](GtkWidget* box) {
        GtkWidget* child = gtk_widget_get_first_child(box);
        while (child) {
            GtkWidget* next = gtk_widget_get_next_sibling(child);
            gtk_widget_unparent(child);
            child = next;
        }
    };
    clear_box(m_model_list);
    clear_box(m_embedding_list);
    clear_box(m_llm_list);
    m_cards.clear();
    m_model_ids.clear();
    m_progress_bars.clear();  // Clear progress bar references too
    
    const auto& catalog = ModelManager::instance().get_catalog();
    
    int downloaded = 0;
    int total = (int)catalog.size();
    for (const auto& m : catalog) {
        LUCID_DEBUG("[ModelHubPage] Model: %s, state=%d", m.id.c_str(), (int)m.state);
        if (m.state == ModelState::DOWNLOADED) downloaded++;
    }
    
    char buf[128];
    snprintf(buf, sizeof(buf), "%d of %d models downloaded", downloaded, total);
    gtk_label_set_text(GTK_LABEL(m_status_bar), buf);
    
    // Create cards
    for (const auto& m : catalog) {
        m_model_ids.push_back(m.id);
        GtkWidget* card = create_model_card(m.id);
        if (card) {
            m_cards[m.id] = card;
            GtkWidget* target;
            if (m.type == ModelType::WHISPER) {
                target = m_model_list;
            } else if (m.type == ModelType::EMBEDDING) {
                target = m_embedding_list;
            } else {
                target = m_llm_list;  // LLM models
            }
            gtk_box_append(GTK_BOX(target), card);
        }
    }
    
    LUCID_DEBUG("[ModelHubPage] refresh_models() done, %zu cards", m_cards.size());
}

GtkWidget* ModelHubPage::create_model_card(const std::string& model_id) {
    LUCID_DEBUG("[ModelHubPage] create_model_card(%s)", model_id.c_str());
    
    const ModelDescriptor* model = ModelManager::instance().get_model(model_id);
    if (!model) {
        LUCID_ERROR("[ModelHubPage] Model not found: %s", model_id.c_str());
        return nullptr;
    }
    
    // Card frame
    GtkWidget* card = gtk_frame_new(nullptr);
    gtk_widget_set_margin_start(card, 4);
    gtk_widget_set_margin_end(card, 4);
    gtk_widget_set_margin_top(card, 4);
    gtk_widget_set_margin_bottom(card, 4);
    
    // Main content box - VERTICAL layout
    GtkWidget* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_frame_set_child(GTK_FRAME(card), content);
    gtk_widget_set_size_request(content, -1, 100);
    
    // Top row: Icon + Info
    GtkWidget* top_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(content), top_row);
    
    // Icon based on model type
    const char* icon;
    if (model->type == ModelType::WHISPER) {
        icon = "audio-x-generic";
    } else if (model->type == ModelType::EMBEDDING) {
        icon = "text-x-generic";
    } else {
        icon = "chat-message-new";  // LLM model
    }
    GtkWidget* icon_img = gtk_image_new_from_icon_name(icon);
    gtk_image_set_icon_size(GTK_IMAGE(icon_img), GTK_ICON_SIZE_LARGE);
    gtk_widget_set_size_request(icon_img, 48, 48);
    gtk_box_append(GTK_BOX(top_row), icon_img);
    
    // Info section (expandable)
    GtkWidget* info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(info, TRUE);
    
    // Name
    char name_buf[256];
    snprintf(name_buf, sizeof(name_buf), "<b>%s</b>", model->name.c_str());
    GtkWidget* name_label = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(name_label), name_buf);
    gtk_widget_set_halign(name_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(info), name_label);
    
    // Description
    GtkWidget* desc_label = gtk_label_new(model->description.c_str());
    gtk_widget_set_halign(desc_label, GTK_ALIGN_START);
    gtk_label_set_ellipsize(GTK_LABEL(desc_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(desc_label), 30);
    gtk_box_append(GTK_BOX(info), desc_label);
    
    // Size
    char size_buf[64];
    if (model->size_bytes >= 1024*1024*1024) {
        snprintf(size_buf, sizeof(size_buf), "%s | %.1f GB", model->param_count.c_str(), 
                 model->size_bytes / (1024.0*1024.0*1024.0));
    } else {
        snprintf(size_buf, sizeof(size_buf), "%s | %.0f MB", model->param_count.c_str(),
                 model->size_bytes / (1024.0*1024.0));
    }
    GtkWidget* size_label = gtk_label_new(size_buf);
    gtk_widget_set_halign(size_label, GTK_ALIGN_START);
    gtk_widget_set_opacity(size_label, 0.6);
    gtk_box_append(GTK_BOX(info), size_label);
    
    gtk_box_append(GTK_BOX(top_row), info);
    
    // Store model-id for button callbacks
    g_object_set_data(G_OBJECT(card), "model-id", g_strdup(model_id.c_str()));
    
    // ========================================================================
    // PROGRESS BAR
    // ========================================================================
    GtkWidget* progress_bar = gtk_progress_bar_new();
    gtk_widget_set_hexpand(progress_bar, TRUE);
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(progress_bar), TRUE);
    
    // Set progress based on state
    if (model->state == ModelState::DOWNLOADING) {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), model->download_progress);
        char progress_text[64];
        snprintf(progress_text, sizeof(progress_text), "Downloading... %.0f%%", model->download_progress * 100);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), progress_text);
    } else if (model->state == ModelState::DOWNLOADED) {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 1.0);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), "Downloaded");
    } else if (model->state == ModelState::ERROR) {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), "Error");
    } else {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), "Not Downloaded");
    }
    
    gtk_box_append(GTK_BOX(content), progress_bar);
    
    // Store progress bar for direct updates during download
    m_progress_bars[model_id] = progress_bar;
    
    // ========================================================================
    // ACTION BUTTONS - Always show all 3 buttons
    // ========================================================================
    GtkWidget* button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign(button_box, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(content), button_box);
    
    // DOWNLOAD button
    GtkWidget* download_btn = gtk_button_new_with_label("Download");
    gtk_button_set_icon_name(GTK_BUTTON(download_btn), "arrow-down");
    gtk_widget_add_css_class(download_btn, "suggested-action");
    g_signal_connect(download_btn, "clicked", G_CALLBACK(on_download_clicked), this);
    gtk_widget_set_size_request(download_btn, 90, -1);
    gtk_box_append(GTK_BOX(button_box), download_btn);
    gtk_widget_set_visible(download_btn, 
        model->state == ModelState::NOT_DOWNLOADED || model->state == ModelState::ERROR);
    
    // STOP button  
    GtkWidget* stop_btn = gtk_button_new_with_label("Stop");
    gtk_button_set_icon_name(GTK_BUTTON(stop_btn), "process-stop");
    g_signal_connect(stop_btn, "clicked", G_CALLBACK(on_cancel_clicked), this);
    gtk_widget_set_size_request(stop_btn, 70, -1);
    gtk_box_append(GTK_BOX(button_box), stop_btn);
    gtk_widget_set_visible(stop_btn, model->state == ModelState::DOWNLOADING);
    
    // DELETE button
    GtkWidget* delete_btn = gtk_button_new_with_label("Delete");
    gtk_button_set_icon_name(GTK_BUTTON(delete_btn), "user-trash-symbolic");
    g_signal_connect(delete_btn, "clicked", G_CALLBACK(on_delete_clicked), this);
    gtk_widget_set_size_request(delete_btn, 80, -1);
    gtk_box_append(GTK_BOX(button_box), delete_btn);
    gtk_widget_set_visible(delete_btn, model->state == ModelState::DOWNLOADED);
    
    // Status indicator label
    GtkWidget* status_label = gtk_label_new(nullptr);
    gtk_widget_set_hexpand(status_label, TRUE);
    gtk_widget_set_halign(status_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(button_box), status_label);
    
    const char* status_text = "Ready";
    if (model->state == ModelState::DOWNLOADING) status_text = "Downloading...";
    else if (model->state == ModelState::DOWNLOADED) status_text = "Ready";
    else if (model->state == ModelState::ERROR) status_text = "Error";
    else status_text = "Not Downloaded";
    
    gtk_label_set_text(GTK_LABEL(status_label), status_text);
    
    LUCID_DEBUG("[ModelHubPage] Card created for %s with state=%d", model_id.c_str(), (int)model->state);
    return card;
}

void ModelHubPage::on_model_status_changed(const ModelStatus& status) {
    LUCID_DEBUG("[ModelHubPage] on_model_status_changed: %s, state=%d, progress=%.1f%%",
               status.model_id.c_str(), (int)status.state, status.progress * 100);
    // Note: refresh_models() is now scheduled via g_idle_add in the callback
}

// ============================================================================
// Button Callbacks
// ============================================================================

void ModelHubPage::on_download_clicked(GtkButton* button, gpointer data) {
    auto* self = static_cast<ModelHubPage*>(data);
    GtkWidget* card = gtk_widget_get_parent(GTK_WIDGET(button));
    while (card && !GTK_IS_FRAME(card)) {
        card = gtk_widget_get_parent(card);
    }
    char* model_id = static_cast<char*>(g_object_get_data(G_OBJECT(card), "model-id"));
    
    if (model_id) {
        LUCID_INFO("[ModelHubPage] Download clicked: %s", model_id);
        self->start_download(model_id);
    }
}

void ModelHubPage::on_cancel_clicked(GtkButton* button, gpointer data) {
    auto* self = static_cast<ModelHubPage*>(data);
    GtkWidget* card = gtk_widget_get_parent(GTK_WIDGET(button));
    while (card && !GTK_IS_FRAME(card)) {
        card = gtk_widget_get_parent(card);
    }
    char* model_id = static_cast<char*>(g_object_get_data(G_OBJECT(card), "model-id"));
    
    if (model_id) {
        LUCID_INFO("[ModelHubPage] Cancel clicked: %s", model_id);
        ModelManager::instance().cancel_download(model_id);
        self->refresh_models();
    }
}

void ModelHubPage::on_delete_clicked(GtkButton* button, gpointer data) {
    auto* self = static_cast<ModelHubPage*>(data);
    GtkWidget* card = gtk_widget_get_parent(GTK_WIDGET(button));
    while (card && !GTK_IS_FRAME(card)) {
        card = gtk_widget_get_parent(card);
    }
    char* model_id = static_cast<char*>(g_object_get_data(G_OBJECT(card), "model-id"));
    
    if (model_id) {
        LUCID_INFO("[ModelHubPage] Delete clicked: %s", model_id);
        ModelManager::instance().delete_model(model_id);
        self->refresh_models();
    }
}

void ModelHubPage::start_download(const std::string& model_id) {
    LUCID_DEBUG("[ModelHubPage] start_download(%s)", model_id.c_str());
    
    if (ModelManager::instance().start_download(model_id)) {
        LUCID_INFO("[ModelHubPage] Download started: %s", model_id.c_str());
        // Note: Don't call refresh_models() here - the callback will handle it
        // This prevents blocking the UI thread
    } else {
        LUCID_WARN("[ModelHubPage] Failed to start download: %s", model_id.c_str());
    }
}

} // namespace lucid
