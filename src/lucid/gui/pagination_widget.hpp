// pagination_widget.hpp - Pagination navigation widget
#pragma once

#include <gtk/gtk.h>
#include <functional>

namespace lucid {

/**
 * PaginationWidget - Reusable pagination controls
 * 
 * Provides navigation buttons for:
 * - Jump to first page
 * - Previous page
 * - Current page indicator (e.g., "Page 1 of 100")
 * - Next page
 * - Jump to last page
 * 
 * Note: Page size is set programmatically via set_page_size(),
 * not via a user-visible dropdown (to prevent GTK crashes).
 */
class PaginationWidget {
public:
    PaginationWidget();
    ~PaginationWidget();
    
    // Initialize with parent container
    void initialize(GtkWidget* parent);
    
    // Configuration
    void set_total_items(int total);
    void set_page_size(int size);
    void set_current_page(int page);
    
    // Get current state
    int get_current_page() const { return m_current_page; }
    int get_page_size() const { return m_page_size; }
    int get_total_pages() const { return m_total_pages; }
    int get_total_items() const { return m_total_items; }
    
    // Callbacks
    using PageChangedCallback = std::function<void(int page, int page_size)>;
    using PageSizeChangedCallback = std::function<void(int page_size)>;
    
    void set_on_page_changed(PageChangedCallback callback);
    void set_on_page_size_changed(PageSizeChangedCallback callback);
    
    // Control visibility
    void show();
    void hide();
    bool is_visible() const { return m_visible; }
    
    // Get the main container widget
    GtkWidget* get_widget() const { return m_container; }

private:
    void update_controls();
    void update_page_label();
    void calculate_total_pages();
    
    // Signal handlers
    static void on_first_clicked(GtkButton* button, gpointer data);
    static void on_prev_clicked(GtkButton* button, gpointer data);
    static void on_next_clicked(GtkButton* button, gpointer data);
    static void on_last_clicked(GtkButton* button, gpointer data);
    
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