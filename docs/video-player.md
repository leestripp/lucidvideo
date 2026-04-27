# Video Player Widget for GTK4

This document describes how to implement a video player widget for GTK4 applications using GStreamer.

## Overview

GTK4 provides two main approaches for video playback:
1. **High-level: `GtkVideo` widget** - Simple, built-in media controls
2. **Low-level: GStreamer with `GdkPaintable`** - Full control with `gtk4paintablesink`

## Option 1: Using GtkVideo (Simplest)

GTK4 includes a built-in `GtkVideo` widget for simple use cases:

```c
#include <gtk/gtk.h>

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new();
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    // Create GtkVideo widget
    GtkWidget *video = gtk_video_new();
    
    // Load a media file
    GFile *file = g_file_new_for_path("video.mp4");
    GtkMediaFile *media_file = gtk_media_file_new_for_file(file);
    gtk_video_set_media_stream(GTK_VIDEO(video), GTK_MEDIA_STREAM(media_file));

    gtk_window_set_child(GTK_WINDOW(window), video);
    gtk_widget_show(window);

    gtk_main();
    return 0;
}
```

### Advantages
- Very simple to use
- Built into GTK4, no extra dependencies beyond GStreamer
- Supports autoplay, looping, and basic controls

### Limitations
- Limited customization
- No direct access to playback position for timeline scrubbing

## Option 2: GStreamer with gtk4paintablesink (Recommended)

This approach gives full control over the video pipeline and integrates with GTK4's `GdkPaintable` interface.

### CMake Configuration

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-video-1.0)
pkg_check_modules(GTK4 REQUIRED gtk4)

# For video rendering
pkg_check_modules(GSTREAMER_BASE REQUIRED gstreamer-base-1.0)

# Include gst/video for GstVideoSink and gst VideoOverlay
pkg_search_module(GST_VIDEO gstreamer-video-1.0)

add_executable(my_player main.cpp video_player.cpp)

target_include_directories(my_player PRIVATE ${GTK4_INCLUDE_DIRS} ${GSTREAMER_INCLUDE_DIRS})

target_link_libraries(my_player PRIVATE 
    ${GTK4_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GST_VIDEO_LIBRARIES}
    m
)

target_compile_options(my_player PRIVATE ${GTK4_CFLAGS_OTHER} ${GSTREAMER_CFLAGS_OTHER})
```

### Video Player Header (video_player.hpp)

```cpp
#ifndef VIDEO_PLAYER_HPP
#define VIDEO_PLAYER_HPP

#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <memory>

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    // Load a video file
    void load(const char* uri);
    
    // Playback controls
    void play();
    void pause();
    void stop();
    void seek(double position_seconds);
    
    // Volume control (0.0 to 1.0)
    void set_volume(double volume);
    double get_volume() const;
    
    // Get current state
    bool is_playing() const;
    double get_duration() const;
    double get_position() const;

    // GTK widget
    GtkWidget* get_widget() { return m_picture; }

    // Callbacks (signals)
    using PositionCallback = std::function<void(double position)>;
    using DurationCallback = std::function<void(double duration)>;
    using StateCallback = std::function<void(bool playing)>;
    using EndedCallback = std::function<void()>;
    using ErrorCallback = std::function<void(const std::string& error)>;

    void set_position_callback(PositionCallback cb) { m_position_callback = std::move(cb); }
    void set_duration_callback(DurationCallback cb) { m_duration_callback = std::move(cb); }
    void set_state_callback(StateCallback cb) { m_state_callback = std::move(cb); }
    void set_ended_callback(EndedCallback cb) { m_ended_callback = std::move(cb); }
    void set_error_callback(ErrorCallback cb) { m_error_callback = std::move(cb); }

private:
    void setup_pipeline();
    void setup_widget();
    static void on_pad_added(GstElement* src, GstPad* pad, gpointer data);
    static gboolean on_bus_message(GstBus* bus, GstMessage* msg, gpointer data);
    static gboolean on_tick(gpointer data);

    GstElement* m_pipeline = nullptr;
    GstElement* m_playbin = nullptr;
    GstElement* m_video_sink = nullptr;
    
    GtkWidget* m_picture = nullptr;
    
    guint m_tick_id = 0;
    bool m_is_playing = false;
    
    PositionCallback m_position_callback;
    DurationCallback m_duration_callback;
    StateCallback m_state_callback;
    EndedCallback m_ended_callback;
    ErrorCallback m_error_callback;
};

