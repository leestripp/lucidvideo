// search_bar.cpp - Search input bar
#include "search_bar.hpp"
#include "../logging.hpp"

namespace lucid {

SearchBar::SearchBar() = default;

SearchBar::~SearchBar() {
    if (m_box) {
        g_clear_pointer(&m_box, gtk_widget_unparent);
    }
}

void SearchBar::initialize(GtkWidget* parent) {
    // Create horizontal box container
    m_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(m_box, 16);
    gtk_widget_set_margin_end(m_box, 16);
    gtk_widget_set_margin_top(m_box, 8);
    gtk_widget_set_margin_bottom(m_box, 8);
    if (parent) gtk_widget_set_parent(m_box, parent);
    
    // Create search entry
    m_entry = gtk_search_entry_new();
    gtk_widget_set_hexpand(m_entry, TRUE);
    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(m_entry), 
                                          "Search videos by content or description...");
    gtk_box_append(GTK_BOX(m_box), m_entry);
    
    // Connect signals
    g_signal_connect(m_entry, "activate", G_CALLBACK(on_activate), this);
    g_signal_connect(m_entry, "icon-press", G_CALLBACK(on_search_icon_clicked), this);
}

std::string SearchBar::get_text() const {
    const gchar* text = gtk_editable_get_text(GTK_EDITABLE(m_entry));
    return text ? text : "";
}

void SearchBar::set_placeholder(const std::string& text) {
    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(m_entry), text.c_str());
}

void SearchBar::set_on_search(void (*callback)(const std::string&)) {
    m_on_search = callback;
}

void SearchBar::set_enabled(bool enabled) {
    gtk_widget_set_sensitive(m_entry, enabled);
}

void SearchBar::clear() {
    gtk_editable_set_text(GTK_EDITABLE(m_entry), "");
}

void SearchBar::on_activate(GtkEntry* entry, gpointer data) {
    auto* self = static_cast<SearchBar*>(data);
    std::string query = gtk_editable_get_text(GTK_EDITABLE(entry));
    
    if (self->m_on_search && !query.empty()) {
        LUCID_DEBUG("Search triggered: %s", query.c_str());
        self->m_on_search(query);
    }
}

void SearchBar::on_search_icon_clicked(GtkEntry* entry, 
                                        GtkEntryIconPosition icon_pos,
                                        gpointer data) {
    if (icon_pos == GTK_ENTRY_ICON_SECONDARY) {
        // Clear button clicked
        gtk_editable_set_text(GTK_EDITABLE(entry), "");
    } else {
        // Search icon clicked
        auto* self = static_cast<SearchBar*>(data);
        std::string query = gtk_editable_get_text(GTK_EDITABLE(entry));
        
        if (self->m_on_search && !query.empty()) {
            LUCID_DEBUG("Search triggered via icon: %s", query.c_str());
            self->m_on_search(query);
        }
    }
}

} // namespace lucid