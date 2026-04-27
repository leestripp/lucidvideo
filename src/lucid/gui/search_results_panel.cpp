// search_results_panel.cpp - Search results display panel
#include "search_results_panel.hpp"
#include "../search/semantic_search.hpp"
#include "../logging.hpp"

namespace lucid {

SearchResultsPanel::SearchResultsPanel() = default;

SearchResultsPanel::~SearchResultsPanel() {
    if (m_box) {
        g_clear_pointer(&m_box, gtk_widget_unparent);
    }
}

void SearchResultsPanel::initialize(GtkWidget* parent) {
    // Create main container
    m_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_vexpand(m_box, TRUE);
    gtk_widget_set_hexpand(m_box, TRUE);
    if (parent) gtk_widget_set_parent(m_box, parent);
    
    // Header label
    GtkWidget* header = gtk_label_new("Search Results");
    gtk_widget_set_halign(header, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(m_box), header);
    
    // List box for results (instead of scrolled window for simplicity)
    m_list_box = gtk_list_box_new();
    gtk_widget_set_vexpand(m_list_box, TRUE);
    gtk_widget_set_hexpand(m_list_box, TRUE);
    gtk_box_append(GTK_BOX(m_box), m_list_box);
    
    // Empty state
    GtkWidget* empty_label = gtk_label_new("No results yet.\nEnter a search query to find videos.");
    gtk_list_box_append(GTK_LIST_BOX(m_list_box), empty_label);
}

void SearchResultsPanel::set_results(const std::vector<SearchResult>& results) {
    m_results = results;
    populate_results();
}

void SearchResultsPanel::clear() {
    m_results.clear();
    m_query.clear();
    populate_results();
}

void SearchResultsPanel::set_current_query(const std::string& query) {
    m_query = query;
}

void SearchResultsPanel::set_on_result_selected(void (*callback)(const SearchResult&)) {
    m_callback = callback;
}

void SearchResultsPanel::populate_results() {
    // Remove all children from list box
    GtkListBoxRow* row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(m_list_box), 0);
    while (row) {
        gtk_list_box_remove(GTK_LIST_BOX(m_list_box), GTK_WIDGET(row));
        row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(m_list_box), 0);
    }
    
    if (m_results.empty()) {
        // Show empty state
        const char* msg = m_query.empty() 
            ? "Enter a search query to find videos." 
            : "No matching results found.";
        GtkWidget* empty_label = gtk_label_new(msg);
        gtk_list_box_append(GTK_LIST_BOX(m_list_box), empty_label);
        return;
    }
    
    // Add result items
    for (size_t i = 0; i < m_results.size(); ++i) {
        GtkWidget* item = create_result_item(m_results[i], (int)i);
        gtk_list_box_append(GTK_LIST_BOX(m_list_box), item);
    }
}

void SearchResultsPanel::on_result_clicked(GtkButton* button, gpointer data) {
    int index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "result-index"));
    auto* self = static_cast<SearchResultsPanel*>(data);
    
    if (index >= 0 && index < static_cast<int>(self->m_results.size())) {
        if (self->m_callback) {
            self->m_callback(self->m_results[index]);
        }
    }
}

GtkWidget* SearchResultsPanel::create_result_item(const SearchResult& result, int index) {
    GtkWidget* card = gtk_frame_new(nullptr);
    gtk_widget_set_margin_start(card, 8);
    gtk_widget_set_margin_end(card, 8);
    gtk_widget_set_margin_top(card, 4);
    gtk_widget_set_margin_bottom(card, 4);
    
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);
    gtk_frame_set_child(GTK_FRAME(card), box);
    
    // Thumbnail placeholder
    GtkWidget* thumb = gtk_picture_new();
    gtk_widget_set_size_request(thumb, 80, 45);
    gtk_box_append(GTK_BOX(box), thumb);
    
    // Info
    GtkWidget* info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(info, TRUE);
    gtk_box_append(GTK_BOX(box), info);
    
    // Title
    const char* title = result.title.empty() ? result.video_id.c_str() : result.title.c_str();
    GtkWidget* title_label = gtk_label_new(title);
    gtk_label_set_ellipsize(GTK_LABEL(title_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(title_label), 30);
    gtk_widget_set_halign(title_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(info), title_label);
    
    // Timestamp
    char time_str[32];
    snprintf(time_str, sizeof(time_str), "%02d:%02d.%03d",
             (result.timestamp_ms / 60000) % 60,
             (result.timestamp_ms / 1000) % 60,
             result.timestamp_ms % 1000);
    
    GtkWidget* time_label = gtk_label_new(time_str);
    gtk_widget_set_halign(time_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(info), time_label);
    
    // Score badge
    char score_text[16];
    snprintf(score_text, sizeof(score_text), "%.0f%%", result.relevance_score * 100);
    GtkWidget* score_label = gtk_label_new(score_text);
    gtk_box_append(GTK_BOX(box), score_label);
    
    // Snippet
    if (!result.snippet.empty()) {
        GtkWidget* snippet_label = gtk_label_new(result.snippet.c_str());
        gtk_label_set_ellipsize(GTK_LABEL(snippet_label), PANGO_ELLIPSIZE_END);
        gtk_label_set_max_width_chars(GTK_LABEL(snippet_label), 40);
        gtk_widget_set_halign(snippet_label, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(info), snippet_label);
    }
    
    // Make card clickable via button
    GtkWidget* button = gtk_button_new();
    gtk_widget_set_halign(button, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(button, TRUE);
    gtk_button_set_child(GTK_BUTTON(button), card);
    g_object_set_data(G_OBJECT(button), "result-index", GINT_TO_POINTER(index));
    g_signal_connect(button, "clicked", G_CALLBACK(on_result_clicked), this);
    
    return button;
}

} // namespace lucid