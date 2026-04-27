# GTK4 Video Player Integration Guide

## Overview

This document covers options for embedding video playback in Lucid's GTK4/gtkmm4 application. We need video playback for:
- Playing indexed videos
- Preview thumbnails on hover
- Seeking to specific timestamps from search results

## Options Summary

| Option | Complexity | Features | Dependencies | Best For |
|--------|------------|----------|--------------|----------|
| **libmpv** | Medium | Full-featured, HW acceleration | libmpv | Production video player |
| **GStreamer + gtk4paintablesink** | Medium | Hardware decoding, pipelines | gstreamer, gst-plugins-* | Custom pipelines |
| **GtkVideo** (built-in) | Low | Simple playback | gtk4-media-gstreamer | Previews, simple playback |
| **External player (mpv)** | Low | Full-featured | mpv installed | Quick implementation |

---

## Option 1: libmpv (Recommended)

**Why libmpv?**
- Most feature-rich video playback library
- Excellent hardware acceleration (VA-API, VDPAU, NVDEC, CUDA)
- Works on X11 and Wayland
- Used by Celluloid, mpv itself, and many other players
- Active development, well-maintained

### Installation (Arch Linux)

```bash
sudo pacman -S mpv libmpv
```

### Dependencies for CMake

```cmake
pkg_check_modules(MPV REQUIRED libmpv)
```

### CMakeLists.txt Integration

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtk4)
pkg_check_modules(MPV REQUIRED libmpv)

target_link_libraries(lucid PRIVATE
    ${GTK4_LIBRARIES}
    ${MPV_LIBRARIES}
    pthread
)
```

### libmpv Header

```cpp
#include <mpv/client.h>
```

### MpvPlayer Class Implementation

**File**: `src/lucid/gui/mpv_player.hpp`

```cpp
#pragma once

#include <gtkmm.h>
#include <mpv/client.h>
#include <string>
#include <functional>

namespace lucid {

class MpvPlayer : public Gtk::Widget {
public:
    MpvPlayer();
    ~MpvPlayer() override;
    
    // Playback control
    void play(const std::string& uri);
    void pause();
    void resume();
    void stop();
    void seek(double seconds);
    void seek_relative(double delta_seconds);
    
    // Volume
    void set_volume(double volume);  // 0.0 to 1.0
    double get_volume() const;
    
    // State
    bool is_playing() const;
    bool is_paused() const;
    double get_current_time() const;  // seconds
    double get_duration() const;        // seconds
    
    // Fullscreen
    void set_fullscreen(bool fullscreen);
    bool get_fullscreen() const;
    
    // Loop
    void set_loop(bool loop);
    bool get_loop() const;
    
    // Media URI (file:// or https://)
    void set_media(const std::string& uri);
    
signals:
    void playback_started();
    void playback_paused();
    void playback_stopped();
    void playback_finished();
    void seeked(double new_time);
    void time_updated(double current_time);
    void error_occurred(const std::string& error);

protected:
    // GtkWidget overrides
    void on_realize() override;
    void on_unrealize() override;
    void on_size_allocate(Gtk::Allocation& allocation) override;
    void measure(Gtk::Orientation orientation, int for_size, 
                  int& minimum, int& natural, int& minimum_baseline, 
                  int& natural_baseline) override;
    Gdk::Surface* on_ref_surface() override;
    
    // GLib source callback for events
    bool on_mpv_events();
    bool on_mpv_render();

private:
    void initialize_mpv();
    void uninitialize_mpv();
    void command(const char* cmd, ...);
    void set_property(const char* name, const char* value);
    double get_property_double(const char* name);
    int64_t get_property_int64(const char* name);
    
    mpv_handle* m_mpv = nullptr;
    mpv_render_context* m_render_ctx = nullptr;
    
    bool m_fullscreen = false;
    bool m_loop = false;
    std::string m_current_uri;
    
    // Wayland/X11 surface handling
    void* m_window_handle = nullptr;
    bool m_surface_realized = false;
    
    // GLib event sources
    guint m_mpv_events_id = 0;
    guint m_mpv_render_id = 0;
    