#endif // VIDEO_PLAYER_HPP
```

### Video Player Implementation (video_player.cpp)

```cpp
#include "video_player.hpp"
#include <iostream>

VideoPlayer::VideoPlayer() {
    gst_init(nullptr, nullptr);
    setup_pipeline();
    setup_widget();
}

VideoPlayer::~VideoPlayer() {
    if (m_tick_id > 0) {
        g_source_remove(m_tick_id);
    }
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
    }
}

void VideoPlayer::setup_pipeline() {
    // Create playbin element (handles demuxing, decoding automatically)
    m_playbin = gst_element_factory_make("playbin", "playbin");
    if (!m_playbin) {
        std::cerr << "Failed to create playbin" << std::endl;
        return;
    }

    // Use gtk4paintablesink for GTK4 integration
    m_video_sink = gst_element_factory_make("gtk4paintablesink", "video-sink");
    if (m_video_sink) {
        g_object_set(m_playbin, "video-sink", m_video_sink, nullptr);
    }

    // Create pipeline
    m_pipeline = gst_pipeline_new("video-pipeline");
    gst_bin_add(GST_BIN(m_pipeline), m_playbin);

    // Setup bus for message handling
    GstBus* bus = gst_element_get_bus(m_pipeline);
    gst_bus_add_watch(bus, on_bus_message, this);
    gst_object_unref(bus);

    // Start tick for position updates
    m_tick_id = g_timeout_add(100, on_tick, this);
}

void VideoPlayer::setup_widget() {
    // Create GtkPicture and attach the paintable from gtk4paintablesink
    m_picture = gtk_picture_new();
    gtk_widget_set_hexpand(m_picture, TRUE);
    gtk_widget_set_vexpand(m_picture, TRUE);

    if (m_video_sink) {
        GstObject* sink_obj = GST_OBJECT(m_video_sink);
        GValue paintable_val = G_VALUE_INIT;
        g_value_init(&paintable_val, G_TYPE_OBJECT);
        
        g_object_get_property(G_OBJECT(m_video_sink), "paintable", &paintable_val);
        GObject* paintable = g_value_get_object(&paintable_val);
        
        if (paintable) {
            gtk_picture_set_paintable(GTK_PICTURE(m_picture), GDK_PAINTABLE(paintable));
        }
        g_value_unset(&paintable_val);
    }
}

void VideoPlayer::load(const char* uri) {
    if (!m_playbin) return;
    
    // Handle file paths or URIs
    std::string uri_str;
    if (g_str_has_prefix(uri, "file://") || g_str_has_prefix(uri, "http://") || 
        g_str_has_prefix(uri, "https://")) {
        uri_str = uri;
    } else {
        uri_str = "file://" + std::string(uri);
    }
    
    g_object_set(m_playbin, "uri", uri_str.c_str(), nullptr);
    gst_element_set_state(m_pipeline, GST_STATE_READY);
}

void VideoPlayer::play() {
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
        m_is_playing = true;
        if (m_state_callback) {
            m_state_callback(true);
        }
    }
}

void VideoPlayer::pause() {
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
        m_is_playing = false;
        if (m_state_callback) {
            m_state_callback(false);
        }
    }
}

void VideoPlayer::stop() {
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        m_is_playing = false;
    }
}

void VideoPlayer::seek(double position_seconds) {
    if (!m_playbin) return;
    
    gint64 duration;
    if (gst_element_query_duration(m_playbin, GST_FORMAT_TIME, &duration)) {
        gint64 target = static_cast<gint64>(position_seconds * GST_SECOND);
        target = std::min(target, duration);
        gst_element_seek_simple(m_playbin, GST_FORMAT_TIME,
            static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEYFRAME),
            target);
    }
}

void VideoPlayer::set_volume(double volume) {
    if (m_playbin) {
        volume = std::max(0.0, std::min(1.0, volume));
        g_object_set(m_playbin, "volume", volume, nullptr);
    }
}

double VideoPlayer::get_volume() const {
    if (m_playbin) {
        gdouble volume;
        g_object_get(m_playbin, "volume", &volume, nullptr);
        return volume;
    }
    return 0.0;
}

bool VideoPlayer::is_playing() const {
    return m_is_playing;
}

double VideoPlayer::get_duration() const {
    if (m_playbin) {
        gint64 duration;
        if (gst_element_query_duration(m_playbin, GST_FORMAT_TIME, &duration)) {
            return static_cast<double>(duration) / GST_SECOND;
        }
    }
    return 0.0;
}

