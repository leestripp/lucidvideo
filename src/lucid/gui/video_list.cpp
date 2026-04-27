// video_list.cpp - Vertical video list with pagination
#include "video_list.hpp"
#include "pagination_widget.hpp"
#include "video_item.hpp"
#include "../logging.hpp"
#include "../settings/app_settings.hpp"

#include <gtk/gtk.h>
#include <algorithm>

namespace lucid {

VideoList::VideoList() = default;

VideoList::~VideoList() = default;

void VideoList::initialize(GtkWidget* /*parent*/) {
    // Main container (vertical box: list + pagination)
    m_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(m_container, TRUE);
    gtk_widget_set_vexpand(m_container, TRUE);
    
    // Create scrolled window
    m_scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(m_scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(m_scrolled_window, TRUE);
    gtk_widget_set_vexpand(m_scrolled_window, TRUE);
    gtk_box_append(GTK_BOX(m_container), m_scrolled_window);
    
    // Create ListBox for vertical list layout
    m_list_box = gtk_list_box_new();
    gtk_widget_set_margin_start(m_list_box, 12);
    gtk_widget_set_margin_end(m_list_box, 12);
    gtk_widget_set_margin_top(m_list_box, 8);
    gtk_widget_set_margin_bottom(m_list_box, 8);
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(m_list_box), GTK_SELECTION_SINGLE);
    
    // Connect row-activated signal for video selection
    g_signal_connect(m_list_box, "row-activated", G_CALLBACK(on_row_activated), this);
    
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(m_scrolled_window), m_list_box);
    
    // Create and add pagination widget at the bottom
    m_pagination = std::make_unique<PaginationWidget>();
    m_pagination->initialize(nullptr);
    gtk_box_append(GTK_BOX(m_container), m_pagination->get_widget());
    
    // Wire up pagination callbacks
    m_pagination->set_on_page_changed([this](int page, int page_size) {
        m_current_page = page;
        m_page_size = page_size;
        if (m_page_changed_callback) {
            m_page_changed_callback(page, page_size);
        }
    });
    
    m_pagination->set_on_page_size_changed([this](int page_size) {
        m_page_size = page_size;
        if (m_page_changed_callback) {
            m_page_changed_callback(0, page_size);
        }
    });
}

void VideoList::set_videos(const std::vector<VideoItem>& videos) {
    m_videos = videos;
    m_filtered_videos = videos;
    populate_list();
}

void VideoList::add_video(const VideoItem& video) {
    m_videos.push_back(video);
    m_filtered_videos = m_videos;
    populate_list();
}

void VideoList::remove_video(const std::string& video_id) {
    m_videos.erase(
        std::remove_if(m_videos.begin(), m_videos.end(),
                      [&video_id](const VideoItem& v) { return v.id == video_id; }),
        m_videos.end()
    );
    populate_list();
}

void VideoList::update_video_status(const std::string& video_id,
                                     bool is_transcribing,
                                     bool is_indexed,
                                     float progress) {
    auto it = std::find_if(m_videos.begin(), m_videos.end(),
                          [&video_id](const VideoItem& v) { return v.id == video_id; });
    if (it != m_videos.end()) {
        it->is_transcribing = is_transcribing;
        it->is_indexed = is_indexed;
        it->transcribe_progress = progress;
        populate_list();
    }
}

void VideoList::clear() {
    GtkListBoxRow* row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(m_list_box), 0);
    while (row) {
        gtk_list_box_remove(GTK_LIST_BOX(m_list_box), GTK_WIDGET(row));
        row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(m_list_box), 0);
    }
    m_videos.clear();
    m_filtered_videos.clear();
}

void VideoList::populate_list() {
    // Remove all existing rows
    GtkListBoxRow* row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(m_list_box), 0);
    while (row) {
        gtk_list_box_remove(GTK_LIST_BOX(m_list_box), GTK_WIDGET(row));
        row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(m_list_box), 0);
    }
    
    // Apply sort
    if (m_sort_by == SortBy::NAME) {
        std::sort(m_filtered_videos.begin(), m_filtered_videos.end(),
                 [this](const VideoItem& a, const VideoItem& b) {
                     return m_sort_ascending ? a.title < b.title : a.title > b.title;
                 });
    } else if (m_sort_by == SortBy::DURATION) {
        std::sort(m_filtered_videos.begin(), m_filtered_videos.end(),
                 [this](const VideoItem& a, const VideoItem& b) {
                     return m_sort_ascending ? a.duration_sec < b.duration_sec : 
                                               a.duration_sec > b.duration_sec;
                 });
    }
    
    // Add rows
    for (const auto& video : m_filtered_videos) {
        GtkWidget* row = create_list_row(video);
        gtk_list_box_append(GTK_LIST_BOX(m_list_box), row);
    }
}