    std::function<void(mpv_event*)> handle_mpv_event;
};

// Helper: Convert file path to URI
inline std::string path_to_uri(const std::string& path) {
    if (path.starts_with("http://") || path.starts_with("https://")) {
        return path;
    }
    // Assume local file
    char* uri = g_filename_to_uri(path.c_str(), nullptr, nullptr);
    std::string result = uri ? uri : "file://" + path;
    g_free(uri);
    return result;
}

// Helper: Convert URI to file path
inline std::string uri_to_path(const std::string& uri) {
    if (uri.starts_with("file://")) {
        char* path = g_filename_from_uri(uri.c_str(), nullptr, nullptr);
        std::string result = path ? path : "";
        g_free(path);
        return result;
    }
    return uri;
}
```

**File**: `src/lucid/gui/mpv_player.cpp`

```cpp
#include "mpv_player.hpp"
#include <gdk/gdk.h>
#include <GL/gl.h>

namespace lucid {

// Forward declarations for mpv render API
static void* get_proc_address(void* ctx, const char* name);

MpvPlayer::MpvPlayer() {
    set_has_window(true);
    set_focusable(true);
}

MpvPlayer::~MpvPlayer() {
    uninitialize_mpv();
}

void MpvPlayer::on_realize() {
    Gtk::Widget::on_realize();
    
    // Get GdkSurface
    auto surface = get_surface();
    if (!surface) return;
    
    // Get native window handle for mpv
    auto display = surface->get_display();
    
    // Store the window handle for mpv
    // This depends on whether we're on X11 or Wayland
    GdkSurface* gdk_surface = surface->gobj();
    
    // Get the native handle (works on both X11 and Wayland)
    m_window_handle = reinterpret_cast<void*>(
        gdk_surface_get_wayland_surface ?
        gdk_surface_get_wayland_surface(gdk_surface) :
        reinterpret_cast<void*>(gdk_surface_get_xid(gdk_surface))
    );
    
    m_surface_realized = true;
    
    // Initialize mpv with the window
    initialize_mpv();
    
    // Load media if set
    if (!m_current_uri.empty()) {
        set_media(m_current_uri);
    }
}

void MpvPlayer::on_unrealize() {
    uninitialize_mpv();
    Gtk::Widget::on_unrealize();
}

void MpvPlayer::on_size_allocate(Gtk::Allocation& allocation) {
    Gtk::Widget::on_size_allocate(allocation);
    
    if (m_render_ctx) {
        // Notify mpv of size change
        int width = allocation.get_width();
        int height = allocation.get_height();
        
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_WIN_POSITION, "xxxx"},
            {MPV_RENDER_PARAM_INVALID, nullptr}
        };
        
        // Trigger a re-render
        if (m_mpv_render_id == 0) {
            m_mpv_render_id = g_idle_add([](gpointer data) -> gboolean {
                auto* player = static_cast<MpvPlayer*>(data);
                if (player->m_render_ctx) {
                    mpv_render_context_update(player->m_render_ctx);
                }
                return G_SOURCE_REMOVE;
            }, this);
        }
    }
}

void MpvPlayer::measure(Gtk::Orientation orientation, int for_size,
                        int& minimum, int& natural, 
                        int& minimum_baseline, int& natural_baseline) {
    // 16:9 aspect ratio by default
    if (orientation == Gtk::Orientation::HORIZONTAL) {
        minimum = 320;
        natural = for_size > 0 ? for_size : 640;
    } else {
        minimum = 180;
        natural = for_size > 0 ? for_size * 9 / 16 : 360;
    }
    minimum_baseline = -1;
    natural_baseline = -1;
}

Gdk::Surface* MpvPlayer::on_ref_surface() {
    return Gtk::Widget::on_ref_surface();
}

void MpvPlayer::initialize_mpv() {
    // Create mpv context
    m_mpv = mpv_create();
    if (!m_mpv) {
        g_warning("Failed to create mpv context");
        return;
    }
    
    // Set mpv options for GTK4 embedding
    mpv_set_option_string(m_mpv, "terminal", "no");
    mpv_set_option_string(m_mpv, "input-cursor", "no");
    mpv_set_option_string(m_mpv, "cursor-autohide", "no");
    
    // Enable hardware decoding (auto-select best method)
    mpv_set_option_string(m_mpv, "hwdec", "auto");
    
    // Disable OSD (we'll show our own controls)
    mpv_set_option_string(m_mpv, "osc", "no");
    
    // Set video output for Wayland/X11
    const char* vo = "gpu";
    mpv_set_option_string(m_mpv, "vo", vo);
    
    // Initialize mpv
    if (mpv_initialize(m_mpv) < 0) {
        g_warning("Failed to initialize mpv");
        mpv_destroy(m_mpv);
        m_mpv = nullptr;
        return;
    }
    
    // Set up event handling
    int fd = mpv_get_fd(m_mpv);
    if (fd >= 0) {
        // Use GLib to watch the fd for events
        Glib::RefPtr<Glib::MainContext> ctx = Glib::MainContext::get_default();
        
        // Note: For actual implementation, you'd use a custom source
        // This is simplified - see mpv-examples for proper fd handling
    }
    
    // If we have a window handle, set it
    if (m_window_handle) {
        mpv_set_option(m_mpv, "wid", MPV_FORMAT_INT64, 
                      reinterpret_cast<int64_t>(m_window_handle));
    }
}

