// timeline_widget.cpp - Lucid Timeline Widget Implementation
#include "timeline_widget.hpp"
#include "../logging.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace lucid {

TimelineWidget::TimelineWidget() {
    set_hexpand(true);
    set_vexpand(false);
    
    // Setup event controllers for GTK4-style input handling
    setup_event_controllers();
    
    add_css_class("timeline-widget");
}

TimelineWidget::~TimelineWidget() = default;

void TimelineWidget::setup_event_controllers() {
    // Motion controller for hover and drag
    m_motion_controller = Gtk::EventControllerMotion::create();
    m_motion_controller->signal_motion().connect(sigc::mem_fun(*this, &TimelineWidget::on_motion));
    m_motion_controller->signal_leave().connect(sigc::mem_fun(*this, &TimelineWidget::on_leave));
    add_controller(m_motion_controller);
    
    // Click controller for seeking and marker clicks
    m_click_controller = Gtk::GestureClick::create();
    m_click_controller->signal_pressed().connect(sigc::mem_fun(*this, &TimelineWidget::on_click_pressed));
    m_click_controller->signal_released().connect(sigc::mem_fun(*this, &TimelineWidget::on_click_released));
    add_controller(m_click_controller);
}

void TimelineWidget::set_duration(int64_t duration_ms) {
    m_duration_ms = duration_ms;
    queue_draw();
}

void TimelineWidget::set_position(int64_t position_ms) {
    m_position_ms = position_ms;
    queue_draw();
}

void TimelineWidget::set_thumbnails(const std::vector<TimelineThumbnail>& thumbs) {
    m_thumbnails = thumbs;
    queue_draw();
}

void TimelineWidget::set_markers(const std::vector<TimelineMarker>& markers) {
    m_markers = markers;
    queue_draw();
}

void TimelineWidget::clear() {
    m_duration_ms = 0;
    m_position_ms = 0;
    m_thumbnails.clear();
    m_markers.clear();
    queue_draw();
}

// GTK4 snapshot virtual method - main rendering
void TimelineWidget::snapshot_vfunc(const Glib::RefPtr<Gtk::Snapshot>& snapshot) {
    const auto allocation = get_allocation();
    const int width = allocation.get_width();
    const int height = allocation.get_height();
    
    if (width <= 0 || height <= 0) return;
    
    // Get the C pointer for the snapshot
    GtkSnapshot* snap = snapshot->gobj();
    draw_all(snap, width, height);
}

// GTK4 measure virtual method - report our preferred size
void TimelineWidget::measure_vfunc(Gtk::Orientation orientation, int for_size,
                                   int& minimum, int& natural, 
                                   int& minimum_baseline, int& natural_baseline) const {
    if (orientation == Gtk::Orientation::VERTICAL) {
        minimum = get_preferred_height();
        natural = minimum;
    } else {
        // Horizontal - be flexible, but have a minimum
        minimum = 100;
        natural = std::max(for_size > 0 ? for_size : 400, 100);
    }
    minimum_baseline = -1;
    natural_baseline = -1;
}

void TimelineWidget::draw_all(GtkSnapshot* snapshot, int width, int height) {
    // Draw background
    draw_background(snapshot, width, height);
    
    // Draw thumbnail strip (top)
    draw_thumbnail_strip(snapshot, width);
    
    // Draw seek bar
    draw_seek_bar(snapshot, width);
    
    // Draw markers in the area below the seek bar
    draw_markers(snapshot, width);
    
    // Draw playhead on top of everything
    if (m_duration_ms > 0) {
        double playhead_x = static_cast<double>(m_position_ms) / static_cast<double>(m_duration_ms) * width;
        draw_playhead(snapshot, playhead_x, get_preferred_height());
    }
}

void TimelineWidget::draw_background(GtkSnapshot* snapshot, int width, int height) {
    // Draw visible background (lighter gray to be visible)
    GdkRGBA bg_color = { 0.25, 0.25, 0.25, 1.0 };
    
    graphene_rect_t bg_rect;
    graphene_rect_init(&bg_rect, 0, 0, 
        static_cast<float>(width), 
        static_cast<float>(height));
    
    gtk_snapshot_append_color(snapshot, &bg_color, &bg_rect);
    
    // Add a visible border
    // Top border
    GdkRGBA border_color = { 0.4, 0.4, 0.4, 1.0 };
    graphene_rect_t top_border;
    graphene_rect_init(&top_border, 0, 0, static_cast<float>(width), 2);
    gtk_snapshot_append_color(snapshot, &border_color, &top_border);
}

