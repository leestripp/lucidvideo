// video_player_widget.hpp - Lucid Video Player Widget
#pragma once

#include <gtkmm.h>
#include <string>
#include <functional>

namespace lucid {

// Video playback state
enum class PlaybackState {
    IDLE,       // No video loaded
    LOADING,    // Video loading
    READY,      // Video ready to play
    PLAYING,    // Video playing
    PAUSED,     // Video paused
    ENDED,      // Video ended
    ERROR       // Error occurred
};

// Video player widget using GtkVideo (built-in GTK4 video playback)
class VideoPlayerWidget : public Gtk::Box {
public:
    VideoPlayerWidget();
    ~VideoPlayerWidget() override;

    // Video loading
    void load_video(const std::string& path);
    void unload();
    
    // Playback control
    void play();
    void pause();
    void toggle_play_pause();
    void stop();
    
    // Seeking
    void seek_to(int64_t position_ms);
    int64_t get_position() const { return m_position_ms; }
    int64_t get_duration() const { return m_duration_ms; }
    
    // Volume
    void set_volume(double volume);  // 0.0 to 1.0
    double get_volume() const { return m_volume; }
    void toggle_mute();
    
    // Skip forward/backward
    void skip_forward(int64_t delta_ms = 10000);  // Default 10 seconds
    void skip_backward(int64_t delta_ms = 10000);  // Default 10 seconds
    
    // State queries
    PlaybackState get_state() const { return m_state; }
    bool is_playing() const { return m_state == PlaybackState::PLAYING; }
    bool is_loaded() const { return m_state != PlaybackState::IDLE && m_state != PlaybackState::ERROR; }
    
    // Callbacks
    using PositionCallback = std::function<void(int64_t position_ms)>;
    using DurationCallback = std::function<void(int64_t duration_ms)>;
    using StateCallback = std::function<void(PlaybackState state)>;
    using EndedCallback = std::function<void()>;
    using ErrorCallback = std::function<void(const std::string& error)>;
    
    void set_position_callback(PositionCallback callback) { m_position_callback = std::move(callback); }
    void set_duration_callback(DurationCallback callback) { m_duration_callback = std::move(callback); }
    void set_state_callback(StateCallback callback) { m_state_callback = std::move(callback); }
    void set_ended_callback(EndedCallback callback) { m_ended_callback = std::move(callback); }
    void set_error_callback(ErrorCallback callback) { m_error_callback = std::move(callback); }
    
    // GTK widget access
    GtkWidget* get_video_widget() const { return m_video; }

private:
    void setup_ui();
    void update_time_label();
    void update_play_button_icon();
    void update_volume_icon();
    void set_state(PlaybackState state);
    void start_position_timer();
    
    // Signal handlers
    void on_play_clicked();
    void on_skip_back_clicked();
    void on_skip_forward_clicked();
    void on_volume_clicked();
    void on_scale_value_changed(double value);
    
    // Timer callback
    static gboolean on_position_timer(gpointer data);

    // GTK widgets
    GtkWidget* m_video = nullptr;
    GtkWidget* m_controls_box = nullptr;
    GtkWidget* m_play_btn = nullptr;
    GtkWidget* m_skip_back_btn = nullptr;
    GtkWidget* m_skip_forward_btn = nullptr;
    GtkWidget* m_time_label = nullptr;
    GtkWidget* m_scale = nullptr;
    GtkWidget* m_volume_btn = nullptr;
    
    // Media stream
    GtkMediaStream* m_media_stream = nullptr;
    
    // State
    PlaybackState m_state = PlaybackState::IDLE;
    int64_t m_duration_ms = 0;
    int64_t m_position_ms = 0;
    double m_volume = 1.0;
    bool m_muted = false;
    bool m_position_updating = false;
    guint m_tick_source_id = 0;
    std::string m_current_path;
    
    // Callbacks
    PositionCallback m_position_callback;
    DurationCallback m_duration_callback;
    StateCallback m_state_callback;
    EndedCallback m_ended_callback;
    ErrorCallback m_error_callback;
};

} // namespace lucid