void MpvPlayer::uninitialize_mpv() {
    if (m_render_ctx) {
        mpv_render_context_destroy(m_render_ctx);
        m_render_ctx = nullptr;
    }
    
    if (m_mpv) {
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
    }
    
    if (m_mpv_events_id > 0) {
        g_source_remove(m_mpv_events_id);
        m_mpv_events_id = 0;
    }
    
    if (m_mpv_render_id > 0) {
        g_source_remove(m_mpv_render_id);
        m_mpv_render_id = 0;
    }
}

void MpvPlayer::set_media(const std::string& uri) {
    if (!m_mpv) {
        m_current_uri = uri;
        return;
    }
    
    m_current_uri = uri;
    command("loadfile", uri.c_str(), "replace");
}

void MpvPlayer::play(const std::string& uri) {
    if (!uri.empty()) {
        set_media(uri);
    }
    command("set", "pause", "no");
}

void MpvPlayer::pause() {
    command("set", "pause", "yes");
}

void MpvPlayer::resume() {
    command("set", "pause", "no");
}

void MpvPlayer::stop() {
    command("stop");
}

void MpvPlayer::seek(double seconds) {
    command("seek", std::to_string(seconds).c_str(), "absolute");
}

void MpvPlayer::seek_relative(double delta_seconds) {
    command("seek", std::to_string(delta_seconds).c_str(), "relative");
}

void MpvPlayer::set_volume(double volume) {
    command("set", "volume", std::to_string(volume * 100).c_str());
}

double MpvPlayer::get_volume() const {
    if (!m_mpv) return 1.0;
    return get_property_double("volume") / 100.0;
}

bool MpvPlayer::is_playing() const {
    if (!m_mpv) return false;
    return get_property_double("playback-time") > 0 && 
           get_property_int64("paused") == 0;
}

bool MpvPlayer::is_paused() const {
    if (!m_mpv) return true;
    return get_property_int64("paused") != 0;
}

double MpvPlayer::get_current_time() const {
    if (!m_mpv) return 0.0;
    return get_property_double("playback-time");
}

double MpvPlayer::get_duration() const {
    if (!m_mpv) return 0.0;
    double dur = get_property_double("duration");
    return dur > 0 ? dur : 0.0;
}

void MpvPlayer::set_fullscreen(bool fullscreen) {
    m_fullscreen = fullscreen;
    command("set", "fullscreen", fullscreen ? "yes" : "no");
}

bool MpvPlayer::get_fullscreen() const {
    return m_fullscreen;
}

void MpvPlayer::set_loop(bool loop) {
    m_loop = loop;
    command("set", "loop", loop ? "inf" : "no");
}

bool MpvPlayer::get_loop() const {
    return m_loop;
}

void MpvPlayer::command(const char* cmd, ...) {
    if (!m_mpv) return;
    
    va_list ap;
    va_start(ap, cmd);
    
    const char* args[32];
    args[0] = cmd;
    int n = 1;
    
    while (n < 31) {
        const char* arg = va_arg(ap, const char*);
        if (!arg) break;
        args[n++] = arg;
    }
    args[n] = nullptr;
    
    mpv_command(m_mpv, args);
    va_end(ap);
}

void MpvPlayer::set_property(const char* name, const char* value) {
    if (!m_mpv) return;
    mpv_set_property(m_mpv, name, MPV_FORMAT_STRING, &value);
}

double MpvPlayer::get_property_double(const char* name) {
    if (!m_mpv) return 0.0;
    
    double value = 0.0;
    if (mpv_get_property(m_mpv, name, MPV_FORMAT_DOUBLE, &value) < 0) {
        return 0.0;
    }
    return value;
}