void TimelineWidget::draw_thumbnail_strip(GtkSnapshot* snapshot, int width) {
    if (m_thumbnails.empty() || m_duration_ms <= 0) return;
    
    int y = get_thumbnail_strip_y();
    float thumb_width = static_cast<float>(width) / static_cast<float>(m_thumbnails.size());
    float thumb_height = static_cast<float>(THUMBNAIL_HEIGHT);
    
    for (size_t i = 0; i < m_thumbnails.size(); ++i) {
        const auto& thumb = m_thumbnails[i];
        
        // Load and draw thumbnail image
        try {
            auto pixbuf = Gdk::Pixbuf::create_from_file(thumb.path);
            
            // Scale to fit
            float scale = std::min(thumb_width / pixbuf->get_width(), 
                                   thumb_height / pixbuf->get_height());
            float scaled_width = pixbuf->get_width() * scale;
            float scaled_height = pixbuf->get_height() * scale;
            
            float x = i * thumb_width + (thumb_width - scaled_width) / 2;
            float thumb_y = y + (thumb_height - scaled_height) / 2;
            
            graphene_rect_t thumb_rect;
    graphene_rect_init(&thumb_rect, x, thumb_y, scaled_width, scaled_height);
            
            // Create texture from pixbuf
            auto texture = Gdk::Texture::create_for_pixbuf(pixbuf);
            gtk_snapshot_append_texture(snapshot, texture->gobj(), &thumb_rect);
            
        } catch (const Glib::Error& e) {
            // Failed to load thumbnail, draw placeholder
            LUCID_DEBUG("[TimelineWidget] Failed to load thumbnail: %s - %s", 
                       thumb.path.c_str(), e.what());
            
            graphene_rect_t placeholder;
    graphene_rect_init(&placeholder,
                i * thumb_width, y, 
                thumb_width - 1, thumb_height);
            
            GdkRGBA placeholder_color = { 0.2, 0.2, 0.2, 1.0 };
            gtk_snapshot_append_color(snapshot, &placeholder_color, &placeholder);
        }
    }
}

void TimelineWidget::draw_seek_bar(GtkSnapshot* snapshot, int width) {
    int y = get_seek_bar_y();
    int height = get_seek_bar_height();
    float bar_width = static_cast<float>(width);
    float bar_height = static_cast<float>(height);
    
    // Draw track background (more visible)
    graphene_rect_t track_rect;
    graphene_rect_init(&track_rect, 0, y, bar_width, bar_height);
    GdkRGBA track_color = { 0.45, 0.45, 0.45, 1.0 };
    gtk_snapshot_append_color(snapshot, &track_color, &track_rect);
    
    // Draw progress
    if (m_duration_ms > 0) {
        double progress = static_cast<double>(m_position_ms) / static_cast<double>(m_duration_ms);
        progress = std::max(0.0, std::min(1.0, progress));
        
        float progress_width = static_cast<float>(progress * bar_width);
        
        graphene_rect_t progress_rect;
    graphene_rect_init(&progress_rect, 0, y, progress_width, bar_height);
        GdkRGBA progress_color = { 0.2, 0.5, 0.9, 1.0 }; // Blue
        gtk_snapshot_append_color(snapshot, &progress_color, &progress_rect);
    }
    
    // Note: Playhead is now drawn in draw_all() after markers
    // to ensure it appears on top of everything
}

