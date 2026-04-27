// marker_panel.cpp - Lucid Marker Panel Implementation
// Displays markers (both audio transcription and vision scan results)
#include "marker_panel.hpp"
#include "../logging.hpp"
#include "../model/database.hpp"  // For TranscriptionSegment

#include <iostream>

namespace lucid {

MarkerPanel::MarkerPanel()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 0) {
    setup_ui();
}

MarkerPanel::~MarkerPanel() = default;

void MarkerPanel::initialize(GtkWidget* parent) {
    if (parent) {
        gtk_widget_set_parent(GTK_WIDGET(gobj()), parent);
    }
}

void MarkerPanel::setup_ui() {
    // Header
    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_bottom(header, 8);
    gtk_box_append(GTK_BOX(gobj()), header);
    
    // Title label - renamed from "Transcription" to "Markers"
    GtkWidget* title = gtk_label_new("Markers");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(header), title);
    
    // Scrolled window for segments
    m_scrolled_window = gtk_scrolled_window_new();
    gtk_widget_set_hexpand(m_scrolled_window, TRUE);
    gtk_widget_set_vexpand(m_scrolled_window, TRUE);
    gtk_widget_set_size_request(m_scrolled_window, -1, 200);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(m_scrolled_window),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_append(GTK_BOX(gobj()), m_scrolled_window);
    
    // Container for segments
    m_segments_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_top(m_segments_box, 4);
    gtk_widget_set_margin_bottom(m_segments_box, 4);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(m_scrolled_window), m_segments_box);
    
    // Empty state label - updated to be marker-agnostic
    m_empty_label = gtk_label_new("No markers available.\nProcess the video to see markers here.");
    gtk_widget_set_halign(m_empty_label, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(m_empty_label, GTK_ALIGN_CENTER);
    gtk_widget_set_sensitive(m_empty_label, FALSE);
    gtk_box_append(GTK_BOX(m_segments_box), m_empty_label);
    
    LUCID_DEBUG("[MarkerPanel] UI setup complete");
}

void MarkerPanel::set_segments(const std::vector<TranscriptionSegment>& segments) {
    m_segments = segments;
    populate_segments();
}

void MarkerPanel::clear() {
    m_segments.clear();
    populate_segments();
}

void MarkerPanel::populate_segments() {
    // Remove all children from segments box
    GtkWidget* child = gtk_widget_get_first_child(m_segments_box);
    while (child) {
        GtkWidget* next = gtk_widget_get_next_sibling(child);
        gtk_widget_unparent(child);
        child = next;
    }
    
    if (m_segments.empty()) {
        // Show empty state - updated message
        m_empty_label = gtk_label_new("No markers available.\nProcess the video to see markers here.");
        gtk_widget_set_halign(m_empty_label, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(m_empty_label, GTK_ALIGN_CENTER);
        gtk_widget_set_sensitive(m_empty_label, FALSE);
        gtk_box_append(GTK_BOX(m_segments_box), m_empty_label);
        return;
    }
    
    // Add each segment as a clickable row
    for (const auto& segment : m_segments) {
        create_segment_widget(segment);
    }
    
    LUCID_DEBUG("[MarkerPanel] Populated %zu segments", m_segments.size());
}

void MarkerPanel::create_segment_widget(const TranscriptionSegment& segment) {
    // Create a container for the segment row
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(row, 8);
    gtk_widget_set_margin_end(row, 8);
    gtk_widget_set_margin_top(row, 2);
    gtk_widget_set_margin_bottom(row, 2);
    
    // Add CSS class for styling
    gtk_widget_add_css_class(row, "transcript-segment");
    
    // Make the row clickable
    gtk_widget_set_cursor_from_name(row, "pointer");
    
    // Format timestamp
    auto format_time = [](int64_t ms) -> std::string {
        int64_t total_seconds = ms / 1000;
        int64_t minutes = total_seconds / 60;
        int64_t seconds = total_seconds % 60;
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%02lld:%02lld", (long long)minutes, (long long)seconds);
        return std::string(buffer);
    };
    
    // Timestamp label
    std::string time_str = format_time(segment.start_ms);
    GtkWidget* time_label = gtk_label_new(time_str.c_str());
    gtk_widget_set_halign(time_label, GTK_ALIGN_START);
    gtk_widget_set_size_request(time_label, 50, -1);
    gtk_widget_add_css_class(time_label, "transcript-timestamp");
    gtk_box_append(GTK_BOX(row), time_label);
    
    // Text label
    std::string display_text = segment.text;
    if (display_text.length() > 200) {
        display_text = display_text.substr(0, 200) + "...";
    }
    GtkWidget* text_label = gtk_label_new(display_text.c_str());
    gtk_widget_set_halign(text_label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(text_label, TRUE);
    gtk_label_set_wrap(GTK_LABEL(text_label), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(text_label), PANGO_WRAP_WORD_CHAR);
    gtk_widget_add_css_class(text_label, "transcript-text");
    gtk_box_append(GTK_BOX(row), text_label);
    
    // Store segment data for click handler
    int64_t* start_ms_ptr = new int64_t(segment.start_ms);
    g_object_set_data_full(G_OBJECT(row), "start_ms", start_ms_ptr, [](gpointer data) {
        delete static_cast<int64_t*>(data);
    });
    
    // Add click gesture
    GtkGesture* click = gtk_gesture_click_new();
    gtk_widget_add_controller(row, GTK_EVENT_CONTROLLER(click));
    g_signal_connect(click, "pressed", G_CALLBACK(+[](GtkGestureClick* gesture, int n_press, double x, double y, gpointer data) {
        auto* panel = static_cast<MarkerPanel*>(data);
        GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
        
        // Find the start_ms from widget data
        int64_t start_ms = 0;
        gpointer ptr = g_object_get_data(G_OBJECT(widget), "start_ms");
        if (ptr) {
            start_ms = *static_cast<int64_t*>(ptr);
        }
        
        LUCID_DEBUG("[MarkerPanel] Segment clicked at %ld ms", start_ms);
        
        if (panel->m_seek_callback) {
            panel->m_seek_callback(start_ms);
        }
    }), this);
    
    // Add hover effect using CSS class toggle
    GtkEventController* hover = gtk_event_controller_motion_new();
    gtk_widget_add_controller(row, hover);
    g_signal_connect(hover, "enter", G_CALLBACK(+[](GtkEventController* controller, double x, double y, gpointer) {
        GtkWidget* widget = gtk_event_controller_get_widget(controller);
        gtk_widget_add_css_class(widget, "hover");
    }), nullptr);
    
    g_signal_connect(hover, "leave", G_CALLBACK(+[](GtkEventController* controller, gpointer) {
        GtkWidget* widget = gtk_event_controller_get_widget(controller);
        gtk_widget_remove_css_class(widget, "hover");
    }), nullptr);
    
    gtk_box_append(GTK_BOX(m_segments_box), row);
}

void MarkerPanel::set_visible(bool visible) {
    m_visible = visible;
    gtk_widget_set_visible(m_scrolled_window, visible);
}

} // namespace lucid
