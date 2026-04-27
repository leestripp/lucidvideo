// page_widget.hpp - Unified pagination navigation widget
#pragma once

#include <gtk/gtk.h>
#include <functional>
#include <string>
#include <cstdint>

namespace lucid {

/**
 * PageWidget - Unified pagination controls for video grid
 * 
 * Provides navigation for:
 * - Jump to first page [<<]
 * - Previous page [<]
 * - Current page indicator ("Page 1 of 100")
 * - Next page [>]
 * - Jump to last page [>>]
 * - Separator
 * - Items per page label
 * - Page size selector dropdown [25, 50, 100, 200]
 */
class PageWidget {
public:
    PageWidget();
    ~PageWidget();
    
    // Initialize the widget (must be called before use)
    void initialize();
    
    // Configuration
    void set_total_items(int total);
    void set_page_size(int size);
    void set_current_page(int page);
    void set_page(int page) { set_current_page(page); }  // Alias for set_current_page
    
    // Getters
    int get_total_items() const { return m_total_items; }
    int get_page_size() const { return m_page_size; }
    int get_current_page() const { return m_current_page; }
    int get_total_pages() const { return m_total_pages; }
    
    // Callbacks
    using PageChangedCallback = std::function<void(int page, int page_size)>;
    using PageSizeChangedCallback = std::function<void(int page_size)>;
    
    void set_on_page_changed(PageChangedCallback callback);
    void set_on_page_size_changed(PageSizeChangedCallback callback);
    
    // Visibility
    void show();
    void hide();
    bool is_visible() const { return m_visible; }
    
    // Widget access
    GtkWidget* get_widget() const { return m_container; }
    
    // Get current state as JSON string (for API responses)
    std::string to_json_string() const;

private:
    void update_controls();
    void update_page_label();
    void calculate_total_pages();
    
    // Navigation helpers
    void go_to_first();
    void go_to_prev();
    void go_to_next();
    void go_to_last();
    void on_page_size_selected(int index);
    
    // GTK signal handlers (static callbacks)
    static void on_first_clicked(GtkButton* button, gpointer data);
    static void on_prev_clicked(GtkButton* button, gpointer data);
    static void on_next_clicked(GtkButton* button, gpointer data);
    static void on_last_clicked(GtkButton* button, gpointer data);
    static void on_page_size_changed(GtkDropDown* dropdown, gpointer data);
    
    GtkWidget* m_container = nullptr;
    GtkWidget* m_btn_first = nullptr;
    GtkWidget* m_btn_prev = nullptr;
    GtkWidget* m_btn_next = nullptr;
    GtkWidget* m_btn_last = nullptr;
    GtkWidget* m_page_label = nullptr;
    GtkWidget* m_page_size_dropdown = nullptr;
    
    int m_total_items = 0;
    int m_page_size = 50;
    int m_current_page = 0;
    int m_total_pages = 0;
    bool m_visible = true;
    
    PageChangedCallback m_page_changed_callback;
    PageSizeChangedCallback m_page_size_changed_callback;
    
    // Available page size options
    static constexpr int PAGE_SIZES[] = {25, 50, 100, 200};
    static constexpr int NUM_PAGE_SIZES = 4;
};

} // namespace lucid