void TimelineWidget::draw_markers(GtkSnapshot* snapshot, int width) {
    if (m_markers.empty() || m_duration_ms <= 0) return;
    
    float bar_width = static_cast<float>(width);
    int seek_y = get_seek_bar_y();
    int marker_area_y = get_marker_area_y();
    int marker_area_h = get_marker_area_height();
    
    // Draw marker area background (subtle, below seek bar)
    graphene_rect_t area_rect;
    graphene_rect_init(&area_rect, 0, static_cast<float>(marker_area_y), bar_width, static_cast<float>(marker_area_h));
    GdkRGBA area_bg = { 0.18, 0.18, 0.22, 1.0 };  // Subtle marker area background
    gtk_snapshot_append_color(snapshot, &area_bg, &area_rect);
    
    for (size_t i = 0; i < m_markers.size(); ++i) {
        const auto& marker = m_markers[i];
        
        double pos_x = position_to_x(marker.position_ms, bar_width);
        bool is_hovered = (static_cast<int>(i) == m_hovered_marker_index && m_is_hovering);
        
        // Marker color based on confidence and type
        GdkRGBA marker_color;
        if (marker.is_pending) {
            marker_color = { 0.5, 0.5, 0.5, 1.0 }; // Gray
        } else if (marker.marker_type == MarkerType::VISION) {
            marker_color = { 0.6, 0.3, 0.8, 1.0 }; // Purple
        } else if (marker.confidence >= 0.9f) {
            marker_color = { 0.2, 0.8, 0.2, 1.0 }; // Green - high confidence AUDIO
        } else if (marker.confidence >= 0.7f) {
            marker_color = { 0.9, 0.7, 0.2, 1.0 }; // Yellow - medium
        } else {
            marker_color = { 0.9, 0.3, 0.3, 1.0 }; // Red - low confidence
        }
        
        // Draw marker as a TICK MARK hanging down from seek bar into marker area
        if (is_hovered) {
            // Highlighted marker - wider indicator
            float marker_width = 8.0f;
            float marker_height = static_cast<float>(marker_area_h + 4);
            graphene_rect_t marker_rect;
            graphene_rect_init(&marker_rect,
                static_cast<float>(pos_x) - marker_width/2,
                static_cast<float>(seek_y),
                marker_width,
                marker_height);
            gtk_snapshot_append_color(snapshot, &marker_color, &marker_rect);
        } else {
            // Normal marker - tick mark (taller than wide)
            float marker_width = 4.0f;
            float marker_height = static_cast<float>(marker_area_h);
            graphene_rect_t marker_rect;
            graphene_rect_init(&marker_rect,
                static_cast<float>(pos_x) - marker_width/2,
                static_cast<float>(marker_area_y),
                marker_width,
                marker_height);
            gtk_snapshot_append_color(snapshot, &marker_color, &marker_rect);
        }
        
        // Draw outline for pending markers
        if (marker.is_pending) {
            float marker_width = 6.0f;
            graphene_rect_t outline_rect;
            graphene_rect_init(&outline_rect,
                static_cast<float>(pos_x) - marker_width/2,
                static_cast<float>(marker_area_y) - 1,
                marker_width,
                static_cast<float>(marker_area_h) + 2);
            GdkRGBA outline_color = { 0.7, 0.7, 0.7, 0.8 };
            gtk_snapshot_append_color(snapshot, &outline_color, &outline_rect);
        }
    }
}

void TimelineWidget::draw_playhead(GtkSnapshot* snapshot, double x, int height) {
    // Draw vertical playhead line
    graphene_rect_t line_rect;
    graphene_rect_init(&line_rect, static_cast<float>(x) - 1, 0, 2, height);
    
    GdkRGBA line_color = { 1.0, 1.0, 1.0, 1.0 }; // White
    gtk_snapshot_append_color(snapshot, &line_color, &line_rect);
    
    // Draw playhead handle (small rectangle at top)
    graphene_rect_t handle_rect;
    graphene_rect_init(&handle_rect, static_cast<float>(x) - 5, 0, 10, 10);
    gtk_snapshot_append_color(snapshot, &line_color, &handle_rect);
}

// Event handlers using GTK4 event controllers
void TimelineWidget::on_click_pressed(int n_press, double x, double y) {
    if (!m_seek_callback || m_duration_ms <= 0) return;
    
    int marker_area_y = get_marker_area_y();
    int marker_area_h = get_marker_area_height();
    int seek_y = get_seek_bar_y();
    int seek_h = get_seek_bar_height();
    
    // Check if clicked on a marker (in marker area below seek bar)
    if (y >= marker_area_y && y <= marker_area_y + marker_area_h) {
        const auto allocation = get_allocation();
        int marker_idx = find_marker_at_x(x, allocation.get_width());
        if (marker_idx >= 0 && marker_idx < static_cast<int>(m_markers.size())) {
            const auto& marker = m_markers[marker_idx];
            if (m_marker_click_callback) {
                m_marker_click_callback(marker);
            }
            return;
        }
    }
    
    // Check if click is in seek bar area
    if (y >= seek_y && y <= seek_y + seek_h) {
        m_is_dragging = true;
        const auto allocation = get_allocation();
        int64_t new_pos = x_to_position(x, allocation.get_width());
        set_position(new_pos);
        m_seek_callback(new_pos);
    }
}