int64_t MpvPlayer::get_property_int64(const char* name) {
    if (!m_mpv) return 0;
    
    int64_t value = 0;
    if (mpv_get_property(m_mpv, name, MPV_FORMAT_INT64, &value) < 0) {
        return 0;
    }
    return value;
}

} // namespace lucid
```

---

## Option 2: GStreamer with gtk4paintablesink

For more custom pipelines, GStreamer with gtk4paintablesink provides fine-grained control.

### Installation (Arch Linux)

```bash
sudo pacman -S gstreamer gst-plugins-base gst-plugins-good \
    gst-plugins-bad gst-libav gst-plugin-gtk4
```

### GTK4 Video Widget with GStreamer

```cpp
// video_widget.hpp
#pragma once

#include <gtkmm.h>
#include <gstreamermm.h>

class VideoWidget : public Gtk::Widget {
public:
    VideoWidget();
    ~VideoWidget() override;
    
    void play(const std::string& uri);
    void pause();
    void stop();
    void seek(double seconds);
    
    Glib::RefPtr<Gst::Player::Player> get_player() { return m_player; }

protected:
    void on_realize() override;
    void on_unrealize() override;
    void measure(Gtk::Orientation orientation, int for_size,
                 int& minimum, int& natural,
                 int& minimum_baseline, int& natural_baseline) override;

private:
    Glib::RefPtr<Gst::Player::Player> m_player;
    Glib::RefPtr<Gst::Player::VideoOverlay> m_video_overlay;
};
```

```cpp
// video_widget.cpp
#include "video_widget.hpp"
#include <gstreamermm/playbin.h>

VideoWidget::VideoWidget() {
    // Initialize GStreamer
    Gst::init();
    
    // Create player
    m_player = Gst::Player::Player::create();
    m_video_overlay = Gst::Player::VideoOverlay::create();
    
    // Set video sink to gtk4paintablesink
    try {
        auto sink = Gst::ElementFactory::create_element("gtk4paintablesink");
        if (sink) {
            m_player->set_video_sink(sink);
        }
    } catch (const std::exception& e) {
        g_warning("Failed to create gtk4paintablesink: %s", e.what());
    }
    
    // Connect video overlay to our widget
    auto video_sink = Glib::RefPtr<Gst::VideoSink>::cast_static(
        m_player->get_video_sink()
    );
    
    if (video_sink) {
        // Get the GtkWidget from the sink if it exposes one
        // gtk4paintablesink provides a GdkTexture
    }
    
    set_has_window(true);
}

VideoWidget::~VideoWidget() {
    m_player->stop();
}

void VideoWidget::on_realize() {
    Gtk::Widget::on_realize();
    
    auto surface = get_surface();
    if (surface && m_video_overlay) {
        // Set window handle for video output
        // This handles both X11 and Wayland
        m_video_overlay->set_window_handle(
            reinterpret_cast<uint64_t>(surface->gobj())
        );
    }
}

void VideoWidget::play(const std::string& uri) {
    auto uri_glib = Glib::Uri::for_uri(uri);
    m_player->set_uri(uri_glib.get_uri());
    m_player->play();
}

void VideoWidget::pause() {
    m_player->pause();
}

void VideoWidget::stop() {
    m_player->stop();
}

void VideoWidget::seek(double seconds) {
    auto position = Gst::ClockTime::from_seconds(static_cast<uint64_t>(seconds));
    m_player->seek(position);
}

void VideoWidget::measure(Gtk::Orientation orientation, int for_size,
                          int& minimum, int& natural,
                          int& minimum_baseline, int& natural_baseline) {
    if (orientation == Gtk::Orientation::HORIZONTAL) {
        minimum = 320;
        natural = 640;
    } else {
        minimum = 180;
        natural = 360;
    }
}
```

---

## Option 3: Built-in GtkVideo (Simplest)

For simple preview playback, GTK4's built-in GtkVideo widget is the easiest option.

### Installation

```bash
# Arch Linux - requires gstreamer plugin
sudo pacman -S gtk4 gtk4-media-gstreamer
```

### Simple Video Preview Widget

```cpp
// simple_video_preview.hpp
#pragma once

#include <gtkmm.h>

class SimpleVideoPreview : public Gtk::Box {
public:
    SimpleVideoPreview();
    ~SimpleVideoPreview() override;
    
