// marker_panel.hpp - Lucid Marker Panel
// Displays markers (both audio transcription and vision scan results)
#pragma once

#include <gtkmm.h>
#include <string>
#include <functional>
#include <vector>
#include "../model/database.hpp"  // For TranscriptionSegment (used for both marker types)

namespace lucid {

class MarkerPanel : public Gtk::Box {
public:
    MarkerPanel();
    ~MarkerPanel() override;
    
    // Set segments to display
    void set_segments(const std::vector<TranscriptionSegment>& segments);
    void clear();
    
    // Seek callback when user clicks a segment
    using SeekCallback = std::function<void(int64_t position_ms)>;
    void set_seek_callback(SeekCallback callback) { m_seek_callback = std::move(callback); }
    
    // Initialize widget
    void initialize(GtkWidget* parent);
    GtkWidget* get_widget() const { return GTK_WIDGET(gobj()); }
    
    // Toggle visibility of panel
    void set_visible(bool visible);
    bool is_visible() const { return m_visible; }
    
private:
    void setup_ui();
    void populate_segments();
    void create_segment_widget(const TranscriptionSegment& segment);
    
    // GTK widgets
    GtkWidget* m_scrolled_window = nullptr;
    GtkWidget* m_segments_box = nullptr;
    GtkWidget* m_empty_label = nullptr;
    
    // Data
    std::vector<TranscriptionSegment> m_segments;
    bool m_visible = true;
    
    // Callback
    SeekCallback m_seek_callback;
};

} // namespace lucid
