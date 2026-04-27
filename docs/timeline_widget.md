# Timeline Widget Research for Lucid

## Overview

This document outlines research findings for implementing a timeline widget in Lucid's GTK4/gtkmm-4.0 interface. The timeline will display video thumbnails and transcription markers, allowing users to navigate to specific moments in videos.

## GTK4 Timeline Options

### Option 1: Custom Widget with GtkSnapshot (Recommended)

GTK4 introduced a new rendering API using `GtkSnapshot` instead of Cairo. Custom widgets can draw shapes, thumbnails, and markers directly.

**Resources:**
- [Custom widgets in GTK 4 – Introduction](https://blogs.gnome.org/gtk/2020/04/23/custom-widgets-in-gtk-4-introduction/)
- [Custom widgets in GTK 4 – Drawing](https://blogs.gnome.org/gtk/2020/04/24/custom-widgets-in-gtk-4-drawing/)
- [Custom widgets in GTK 4 – Layout](https://blogs.gnome.org/gtk/2020/04/27/custom-widgets-in-gtk-4-layout/)

### Option 2: Composite Standard Widgets

Combine existing GTK4 widgets:
- `GtkScale` - For the scrubber/seek bar
- `GtkPicture` or `GtkImage` - For thumbnails
- `GtkBox` - Container for markers
- `GtkGesture` - For click/drag handling

**Resources:**
- [Gtk.Scale API](https://docs.gtk.org/gtk4/class.Scale.html)
- [Gtk.Range API](https://docs.gtk.org/gtk4/class.Range.html)

### Option 3: GtkDrawingArea with Cairo

The legacy approach still works in GTK4 but is deprecated. Uses `GtkDrawingArea` with Cairo drawing.

**Resources:**
- [GTK4 Tutorial - DrawingArea](https://github.com/Toshiocp/Gtk4-tutorial/blob/main/gfm/sec27.md)

## Video Playback with GTK4

### GtkVideo Widget (Simplest)

GTK4 provides a built-in `GtkVideo` widget for basic playback:

```cpp
#include <gtkmm.h>

// Create video widget
GtkWidget* video = gtk_video_new();
gtk_video_set_file(GTK_VIDEO(video), gfile);

// For GStreamer pipeline
gtk_video_set_resource(GTK_VIDEO(video), "/path/to/video");
```

**Resources:**
- [Gtk.Video API](https://docs.gtk.org/gtk4/class.Video.html)
- [Stack Overflow: Playing Video in GTK4](https://stackoverflow.com/questions/78919693/playing-video-in-gtk4-window-using-gtkvideo)

### GStreamer Integration (Recommended for Control)

For full control (seek, position tracking), use GStreamer with `gtk4paintablesink`:

```c
#include <gst/gst.h>
#include <gtk/gtk.h>
#include <gst/video/video.h>
#include <gst/gtk4/gtk4paintablesink.h>

// Create pipeline
GstElement* pipeline = gst_element_factory_make("playbin", "player");
g_object_set(pipeline, "video-sink", gtk4paintablesink_new("sink"), NULL);

// Set URI
g_object_set(pipeline, "uri", "file:///path/to/video.mp4", NULL);

// Get paintable for GtkPicture
GstPaintable* paintable;
g_object_get(gtk4sink, "paintable", &paintable, NULL);
gtk_picture_set_paintable(GTK_PICTURE(picture), paintable);
```

**Resources:**
- [gtk4paintablesink - GStreamer](https://gstreamer.freedesktop.org/documentation/gtk4/index.html)
- [Best practice for pipeline to display video in Gtk4 app](https://discourse.gstreamer.org/t/best-practice-for-pipeline-to-display-video-in-gtk4-app/2372)
- [Basic tutorial 5: GUI toolkit integration](https://gstreamer.freedesktop.org/documentation/tutorials/basic/toolkit-integration.html)

## Timeline Widget Implementation

### Component Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│ TimelineWidget (Custom GtkWidget)                                 │
├─────────────────────────────────────────────────────────────────┤
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │ Thumbnails Strip (horizontal scroll)                         │ │
│ │ [thumb1] [thumb2] [thumb3] [thumb4] [thumb5] ...             │ │
│ └─────────────────────────────────────────────────────────────┘ │
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │ Seek Bar (GtkRange-based)                                   │ │
│ │ ▶ ████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │ │
│ │   ▲        ▲                    ▲                            │ │
│ │ Marker1  Marker2              Marker3                      │ │
│ └─────────────────────────────────────────────────────────────┘ │
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │ Time Labels: 00:00    00:30    01:00    01:30    02:00   │ │
│ └─────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

### Key Interface (C++)

```cpp
// timeline_widget.hpp
#pragma once

#include <gtkmm.h>
#include <vector>

namespace lucid {

struct TimelineMarker {
    int64_t position_ms;      // Position in milliseconds
    std::string text;          // Marker label (transcription snippet)
    std::string thumbnail_path; // Optional thumbnail
    float confidence;          // Transcription confidence
};

struct TimelineThumbnail {
    int64_t timestamp_ms;
    std::string path;          // Path to thumbnail image
};

class TimelineWidget : public Gtk::Widget {
public:
    TimelineWidget();
    ~TimelineWidget() override;
    
    // Configuration
    void set_duration(int64_t duration_ms);
    void set_position(int64_t position_ms);
    void set_thumbnails(const std::vector<TimelineThumbnail>& thumbs);
    void set_markers(const std::vector<TimelineMarker>& markers);
    
    // Callbacks
    using SeekCallback = std::function<void(int64_t position_ms)>;
    using MarkerClickCallback = std::function<void(const TimelineMarker& marker)>;
    
    void set_seek_callback(SeekCallback callback);
    void set_marker_click_callback(MarkerClickCallback callback);
    
protected:
    // GTK4 widget methods
    void snapshot_vfunc(const Glib::RefPtr<Gtk::Snapshot>& snapshot) override;
    void measure_vfunc(Gtk::Orientation orientation, int for_size,
                       int& minimum, int& natural, int& minimum_baseline, int& natural_baseline) override;
    void size_allocate_vfunc(int width, int height, int baseline) override;

private:
    int64_t m_duration_ms = 0;
    int64_t m_position_ms = 0;
    std::vector<TimelineThumbnail> m_thumbnails;
    std::vector<TimelineMarker> m_markers;
    
    SeekCallback m_seek_callback;
    MarkerClickCallback m_marker_click_callback;
    
    // Layout calculations
    double position_to_x(int64_t position_ms, double width);
    int64_t x_to_position(double x, double width);
    
    // Drawing helpers
    void draw_thumbnails(const Gtk::RefPtr<Gtk::Snapshot>& snapshot, const Gdk::Rectangle& rect);
    void draw_seek_bar(const Gtk::RefPtr<Gtk::Snapshot>& snapshot, const Gdk::Rectangle& rect);
    void draw_markers(const Gtk::RefPtr<Gtk::Snapshot>& snapshot, const Gdk::Rectangle& rect);
    void draw_time_labels(const Gtk::RefPtr<Gtk::Snapshot>& snapshot, const Gdk::Rectangle& rect);
};

} // namespace lucid
```

### Example: Simple GStreamer Video Player

```cpp
#include <gst/gst.h>
#include <gtk/gtk.h>
#include <gst/gtk4/gtk4paintablesink.h>

static void on_bus_message(GstBus* bus, GstMessage* msg, gpointer user_data) {
    GtkWidget* picture = GTK_WIDGET(user_data);
    
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_DURATION_CHANGED: {
            GstFormat format = GST_FORMAT_TIME;
            gint64 duration;
            if (gst_element_query_duration(GST_ELEMENT(GST_MESSAGE_SRC(msg)), &format, &duration)) {
                g_print("Duration: %" GST_TIME_FORMAT "\n", GST_TIME_ARGS(duration));
            }
            break;
        }
        case GST_MESSAGE_STATE_CHANGED: {
            GstState old_state, new_state;
            gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(GST_MESSAGE_SRC(msg))) {
                g_print("State changed from %s to %s\n",
                        gst_element_state_get_name(old_state),
                        gst_element_state_get_name(new_state));
            }
            break;
        }
    }
}

int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);
    gtk_init();
    
    // Create pipeline
    GstElement* pipeline = gst_pipeline_new(NULL);
    GstElement* source = gst_element_factory_make("filesrc", "source");
    GstElement* decodebin = gst_element_factory_make("decodebin3", "decoder");
    GstElement* conv = gst_element_factory_make("videoconvert", "conv");
    GstElement* gtk4sink = gst_element_factory_make("gtk4paintablesink", "sink");
    
    g_object_set(source, "location", "/path/to/video.mp4", NULL);
    g_object_set(gtk4sink, "paintable", NULL, NULL);
    
    gst_bin_add_many(GST_BIN(pipeline), source, decodebin, conv, gtk4sink, NULL);
    gst_element_link(source, decodebin);
    gst_element_link(conv, gtk4sink);
    
    // Connect decodebin (dynamic linking)
    g_signal_connect(decodebin, "pad-added", G_CALLBACK(+[](GstElement* src, GstPad* pad, gpointer data) {
        GstElement* conv = GST_ELEMENT(data);
        GstPad* sinkpad = gst_element_get_static_pad(conv, "sink");
        gst_pad_link(pad, sinkpad);
        gst_object_unref(sinkpad);
    }), conv);
    
    // Create UI
    GtkWidget* win = gtk_window_new();
    gtk_window_set_default_size(GTK_WINDOW(win), 800, 600);
    
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(win), box);
    
    GtkWidget* picture = gtk_picture_new();
    gtk_widget_set_hexpand(picture, TRUE);
    gtk_widget_set_vexpand(picture, TRUE);
    gtk_box_append(GTK_BOX(box), picture);
    
    // Get paintable and set on picture
    GObject* paintable_obj;
    g_object_get(gtk4sink, "paintable", &paintable_obj, NULL);
    if (paintable_obj) {
        gtk_picture_set_paintable(GTK_PICTURE(picture), GDK_PAINTABLE(paintable_obj));
        g_object_unref(paintable_obj);
    }
    
    // Seek slider
    GtkWidget* scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_box_append(GTK_BOX(box), scale);
    
    g_signal_connect(scale, "change-value", G_CALLBACK(+[](GtkRange* range, GtkScrollType scroll, double value, gpointer data) {
        GstElement* pipeline = GST_ELEMENT(data);
        gst_element_seek_simple(pipeline, GST_FORMAT_TIME, 
                               GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
                               (gint64)(value * GST_SECOND));
    }), pipeline);
    
    // Bus messages
    GstBus* bus = gst_element_get_bus(pipeline);
    gst_bus_add_watch(bus, on_bus_message, picture);
    gst_object_unref(bus);
    
    // Play
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    
    gtk_widget_show(win);
    gtk_main();
    
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    
    return 0;
}
```

## Dependencies

For video playback with GStreamer:
```
# Arch Linux
pacman -S gst-plugins-base gst-plugins-good gst-plugins-bad gst-libav gtk4 gstreamer

# Or with gtkmm4
pacman -S gtkmm4
```

For gst-plugins-gtk4 (provides gtk4paintablesink):
```
# May need to build from source or use gst-build
```

## Recommended Implementation Order

1. **Video Playback** - Get basic video displaying first using GtkVideo or GStreamer
2. **Seek Control** - Implement position tracking and seeking
3. **Timeline Strip** - Add thumbnail strip below video
4. **Marker Overlay** - Draw transcription markers on timeline
5. **Click Handling** - Connect marker clicks to seeking

## References

- [GTK4 Custom Widgets Blog Series](https://blogs.gnome.org/gtk/2020/04/23/custom-widgets-in-gtk-4-introduction/)
- [GStreamer GTK4 Integration](https://gstreamer.freedesktop.org/documentation/gtk4/index.html)
- [gtk_timeline - GTK3 Timeline Widget](https://github.com/colossus73/gtk_timeline) (Reference for concept)
- [Basic Tutorial 5: GUI Toolkit Integration](https://gstreamer.freedesktop.org/documentation/tutorials/basic/toolkit-integration.html)