    void set_media_file(const std::string& path);
    void play();
    void pause();
    void stop();
    
private:
    Gtk::Video* m_video = nullptr;
    Glib::RefPtr<Gtk::MediaFile> m_media_file;
};
```

```cpp
// simple_video_preview.cpp
#include "simple_video_preview.hpp"

SimpleVideoPreview::SimpleVideoPreview()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 0) {
    
    // Create media file
    m_media_file = Gtk::MediaFile::create();
    
    // Create video widget
    m_video = Gtk::make_managed<Gtk::Video>();
    m_video->set_media_stream(m_media_file);
    m_video->set_autoplay(true);
    m_video->set_loop(false);
    
    // Set expansion
    m_video->set_hexpand(true);
    m_video->set_vexpand(true);
    
    append(*m_video);
}

void SimpleVideoPreview::set_media_file(const std::string& path) {
    // Convert path to URI if needed
    Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(path);
    m_media_file->set_file(file);
}

void SimpleVideoPreview::play() {
    m_media_file->play();
}

void SimpleVideoPreview::pause() {
    m_media_file->pause();
}

void SimpleVideoPreview::stop() {
    m_media_file->pause();
    m_media_file->seek_to_start();
}
```

**Note**: GtkVideo is suitable for previews and simple playback. For Lucid's main video player with features like seeking, speed control, and hardware acceleration, use libmpv.

---

## Option 4: External Player (Quickest)

For the quickest implementation, open videos in the system's default player (mpv) via command line.

### Implementation

```cpp
#include <gtkmm.h>
#include <spawn.h>
#include <unistd.h>

void open_in_external_player(const std::string& video_path, 
                            int seek_seconds = -1) {
    std::vector<std::string> args = {"mpv"};
    
    // Seek to timestamp if provided
    if (seek_seconds >= 0) {
        args.push_back("--start=" + std::to_string(seek_seconds));
    }
    
    args.push_back(video_path);
    args.push_back(nullptr);
    
    // Convert to char* for posix_spawn
    std::vector<char*> argv;
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    
    pid_t pid;
    posix_spawnp(&pid, "mpv", nullptr, nullptr, argv.data(), nullptr);
    
    // Don't wait - let mpv run independently
}
```

**Pros**: Zero integration code, full-featured player
**Cons**: No embedded player in Lucid, separate window, no custom controls

---

## Recommendation for Lucid

For **Lucid's video player**, I recommend:

### Primary: libmpv

**Why:**
1. Hardware acceleration (VA-API, NVDEC) for 4K/HDR playback
2. Excellent seeking performance (critical for "jump to moment" feature)
3. Works on both X11 and Wayland
4. Used by Celluloid (proven in production)
5. Can add custom OSD/controls on top

**Trade-offs:**
- More complex integration than GtkVideo
- Requires libmpv dependency

### For Preview Thumbnails: GtkVideo

**Why:**
- Simpler for hover-preview animations
- Lower overhead for short clips
- Built into GTK4

---

## Video Player Module for Lucid

**File**: `src/lucid/gui/video_player_page.hpp`

```cpp
// video_player_page.hpp
#pragma once

#include <gtkmm.h>
#include "mpv_player.hpp"

class VideoPlayerPage : public Gtk::Box {
public:
    VideoPlayerPage();
    ~VideoPlayerPage() override;
    
    // Load and play a video
    void load_video(const std::string& path, int seek_to_ms = 0);
    
    // Update from search results
    void jump_to_moment(int timestamp_ms);
    
private:
    void setup_ui();
    void setup_controls();
    void on_time_changed(double time);
    void on_playback_finished();
    
    // Video display
    lucid::MpvPlayer* m_player = nullptr;
    Gtk::Box* m_controls = nullptr;
    
    // Control widgets
    Gtk::Button* m_play_btn = nullptr;
    Gtk::Button* m_pause_btn = nullptr;
    Gtk::Button* m_fullscreen_btn = nullptr;
    Gtk::Scale* m_seek_bar = nullptr;
    Gtk::Label* m_time_label = nullptr;
    Gtk::Label* m_duration_label = nullptr;
    
    // State
    std::string m_current_video;
    bool m_user_seeking = false;
};
```

```cpp
// video_player_page.cpp
#include "video_player_page.hpp"

VideoPlayerPage::VideoPlayerPage()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 0) {
    setup_ui();
}