double VideoPlayer::get_position() const {
    if (m_playbin) {
        gint64 position;
        if (gst_element_query_position(m_playbin, GST_FORMAT_TIME, &position)) {
            return static_cast<double>(position) / GST_SECOND;
        }
    }
    return 0.0;
}

// Static callback for GStreamer bus messages
gboolean VideoPlayer::on_bus_message(GstBus* bus, GstMessage* msg, gpointer data) {
    VideoPlayer* player = static_cast<VideoPlayer*>(data);
    
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError* err;
            gchar* debug;
            gst_message_parse_error(msg, &err, &debug);
            std::cerr << "GStreamer error: " << err->message << std::endl;
            if (player->m_error_callback) {
                player->m_error_callback(err->message);
            }
            g_error_free(err);
            g_free(debug);
            break;
        }
        case GST_MESSAGE_EOS:
            if (player->m_ended_callback) {
                player->m_ended_callback();
            }
            break;
        case GST_MESSAGE_DURATION_CHANGED:
            if (player->m_duration_callback) {
                player->m_duration_callback(player->get_duration());
            }
            break;
        default:
            break;
    }
    return TRUE;
}

// Static tick callback for position updates
gboolean VideoPlayer::on_tick(gpointer data) {
    VideoPlayer* player = static_cast<VideoPlayer*>(data);
    if (player->m_position_callback) {
        player->m_position_callback(player->get_position());
    }
    return TRUE;
}
```

## Key Concepts

### playbin
`playbin` is a GStreamer element that provides a complete playback pipeline:
- Automatically handles demuxing (MP4, MKV, etc.)
- Automatically handles decoding (H264, VP9, etc.)
- Provides audio and video output

### gtk4paintablesink
This GStreamer plugin provides a `GdkPaintable` interface for GTK4:
- Creates a video sink that exposes a `GdkPaintable`
- Integrates naturally with `GtkPicture` or any widget
- Handles the GTK rendering loop

### GstBus
The bus is used to receive messages from GStreamer pipeline:
- `GST_MESSAGE_ERROR` - Playback errors
- `GST_MESSAGE_EOS` - End of stream reached
- `GST_MESSAGE_DURATION_CHANGED` - Media duration determined

### Seek Flags
For accurate seeking, use keyframe-accurate seeking:
```cpp
gst_element_seek_simple(pipeline, GST_FORMAT_TIME,
    static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEYFRAME),
    target_time);
```

## Required GStreamer Plugins

Make sure the following GStreamer packages are installed:

```bash
# Debian/Ubuntu
sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad gstreamer1.0-libav

# Arch Linux
sudo pacman -S gstreamer gst-plugins-base gst-plugins-good gst-plugins-bad gst-libav

# Fedora
sudo dnf install gstreamer1-devel gstreamer1-plugins-base-devel \
    gstreamer1-plugins-good gstreamer1-plugins-bad-free \
    gstreamer1-libav
```

The `gtk4paintablesink` element requires `gst-plugins-rs` (Rust-based plugins) or recent GStreamer 1.24+.

## Fallback: GstVideoOverlay for older systems

If `gtk4paintablesink` is not available, use the X11/Wayland overlay approach:

```cpp
// Get native window handle
GdkWindow* window = gtk_widget_get_window(video_widget);
GdkSurface* surface = gtk_widget_get_surface(video_widget);

// Set video overlay window
if (surface) {
    guintptr window_handle = 0;
#if defined(GDK_WINDOWING_X11)
    if (GDK_IS_X11_SURFACE(surface)) {
        window_handle = gdk_x11_surface_get_xid(surface);
    }
#elif defined(GDK_WINDOWING_WAYLAND)
    if (GDK_IS_WAYLAND_SURFACE(surface)) {
        window_handle = gdk_wayland_surface_get_wl_surface(surface);
    }
#endif

    GstElement* overlay = gst_element_factory_make("xvimagesink", "video-sink");
    if (overlay) {
        gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(overlay), window_handle);
    }
}
```

## Additional Resources

- [GStreamer Basic Tutorial 5: GUI Toolkit Integration](https://gstreamer.freedesktop.org/documentation/tutorials/basic/toolkit-integration.html)
- [GStreamer gtk4paintablesink documentation](https://gstreamer.freedesktop.org/documentation/gtk4/index.html)
- [Official GStreamer examples repository](https://github.com/GStreamer/gst-examples/tree/master/playback/player/gtk)
- [Playbin documentation](https://gstreamer.freedesktop.org/documentation/playback/playbin.html)
- [GTK4 GdkPaintable interface](https://docs.gtk.org/gdk4/iface.Paintable.html)