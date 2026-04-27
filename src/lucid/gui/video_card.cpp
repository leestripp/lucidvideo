// video_card.cpp - Video thumbnail card widget implementation
#include "video_card.hpp"
#include "video_list.hpp"
#include "../logging.hpp"

#include <gtk/gtk.h>
#include <cstdio>
#include <cstring>

namespace lucid {

GtkWidget* VideoCard::create(const VideoItem& video) {
    // Main card container
    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(card, 4);
    gtk_widget_set_margin_end(card, 4);
    gtk_widget_set_margin_top(card, 4);
    gtk_widget_set_margin_bottom(card, 4);
    gtk_widget_set_cursor_from_name(card, "pointer");
    
    // Thumbnail container with 16:9 aspect ratio
    GtkWidget* thumb_container = gtk_aspect_frame_new(0.5, 0.5, 16.0/9.0, FALSE);
    gtk_widget_set_size_request(thumb_container, 200, -1);
    
    // Thumbnail box
    GtkWidget* thumb_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(thumb_box, TRUE);
    gtk_widget_set_vexpand(thumb_box, TRUE);
    
    // Check if we have a thumbnail image
    if (!video.thumbnail_path.empty() && 
        g_file_test(video.thumbnail_path.c_str(), G_FILE_TEST_EXISTS)) {
        GtkWidget* image = gtk_image_new_from_file(video.thumbnail_path.c_str());
        gtk_widget_set_halign(image, GTK_ALIGN_FILL);
        gtk_widget_set_valign(image, GTK_ALIGN_FILL);
        gtk_box_append(GTK_BOX(thumb_box), image);
    } else {
        // Fallback to generic video icon
        GtkWidget* thumb_icon = gtk_image_new_from_icon_name("video-x-generic");
        gtk_image_set_icon_size(GTK_IMAGE(thumb_icon), GTK_ICON_SIZE_LARGE);
        gtk_widget_set_halign(thumb_icon, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(thumb_icon, GTK_ALIGN_CENTER);
        gtk_box_append(GTK_BOX(thumb_box), thumb_icon);
    }
    
    // Status overlay
    GtkWidget* status_icon = create_status_icon(video.is_indexed, video.is_transcribing);
    if (status_icon) {
        gtk_widget_set_halign(status_icon, GTK_ALIGN_END);
        gtk_widget_set_valign(status_icon, GTK_ALIGN_START);
        gtk_widget_set_margin_start(status_icon, 4);
        gtk_widget_set_margin_top(status_icon, 4);
        gtk_box_append(GTK_BOX(thumb_box), status_icon);
    }
    
    gtk_aspect_frame_set_child(GTK_ASPECT_FRAME(thumb_container), thumb_box);
    gtk_box_append(GTK_BOX(card), thumb_container);
    
    // Title label
    std::string display_title = video.title;
    if (display_title.length() > 30) {
        display_title = display_title.substr(0, 27) + "...";
    }
    GtkWidget* title = gtk_label_new(display_title.c_str());
    gtk_label_set_ellipsize(GTK_LABEL(title), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(title), 20);
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(card), title);
    
    // Duration label
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
    gtk_widget_set_halign(duration, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(card), duration);
    
    // Store video ID in widget
    g_object_set_data(G_OBJECT(card), "video-id", g_strdup(video.id.c_str()));
    
    // Store thumbnail path for potential updates
    if (!video.thumbnail_path.empty()) {
        g_object_set_data(G_OBJECT(card), "thumbnail-path", 
                         g_strdup(video.thumbnail_path.c_str()));
    }
    
    return card;
}

void VideoCard::update_status(GtkWidget* card, bool is_indexed, 
                              bool is_transcribing, float /*progress*/) {
    // Find the status icon in thumbnail area and update it
    GtkWidget* thumb_container = gtk_widget_get_first_child(card);
    if (!thumb_container) return;
    
    // The status icon should be the last child of the aspect frame's child (thumb_box)
    // For simplicity, we'll just rebuild the card if needed
    // TODO: Optimize by only updating the icon
    
    (void)is_indexed;
    (void)is_transcribing;
}

void VideoCard::set_thumbnail(GtkWidget* card, const char* thumbnail_path) {
    if (!thumbnail_path || !g_file_test(thumbnail_path, G_FILE_TEST_EXISTS)) {
        return;
    }
    
    // Find thumbnail image in card and update it
    GtkWidget* thumb_container = gtk_widget_get_first_child(card);
    if (!thumb_container || !GTK_IS_ASPECT_FRAME(thumb_container)) return;
    
    GtkWidget* thumb_box = gtk_aspect_frame_get_child(GTK_ASPECT_FRAME(thumb_container));
    if (!thumb_box || !GTK_IS_BOX(thumb_box)) return;
    
    // Find and update the image
    GtkWidget* child = gtk_widget_get_first_child(thumb_box);
    while (child) {
        if (GTK_IS_IMAGE(child)) {
            gtk_image_set_from_file(GTK_IMAGE(child), thumbnail_path);
            break;
        }
        child = gtk_widget_get_next_sibling(child);
    }
}

const char* VideoCard::get_video_id(GtkWidget* card) {
    return static_cast<const char*>(g_object_get_data(G_OBJECT(card), "video-id"));
}

GtkWidget* VideoCard::create_status_icon(bool is_indexed, bool is_transcribing) {
    const char* status_icon = nullptr;
    
    if (is_indexed) {
        status_icon = "emblem-ok-symbolic";
    } else if (is_transcribing) {
        status_icon = "content-loading-symbolic";
    }
    
    if (!status_icon) {
        return nullptr;
    }
    
    return gtk_image_new_from_icon_name(status_icon);
}

} // namespace lucid