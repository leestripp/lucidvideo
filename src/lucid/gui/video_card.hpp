// video_card.hpp - Video thumbnail card widget
#pragma once

#include <gtk/gtk.h>
#include "video_item.hpp"

namespace lucid {

/**
 * VideoCard - A single video thumbnail card widget
 * 
 * Displays:
 * - 16:9 aspect ratio thumbnail (or placeholder icon)
 * - Status indicator (indexed/transcribing)
 * - Video title (truncated)
 * - Duration
 * 
 * Features:
 * - Click gesture for selection
 * - Stored video metadata
 */
class VideoCard {
public:
    // Create a new video card widget
    static GtkWidget* create(const VideoItem& video);
    
    // Update status indicators without recreating the card
    static void update_status(GtkWidget* card, bool is_indexed, 
                             bool is_transcribing, float progress);
    
    // Update thumbnail image
    static void set_thumbnail(GtkWidget* card, const char* thumbnail_path);
    
    // Get stored video ID
    static const char* get_video_id(GtkWidget* card);
    
private:
    // Internal helper to create thumbnail area
    static GtkWidget* create_thumbnail_area(const VideoItem& video);
    
    // Create status overlay icon
    static GtkWidget* create_status_icon(bool is_indexed, bool is_transcribing);
};

} // namespace lucid