// search_bar.hpp - Search input bar
#ifndef LUCID_SEARCH_BAR_HPP
#define LUCID_SEARCH_BAR_HPP

#include <gtk/gtk.h>
#include <string>

namespace lucid {

class SearchBar {
public:
    SearchBar();
    ~SearchBar();
    
    // Initialize with parent container
    void initialize(GtkWidget* parent);
    
    // Get the search widget
    GtkWidget* get_widget() const { return m_box; }
    
    // Get current search text
    std::string get_text() const;
    
    // Set placeholder text
    void set_placeholder(const std::string& text);
    
    // Set callback for search action
    void set_on_search(void (*callback)(const std::string&));
    
    // Enable/disable search input
    void set_enabled(bool enabled);
    
    // Clear the input
    void clear();

private:
    static void on_activate(GtkEntry* entry, gpointer data);
    static void on_search_icon_clicked(GtkEntry* entry, 
                                        GtkEntryIconPosition icon_pos,
                                        gpointer data);
    
    GtkWidget* m_box = nullptr;
    GtkWidget* m_entry = nullptr;
    void (*m_on_search)(const std::string&) = nullptr;
};

} // namespace lucid

#endif // LUCID_SEARCH_BAR_HPP