void TimelineWidget::on_click_released(int n_press, double x, double y) {
    m_is_dragging = false;
}

void TimelineWidget::on_motion(double x, double y) {
    const auto allocation = get_allocation();
    double width = allocation.get_width();
    int marker_area_y = get_marker_area_y();
    int marker_area_h = get_marker_area_height();
    int seek_y = get_seek_bar_y();
    int seek_h = get_seek_bar_height();
    
    // Check for marker hover in marker area
    int new_hovered = find_marker_at_x(x, width);
    bool is_in_marker_area = (y >= marker_area_y && y <= marker_area_y + marker_area_h);
    bool is_in_seek_area = (y >= seek_y && y <= seek_y + seek_h);
    
    bool needs_redraw = false;
    
    if (is_in_marker_area) {
        if (new_hovered != m_hovered_marker_index) {
            m_hovered_marker_index = new_hovered;
            m_is_hovering = true;
            needs_redraw = true;
        }
    } else if (m_is_hovering) {
        if (!is_in_seek_area) {
            m_is_hovering = false;
            m_hovered_marker_index = -1;
            needs_redraw = true;
        }
    }
    
    // Handle dragging (only in seek bar area)
    if (m_is_dragging && m_seek_callback && m_duration_ms > 0 && is_in_seek_area) {
        int64_t new_pos = x_to_position(x, width);
        set_position(new_pos);
        m_seek_callback(new_pos);
        needs_redraw = true;
    }
    
    if (needs_redraw) {
        queue_draw();
    }
    
    // Update cursor style
    if ((is_in_marker_area && new_hovered >= 0) || (is_in_seek_area && m_is_dragging)) {
        gtk_widget_set_cursor_from_name(gobj(), "pointer");
    } else {
        gtk_widget_set_cursor_from_name(gobj(), nullptr);
    }
}

void TimelineWidget::on_leave() {
    if (m_is_hovering || m_hovered_marker_index >= 0) {
        m_is_hovering = false;
        m_hovered_marker_index = -1;
        queue_draw();
    }
}

// Layout helpers
double TimelineWidget::position_to_x(int64_t position_ms, double width) const {
    if (m_duration_ms <= 0) return 0;
    return (static_cast<double>(position_ms) / static_cast<double>(m_duration_ms)) * width;
}

int64_t TimelineWidget::x_to_position(double x, double width) const {
    if (width <= 0) return 0;
    double ratio = std::max(0.0, std::min(1.0, x / width));
    return static_cast<int64_t>(ratio * m_duration_ms);
}

int TimelineWidget::get_thumbnail_strip_y() const {
    return PADDING;
}

int TimelineWidget::get_thumbnail_strip_height() const {
    return THUMBNAIL_HEIGHT;
}

int TimelineWidget::get_seek_bar_y() const {
    return get_thumbnail_strip_y() + get_thumbnail_strip_height() + PADDING;
}

int TimelineWidget::get_seek_bar_height() const {
    return SEEK_BAR_HEIGHT;
}

// Marker area below the seek bar
int TimelineWidget::get_marker_area_y() const {
    return get_seek_bar_y() + get_seek_bar_height();
}

int TimelineWidget::get_marker_area_height() const {
    return MARKER_AREA_HEIGHT;
}

int TimelineWidget::find_marker_at_x(double x, double width) {
    if (m_markers.empty() || m_duration_ms <= 0) return -1;
    
    // Search radius (in pixels)
    const double hit_radius = 10.0;
    
    for (size_t i = 0; i < m_markers.size(); ++i) {
        double marker_x = position_to_x(m_markers[i].position_ms, width);
        if (std::abs(x - marker_x) <= hit_radius) {
            return static_cast<int>(i);
        }
    }
    
    return -1;
}

std::string TimelineWidget::format_time(int64_t ms) const {
    int64_t total_seconds = ms / 1000;
    int64_t hours = total_seconds / 3600;
    int64_t minutes = (total_seconds % 3600) / 60;
    int64_t seconds = total_seconds % 60;
    
    std::ostringstream oss;
    if (hours > 0) {
        oss << std::setfill('0') << std::setw(2) << hours << ":"
            << std::setw(2) << minutes << ":"
            << std::setw(2) << seconds;
    } else {
        oss << std::setfill('0') << std::setw(2) << minutes << ":"
            << std::setw(2) << seconds;
    }
    return oss.str();
}

} // namespace lucid