void VideoPlayerPage::setup_ui() {
    // Video player (takes remaining space)
    m_player = Gtk::make_managed<lucid::MpvPlayer>();
    m_player->set_hexpand(true);
    m_player->set_vexpand(true);
    
    // Connect player signals
    m_player->signal_time_updated().connect(
        [this](double time) { on_time_changed(time); }
    );
    m_player->signal_playback_finished().connect(
        [this]() { on_playback_finished(); }
    );
    
    append(*m_player);
    
    // Controls bar
    m_controls = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    m_controls->set_margin_start(12);
    m_controls->set_margin_end(12);
    m_controls->set_margin_top(6);
    m_controls->set_margin_bottom(6);
    
    // Play/Pause button
    m_play_btn = Gtk::make_managed<Gtk::Button>();
    m_play_btn->set_icon_name("media-playback-start-symbolic");
    m_play_btn->signal_clicked().connect([this]() {
        m_player->resume();
    });
    m_controls->append(*m_play_btn);
    
    m_pause_btn = Gtk::make_managed<Gtk::Button>();
    m_pause_btn->set_icon_name("media-playback-pause-symbolic");
    m_pause_btn->set_visible(false);
    m_pause_btn->signal_clicked().connect([this]() {
        m_player->pause();
    });
    m_controls->append(*m_pause_btn);
    
    // Time label
    m_time_label = Gtk::make_managed<Gtk::Label>("0:00");
    m_time_label->set_width_chars(8);
    m_controls->append(*m_time_label);
    
    // Seek bar
    m_seek_bar = Gtk::make_managed<Gtk::Scale>(Gtk::Orientation::HORIZONTAL, nullptr);
    m_seek_bar->set_hexpand(true);
    m_seek_bar->set_range(0, 100);
    m_seek_bar->set_increments(1, 10);
    m_seek_bar->signal_value_changed().connect([this]() {
        if (m_user_seeking) {
            double seek_time = m_seek_bar->get_value();
            m_player->seek(seek_time);
        }
    });
    m_seek_bar->signal_pressed().connect([this]() {
        m_user_seeking = true;
    });
    m_seek_bar->signal_released().connect([this]() {
        m_user_seeking = false;
    });
    m_controls->append(*m_seek_bar);
    
    // Duration label
    m_duration_label = Gtk::make_managed<Gtk::Label>("0:00");
    m_duration_label->set_width_chars(8);
    m_controls->append(*m_duration_label);
    
    // Fullscreen button
    m_fullscreen_btn = Gtk::make_managed<Gtk::Button>();
    m_fullscreen_btn->set_icon_name("view-fullscreen-symbolic");
    m_fullscreen_btn->signal_clicked().connect([this]() {
        m_player->set_fullscreen(!m_player->get_fullscreen());
    });
    m_controls->append(*m_fullscreen_btn);
    
    append(*m_controls);
}

void VideoPlayerPage::load_video(const std::string& path, int seek_to_ms) {
    m_current_video = path;
    
    // Convert path to URI
    std::string uri = path_to_uri(path);
    
    // Load and optionally seek
    m_player->set_media(uri);
    m_player->play();
    
    if (seek_to_ms > 0) {
        m_player->seek(seek_to_ms / 1000.0);
    }
}

void VideoPlayerPage::jump_to_moment(int timestamp_ms) {
    m_player->seek(timestamp_ms / 1000.0);
}

void VideoPlayerPage::on_time_changed(double time) {
    if (!m_user_seeking) {
        m_time_label->set_text(format_time(time));
        
        double duration = m_player->get_duration();
        if (duration > 0) {
            m_seek_bar->set_value(time);
            m_duration_label->set_text(format_time(duration));
        }
    }
}

void VideoPlayerPage::on_playback_finished() {
    m_play_btn->set_visible(true);
    m_pause_btn->set_visible(false);
}

