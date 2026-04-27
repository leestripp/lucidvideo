// search_results_panel.hpp - Search results display panel
#pragma once

#include <gtk/gtk.h>
#include <string>
#include <vector>
#include "../model/database.hpp"

namespace lucid {

class SearchResultsPanel {
public:
    SearchResultsPanel();
    ~SearchResultsPanel();
    
    // Initialize with parent container
    void initialize(GtkWidget* parent);
    
    // Set search results to display
    void set_results(const std::vector<SearchResult>& results);
    
    // Clear current results
    void clear();
    
    // Set callback for result selection
    void set_on_result_selected(void (*callback)(const SearchResult&));
    
    // Update current query for highlighting
    void set_current_query(const std::string& query);
    
    GtkWidget* get_widget() const { return m_box; }

private:
    static void on_result_clicked(GtkButton* button, gpointer data);
    
    void populate_results();
    GtkWidget* create_result_item(const SearchResult& result, int index);
    
    GtkWidget* m_box = nullptr;
    GtkWidget* m_list_box = nullptr;
    std::vector<SearchResult> m_results;
    std::string m_query;
    void (*m_callback)(const SearchResult&) = nullptr;
};

} // namespace lucid