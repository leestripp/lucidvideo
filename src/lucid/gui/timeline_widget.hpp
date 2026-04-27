// timeline_widget.hpp - Lucid Timeline Widget
#pragma once

#include <gtkmm.h>
#include <vector>
#include <functional>
#include "../model/database.hpp"

namespace lucid {

// Timeline marker from transcription
struct TimelineMarker {
    int64_t position_ms;          // Position in milliseconds
    std::string text;            // Marker label (transcription snippet)
    std::string thumbnail_path;   // Optional thumbnail
    float confidence = 0.0f;     // Transcription confidence
    bool is_pending = false;     // True if VISION marker waiting for description
    MarkerType marker_type = MarkerType::AUDIO;  // AUDIO or VISION

    TimelineMarker() = default;
    TimelineMarker(int64_t pos, const std::string& t) 
        : position_ms(pos), text(t) {}
};

// Timeline thumbnail
struct TimelineThumbnail {
    int64_t timestamp_ms;        // Timestamp in milliseconds
    std::string path;            // Path to thumbnail image

    TimelineThumbnail() = default;
    TimelineThumbnail(int64_t ts, const std::string& p) 
        : timestamp_ms(ts), path(p) {}
};

// Custom timeline widget using GTK4 event controllers and snapshot
class TimelineWidget : public Gtk::Widget {
public:
    TimelineWidget();
    ~TimelineWidget() override;

    // Configuration
    void set_duration(int64_t duration_ms);
    int64_t get_duration() const { return m_duration_ms; }
    
    void set_position(int64_t position_ms);
    int64_t get_position() const { return m_position_ms; }
    
    void set_thumbnails(const std::vector<TimelineThumbnail>& thumbs);
    const std::vector<TimelineThumbnail>& get_thumbnails() const { return m_thumbnails; }
    
    void set_markers(const std::vector<TimelineMarker>& markers);
    const std::vector<TimelineMarker>& get_markers() const { return m_markers; }
    
    void clear();

    // Callbacks
    using SeekCallback = std::function<void(int64_t position_ms)>;
    using MarkerClickCallback = std::function<void(const TimelineMarker& marker)>;
    
    void set_seek_callback(SeekCallback callback) { m_seek_callback = std::move(callback); }
    void set_marker_click_callback(MarkerClickCallback callback) { m_marker_click_callback = std::move(callback); }

    // Layout constants
    static constexpr int THUMBNAIL_HEIGHT = 48;
    static constexpr int MARKER_AREA_HEIGHT = 20;  // Gap below seek bar for markers
    static constexpr int SEEK_BAR_HEIGHT = 24;
    static constexpr int PADDING = 4;
    
    int get_preferred_height() const { 
        return THUMBNAIL_HEIGHT + PADDING + SEEK_BAR_HEIGHT + PADDING + MARKER_AREA_HEIGHT; 
    }

protected:
    // GTK4 widget virtual methods
    void snapshot_vfunc(const Glib::RefPtr<Gtk::Snapshot>& snapshot) override;
    void measure_vfunc(Gtk::Orientation orientation, int for_size,
                       int& minimum, int& natural, int& minimum_baseline, int& natural_baseline) const override;

private:
    // State
    int64_t m_duration_ms = 0;
    int64_t m_position_ms = 0;
    std::vector<TimelineThumbnail> m_thumbnails;
    std::vector<TimelineMarker> m_markers;
    
    // Callbacks
    SeekCallback m_seek_callback;
    MarkerClickCallback m_marker_click_callback;
    
    // Interaction state
    bool m_is_dragging = false;
    int m_hovered_marker_index = -1;
    bool m_is_hovering = false;
    double m_hover_x = 0;
    
    // Event controllers
    Glib::RefPtr<Gtk::EventControllerMotion> m_motion_controller;
    Glib::RefPtr<Gtk::GestureClick> m_click_controller;
    
    // Internal methods
    void setup_event_controllers();
    void on_click_pressed(int n_press, double x, double y);
    void on_click_released(int n_press, double x, double y);
    void on_motion(double x, double y);
    void on_leave();
    
    // Layout helpers
    double position_to_x(int64_t position_ms, double width) const;
    int64_t x_to_position(double x, double width) const;
    
    // Get region bounds
    int get_thumbnail_strip_y() const;
    int get_thumbnail_strip_height() const;
    int get_seek_bar_y() const;
    int get_seek_bar_height() const;
    int get_marker_area_y() const;
    int get_marker_area_height() const;
    
    // Drawing helpers (using C API)
    void draw_all(GtkSnapshot* snapshot, int width, int height);
    void draw_background(GtkSnapshot* snapshot, int width, int height);
    void draw_thumbnail_strip(GtkSnapshot* snapshot, int width);
    void draw_seek_bar(GtkSnapshot* snapshot, int width);
    void draw_markers(GtkSnapshot* snapshot, int width);
    void draw_playhead(GtkSnapshot* snapshot, double x, int height);
    
    // Find marker at position
    int find_marker_at_x(double x, double width);
    
    // Format time label
    std::string format_time(int64_t ms) const;
};

} // namespace lucid