GtkWidget* VideoList::create_list_row(const VideoItem& video) {
    // Create horizontal container row
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(row, 8);
    gtk_widget_set_margin_end(row, 8);
    gtk_widget_set_margin_top(row, 6);
    gtk_widget_set_margin_bottom(row, 6);
    gtk_widget_set_cursor_from_name(row, "pointer");
    
    // Thumbnail (left side)
    GtkWidget* thumb_container = gtk_aspect_frame_new(0.0, 0.5, 16.0/9.0, FALSE);
    gtk_widget_set_hexpand(thumb_container, FALSE);
    gtk_widget_set_valign(thumb_container, GTK_ALIGN_START);
    
    GtkWidget* thumb_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(thumb_box, TRUE);
    gtk_widget_set_vexpand(thumb_box, TRUE);
    
    // Use configurable thumbnail size from settings
    auto [thumb_w, thumb_h] = get_thumbnail_dimensions(m_thumbnail_size);
    
    // Load thumbnail if available, otherwise show generic icon
    if (!video.thumbnail_path.empty() && 
        g_file_test(video.thumbnail_path.c_str(), G_FILE_TEST_EXISTS)) {
        // Use GtkPicture for proper GTK4 scaling
        GError* error = nullptr;
        GdkTexture* texture = gdk_texture_new_from_file(
            g_file_new_for_path(video.thumbnail_path.c_str()), &error);
        if (texture) {
            GtkWidget* picture = gtk_picture_new();
            gtk_picture_set_paintable(GTK_PICTURE(picture), GDK_PAINTABLE(texture));
            gtk_widget_set_size_request(picture, thumb_w, thumb_h);
            gtk_widget_set_valign(picture, GTK_ALIGN_START);
            gtk_widget_add_css_class(picture, "video-thumbnail");
            gtk_box_append(GTK_BOX(thumb_box), picture);
            g_object_unref(texture);
        } else {
            // Fallback to icon
            if (error) {
                g_error_free(error);
            }
            GtkWidget* thumb_icon = gtk_image_new_from_icon_name("video-x-generic");
            gtk_image_set_icon_size(GTK_IMAGE(thumb_icon), GTK_ICON_SIZE_LARGE);
            gtk_widget_set_halign(thumb_icon, GTK_ALIGN_CENTER);
            gtk_widget_set_valign(thumb_icon, GTK_ALIGN_CENTER);
            gtk_widget_set_size_request(thumb_icon, thumb_w, thumb_h);
            gtk_box_append(GTK_BOX(thumb_box), thumb_icon);
        }
    } else {
        GtkWidget* thumb_icon = gtk_image_new_from_icon_name("video-x-generic");
        gtk_image_set_icon_size(GTK_IMAGE(thumb_icon), GTK_ICON_SIZE_LARGE);
        gtk_widget_set_halign(thumb_icon, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(thumb_icon, GTK_ALIGN_CENTER);
        gtk_widget_set_size_request(thumb_icon, thumb_w, thumb_h);
        gtk_box_append(GTK_BOX(thumb_box), thumb_icon);
    }
    
    gtk_aspect_frame_set_child(GTK_ASPECT_FRAME(thumb_container), thumb_box);
    gtk_box_append(GTK_BOX(row), thumb_container);
    
    // Video info (middle)
    GtkWidget* info_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(info_box, TRUE);
    gtk_widget_set_valign(info_box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(info_box, 8);
    
    // Title
    std::string display_title = video.title;
    if (display_title.length() > 60) {
        display_title = display_title.substr(0, 57) + "...";
    }
    GtkWidget* title = gtk_label_new(display_title.c_str());
    gtk_label_set_ellipsize(GTK_LABEL(title), PANGO_ELLIPSIZE_END);
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(info_box), title);
    
    // Path
    GtkWidget* path_label = gtk_label_new(video.path.c_str());
    gtk_label_set_ellipsize(GTK_LABEL(path_label), PANGO_ELLIPSIZE_START);
    gtk_widget_set_opacity(path_label, 0.6);
    gtk_widget_set_halign(path_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(info_box), path_label);
    
    gtk_box_append(GTK_BOX(row), info_box);
    
    // Duration (right side)
    char duration_str[16];
    int hours = video.duration_sec / 3600;
    int minutes = (video.duration_sec % 3600) / 60;
    int seconds = video.duration_sec % 60;
    if (hours > 0) {
        snprintf(duration_str, sizeof(duration_str), "%d:%02d:%02d", hours, minutes, seconds);
    } else {
        snprintf(duration_str, sizeof(duration_str), "%d:%02d", minutes, seconds);
    }
    GtkWidget* duration = gtk_label_new(duration_str);
    gtk_widget_set_opacity(duration, 0.6);
    gtk_widget_set_valign(duration, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(row), duration);
    
    // Store video ID
    g_object_set_data(G_OBJECT(row), "video-id", g_strdup(video.id.c_str()));
    g_object_set_data(G_OBJECT(row), "video-item", new VideoItem(video));
    
    return row;
}

void VideoList::on_row_activated(GtkListBox* box, GtkListBoxRow* row, gpointer data) {
    auto* self = static_cast<VideoList*>(data);
    if (!row) return;
    
    GtkWidget* widget = gtk_list_box_row_get_child(row);
    char* video_id = static_cast<char*>(g_object_get_data(G_OBJECT(widget), "video-id"));
    
    if (video_id && self->m_activated_callback) {
        auto it = std::find_if(self->m_videos.begin(), self->m_videos.end(),
                              [&video_id](const VideoItem& v) { return v.id == video_id; });
        if (it != self->m_videos.end()) {
            self->m_activated_callback(*it);
        }
    }
}

void VideoList::set_on_video_selected(void (*callback)(const VideoItem&)) {
    m_selected_callback = callback;
}

void VideoList::set_on_video_activated(VideoActivatedCallback callback) {
    m_activated_callback = std::move(callback);
}

void VideoList::set_on_page_changed(PageChangedCallback callback) {
    m_page_changed_callback = std::move(callback);
}

// ==================== Pagination Implementation ====================

void VideoList::set_page(int page) {
    m_current_page = page;
    if (m_pagination) {
        m_pagination->set_current_page(page);
    }
}

void VideoList::set_page_size(int size) {
    m_page_size = size;
    if (m_pagination) {
        m_pagination->set_page_size(size);
    }
}

void VideoList::set_total_count(int total) {
    m_total_count = total;
    if (m_pagination) {
        m_pagination->set_total_items(total);
    }
}

int VideoList::get_current_page() const {
    return m_current_page;
}

int VideoList::get_page_size() const {
    return m_page_size;
}

int VideoList::get_total_count() const {
    return m_total_count;
}

int VideoList::get_total_pages() const {
    if (m_total_count <= 0 || m_page_size <= 0) return 0;
    return (m_total_count + m_page_size - 1) / m_page_size;
}

void VideoList::sort_by(SortBy sort, bool ascending) {
    m_sort_by = sort;
    m_sort_ascending = ascending;
    populate_list();
}

void VideoList::filter_by(const std::string& search_term) {
    m_filter_term = search_term;
    m_filtered_videos.clear();
    
    std::string lower_term = search_term;
    std::transform(lower_term.begin(), lower_term.end(), lower_term.begin(), ::tolower);
    
    for (const auto& video : m_videos) {
        std::string lower_title = video.title;
        std::transform(lower_title.begin(), lower_title.end(), lower_title.begin(), ::tolower);
        
        if (lower_title.find(lower_term) != std::string::npos ||
            video.path.find(lower_term) != std::string::npos) {
            m_filtered_videos.push_back(video);
        }
    }
    
    populate_list();
}

void VideoList::set_loading(bool loading) {
    m_loading = loading;
    if (m_pagination) {
        // Could add loading indicator to pagination if needed
    }
}

void VideoList::set_thumbnail_size(ThumbnailSize size) {
    if (m_thumbnail_size != size) {
        m_thumbnail_size = size;
        LUCID_DEBUG("[VideoList] Thumbnail size changed to %d", static_cast<int>(size));
        // Repopulate to apply new sizes
        populate_list();
    }
}

} // namespace lucid