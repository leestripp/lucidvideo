// video_list.hpp - Vertical video list with pagination
#pragma once

#include <gtk/gtk.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>

#include "pagination_widget.hpp"
#include "video_item.hpp"
#include "../settings/app_settings.hpp"

namespace lucid {

/**
 * VideoList - Vertical list display with pagination
 * 
 * Displays videos in a scrollable vertical list layout with pagination controls.
 * Uses GtkListBox for proper list behavior with pagination widget at the bottom.
 */
class VideoList {
public:
    VideoList();
    ~VideoList();
    
    // Initialize with parent container
    void initialize(GtkWidget* parent);
    
    // ==================== Data Management ====================
    void set_videos(const std::vector<VideoItem>& videos);
    void add_video(const VideoItem& video);
    void remove_video(const std::string& video_id);
    void update_video_status(const std::string& video_id, 
                             bool is_transcribing = false,
                             bool is_indexed = false,
                             float progress = 0.0f);
    void clear();
    
    // ==================== Callbacks ====================
    using VideoActivatedCallback = std::function<void(const VideoItem&)>;
    using VideoSelectedCallback = void(*)(const VideoItem&);
    void set_on_video_selected(VideoSelectedCallback callback);
    void set_on_video_activated(VideoActivatedCallback callback);
    using PageChangedCallback = std::function<void(int page, int page_size)>;
    void set_on_page_changed(PageChangedCallback callback);
    
    // ==================== Pagination ====================
    void set_page(int page);
    void set_page_size(int size);
    void set_total_count(int total);
    int get_current_page() const;
    int get_page_size() const;
    int get_total_count() const;
    int get_total_pages() const;
    
    // ==================== Sorting & Filtering ====================
    enum class SortBy { NAME, DATE_ADDED, DURATION, SIZE };
    void sort_by(SortBy sort, bool ascending = true);
    void filter_by(const std::string& search_term);
    
    // ==================== Widget Access ====================
    GtkWidget* get_widget() const { return m_container; }
    
    // Loading state
    void set_loading(bool loading);
    bool is_loading() const { return m_loading; }
    
    // ==================== Thumbnail Size ====================
    // Set thumbnail size (reads from AppSettings)
    void set_thumbnail_size(ThumbnailSize size);
    ThumbnailSize get_thumbnail_size() const { return m_thumbnail_size; }

private:
    // Internal helpers
    void populate_list();
    GtkWidget* create_list_row(const VideoItem& video);
    
    // Click handler
    static void on_row_activated(GtkListBox* box, GtkListBoxRow* row, gpointer data);
    
    // Container widget (Box: [ScrolledWindow + List + Pagination])
    GtkWidget* m_container = nullptr;
    GtkWidget* m_scrolled_window = nullptr;
    GtkWidget* m_list_box = nullptr;
    std::unique_ptr<PaginationWidget> m_pagination;
    
    // Video data
    std::vector<VideoItem> m_videos;
    std::vector<VideoItem> m_filtered_videos;
    
    // Sorting/filtering state
    SortBy m_sort_by = SortBy::NAME;
    bool m_sort_ascending = true;
    std::string m_filter_term;
    
    // Pagination state
    int m_current_page = 0;
    int m_page_size = 50;
    int m_total_count = 0;
    
    // Callbacks
    VideoSelectedCallback m_selected_callback = nullptr;
    VideoActivatedCallback m_activated_callback;
    PageChangedCallback m_page_changed_callback;
    
    // Loading state
    bool m_loading = false;
    
    // Thumbnail display size (from AppSettings)
    ThumbnailSize m_thumbnail_size = ThumbnailSize::MEDIUM;
};

} // namespace lucid