// Helper: Format seconds to MM:SS or HH:MM:SS
std::string format_time(double seconds) {
    int64_t s = static_cast<int64_t>(seconds);
    int64_t h = s / 3600;
    int64_t m = (s % 3600) / 60;
    int64_t sec = s % 60;
    
    if (h > 0) {
        return Glib::ustring::sprintf("%ld:%02ld:%02ld", h, m, sec);
    }
    return Glib::ustring::sprintf("%ld:%02ld", m, sec);
}
```

---

## Reference Projects

### Celluloid (GNOME MPV Frontend)

**Repository**: [github.com/celluloid-player/celluloid](https://github.com/celluloid-player/celluloid)
**Language**: C
**Framework**: GTK4 + libadwaita
**Backend**: libmpv

Celluloid (formerly GNOME MPV) is the most mature GTK4/libadwaita frontend for mpv. It's an excellent reference for:
- libmpv integration patterns
- GTK4 window management
- MPRIS2 media controls
- Keyboard shortcuts

### Cine (Python GTK4 MPV Player)

**Repository**: [github.com/cine-app/cine](https://github.com/cine-app/cine)
**Language**: Python
**Framework**: GTK4 + libadwaita
**Backend**: libmpv

Modern Python implementation with clean code structure.

### Clapper (GJS/Gtk4 Player)

**Repository**: [github.com/Rafostar/clapper](https://github.com/rafostar/clapper)
**Language**: GJS (JavaScript)
**Framework**: GTK4 + GStreamer
**Backend**: GStreamer (not libmpv)

Uses GStreamer instead of libmpv, good for GStreamer-based approaches.

### mpv-examples (Official)

**Repository**: [github.com/mpv-player/mpv-examples](https://github.com/mpv-player/mpv-examples)
**Examples**:
- `libmpv/simple/` - Minimal C example
- `libmpv/sdl/` - SDL2 + OpenGL integration
- `libmpv/opengl/` - OpenGL rendering
- `libmpv/gtk/` - GTK plugin example

### GNOME Sushi (Video Preview)

**Repository**: [gitlab.gnome.org/GNOME/sushi](https://gitlab.gnome.org/GNOME/sushi)
**Language**: C
**Framework**: GTK4 + Clutter
**Backend**: GStreamer

GNOME's video previewer - good for understanding thumbnail/preview generation.

---

## Useful Links

### Documentation

| Resource | URL |
|----------|-----|
| mpv Manual | [mpv.io/manual/stable](https://mpv.io/manual/stable) |
| libmpv API | [github.com/mpv-player/mpv/blob/master/libmpv/client.h](https://github.com/mpv-player/mpv/blob/master/libmpv/client.h) |
| mpv-examples | [github.com/mpv-player/mpv-examples](https://github.com/mpv-player/mpv-examples) |
| GTK4 Video | [docs.gtk.org/gtk4/class.Video.html](https://docs.gtk.org/gtk4/class.Video.html) |
| gtk4paintablesink | [gstreamer.freedesktop.org/documentation/gtk4](https://gstreamer.freedesktop.org/documentation/gtk4) |
| GStreamer Player | [gnome.pages.gitlab.gnome.org/gstreamermm](https://gnome.pages.gitlab.gnome.org/gstreamermm) |

### GitHub Projects

| Project | Repository | Notes |
|---------|-------------|-------|
| Celluloid | [github.com/celluloid-player/celluloid](https://github.com/celluloid-player/celluloid) | GTK4 + libmpv |
| Cine | [github.com/cine-app/cine](https://github.com/cine-app/cine) | Python GTK4 + mpv |
| Clapper | [github.com/rafostar/clapper](https://github.com/rafostar/clapper) | GJS + GStreamer |
| mpv | [github.com/mpv-player/mpv](https://github.com/mpv-player/mpv) | mpv itself |

### Key mpv Options for Embedding

```bash
# Essential options for embedded player
vo=gpu              # GPU rendering
hwdec=auto          # Hardware decoding
terminal=no         # Disable terminal output
input-cursor=no     # Don't track cursor
cursor-autohide=no  # Don't auto-hide cursor
osc=no              # Disable on-screen controls (we'll add our own)
```

---

## Dependencies for Lucid

### System Dependencies (Arch Linux)

```bash
sudo pacman -S mpv libmpv gtk4 gtk4-media-gstreamer
```

### Alternative: Build libmpv from Source

```bash
git clone https://github.com/mpv-player/mpv.git
cd mpv
./bootstrap.py
./waf configure --enable-libmpv-static
./waf build
sudo ./waf install
```

---

## Implementation Checklist

For Lucid's video player, implement:

1. **MpvPlayer class** (embed libmpv in GtkWidget)
2. **VideoPlayerPage** (player + controls UI)
3. **Seek integration** (jump to timestamp from search)
4. **Thumbnail preview** (GtkVideo for hover previews)
5. **External player fallback** (open in mpv if libmpv unavailable)

---

*Document generated for Lucid project*
