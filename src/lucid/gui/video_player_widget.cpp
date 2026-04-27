// video_player_widget.cpp - Lucid Video Player Widget Implementation
// Uses GtkVideo with GtkMediaFile for built-in GTK4 video playback
#include "video_player_widget.hpp"
#include "../logging.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>

namespace lucid {

VideoPlayerWidget::VideoPlayerWidget()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 0) {
    
    // Setup UI
    setup_ui();
    
    LUCID_DEBUG("[VideoPlayer] Widget created");
}

VideoPlayerWidget::~VideoPlayerWidget() {
    if (m_tick_source_id > 0) {
        g_source_remove(m_tick_source_id);
        m_tick_source_id = 0;
    }
    if (m_media_stream) {
        g_object_unref(m_media_stream);
        m_media_stream = nullptr;
    }
}

void VideoPlayerWidget::setup_ui() {
    // Video display area using GtkVideo
    m_video = gtk_video_new();
    gtk_widget_set_hexpand(m_video, TRUE);
    gtk_widget_set_vexpand(m_video, TRUE);
    gtk_widget_set_size_request(m_video, 640, 360);
    gtk_box_append(GTK_BOX(gobj()), m_video);
    
    // Controls box
    m_controls_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(m_controls_box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(m_controls_box, 8);
    gtk_box_append(GTK_BOX(gobj()), m_controls_box);
    
    // Play/Pause button
    m_play_btn = gtk_button_new_from_icon_name("media-playback-start-symbolic");
    gtk_widget_set_size_request(m_play_btn, 40, 40);
    gtk_box_append(GTK_BOX(m_controls_box), m_play_btn);
    g_signal_connect_swapped(m_play_btn, "clicked", G_CALLBACK(+[](VideoPlayerWidget* p) {
        p->on_play_clicked();
    }), this);
    
    // Skip backward button
    m_skip_back_btn = gtk_button_new_from_icon_name("media-seek-backward-symbolic");
    gtk_widget_set_size_request(m_skip_back_btn, 30, 30);
    gtk_box_append(GTK_BOX(m_controls_box), m_skip_back_btn);
    g_signal_connect_swapped(m_skip_back_btn, "clicked", G_CALLBACK(+[](VideoPlayerWidget* p) {
        p->on_skip_back_clicked();
    }), this);
    
    // Skip forward button
    m_skip_forward_btn = gtk_button_new_from_icon_name("media-seek-forward-symbolic");
    gtk_widget_set_size_request(m_skip_forward_btn, 30, 30);
    gtk_box_append(GTK_BOX(m_controls_box), m_skip_forward_btn);
    g_signal_connect_swapped(m_skip_forward_btn, "clicked", G_CALLBACK(+[](VideoPlayerWidget* p) {
        p->on_skip_forward_clicked();
    }), this);
    
    // Time label
    m_time_label = gtk_label_new("00:00 / 00:00");
    gtk_widget_set_margin_start(m_time_label, 12);
    gtk_widget_set_margin_end(m_time_label, 12);
    gtk_box_append(GTK_BOX(m_controls_box), m_time_label);
    
    // Seek scale
    m_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_widget_set_size_request(m_scale, 200, -1);
    gtk_scale_set_draw_value(GTK_SCALE(m_scale), FALSE);
    gtk_box_append(GTK_BOX(m_controls_box), m_scale);
    g_signal_connect(m_scale, "value-changed", G_CALLBACK(+[](GtkScale* scale, gpointer data) {
        auto* p = static_cast<VideoPlayerWidget*>(data);
        p->on_scale_value_changed(gtk_range_get_value(GTK_RANGE(scale)));
    }), this);
    
    // Volume button
    m_volume_btn = gtk_button_new_from_icon_name("audio-volume-high-symbolic");
    gtk_widget_set_size_request(m_volume_btn, 30, 30);
    gtk_box_append(GTK_BOX(m_controls_box), m_volume_btn);
    g_signal_connect_swapped(m_volume_btn, "clicked", G_CALLBACK(+[](VideoPlayerWidget* p) {
        p->on_volume_clicked();
    }), this);
}

void VideoPlayerWidget::load_video(const std::string& path) {
    if (m_state == PlaybackState::PLAYING) {
        pause();
    }
    
    set_state(PlaybackState::LOADING);
    m_current_path = path;
    
    // Clean up existing media stream
    if (m_media_stream) {
        g_object_unref(m_media_stream);
        m_media_stream = nullptr;
    }
    
    // Create media stream from path (gtk_media_file_new_for_filename returns GtkMediaStream*)
    m_media_stream = gtk_media_file_new_for_filename(path.c_str());
    
    if (!m_media_stream) {
        LUCID_ERROR("[VideoPlayer] Failed to create media stream for: %s", path.c_str());
        set_state(PlaybackState::ERROR);
        return;
    }
    
    // Set the media stream on the video widget
    gtk_video_set_media_stream(GTK_VIDEO(m_video), m_media_stream);
    
    // The prepared state will be detected in the position timer
    start_position_timer();
    
    LUCID_DEBUG("[VideoPlayer] Video loading: %s", path.c_str());
}

void VideoPlayerWidget::unload() {
    stop();
    
    if (m_media_stream) {
        g_object_unref(m_media_stream);
        m_media_stream = nullptr;
    }
    
    gtk_video_set_media_stream(GTK_VIDEO(m_video), nullptr);
    
    m_current_path.clear();
    m_duration_ms = 0;
    m_position_ms = 0;
    set_state(PlaybackState::IDLE);
    update_time_label();
}

void VideoPlayerWidget::play() {
    if (!m_media_stream || m_state == PlaybackState::ERROR) return;
    
    // Reset position when playing after reaching end
    if (m_state == PlaybackState::ENDED) {
        LUCID_DEBUG("[VideoPlayer] Restarting from ENDED state, resetting position");
        m_position_ms = 0;
    }
    
    gtk_media_stream_play(m_media_stream);
    set_state(PlaybackState::PLAYING);
    start_position_timer();
    
    LUCID_DEBUG("[VideoPlayer] Playing, state=%d, tick_id=%u", (int)m_state, m_tick_source_id);
}

void VideoPlayerWidget::pause() {
    if (!m_media_stream) return;
    
    gtk_media_stream_pause(m_media_stream);
    set_state(PlaybackState::PAUSED);
    
    LUCID_DEBUG("[VideoPlayer] Paused");
}

void VideoPlayerWidget::toggle_play_pause() {
    if (!m_media_stream) return;
    
    if (gtk_media_stream_get_playing(m_media_stream)) {
        pause();
    } else {
        play();
    }
}

void VideoPlayerWidget::stop() {
    if (!m_media_stream) return;
    
    gtk_media_stream_pause(m_media_stream);
    seek_to(0);
    set_state(PlaybackState::READY);
}

void VideoPlayerWidget::seek_to(int64_t position_ms) {
    if (!m_media_stream || m_duration_ms <= 0) return;
    
    // Clamp position
    position_ms = std::max<int64_t>(0, std::min<int64_t>(position_ms, m_duration_ms));
    m_position_ms = position_ms;
    
    // Seek in media stream (seek takes microseconds)
    gint64 timestamp_us = position_ms * 1000;
    gtk_media_stream_seek(m_media_stream, timestamp_us);
    
    update_time_label();
    
    if (m_position_callback) {
        m_position_callback(m_position_ms);
    }
    
    LUCID_DEBUG("[VideoPlayer] Seeked to: %ld ms", position_ms);
}

void VideoPlayerWidget::set_volume(double volume) {
    m_volume = std::max(0.0, std::min(1.0, volume));
    m_muted = false;
    
    if (m_media_stream) {
        gtk_media_stream_set_volume(m_media_stream, m_volume);
    }
    
    update_volume_icon();
}

void VideoPlayerWidget::toggle_mute() {
    if (m_muted) {
        set_volume(m_volume > 0 ? m_volume : 1.0);
        m_muted = false;
    } else {
        if (m_media_stream) {
            gtk_media_stream_set_volume(m_media_stream, 0.0);
        }
        m_muted = true;
        gtk_button_set_icon_name(GTK_BUTTON(m_volume_btn), "audio-volume-muted-symbolic");
    }
}

void VideoPlayerWidget::skip_forward(int64_t delta_ms) {
    seek_to(m_position_ms + delta_ms);
}

void VideoPlayerWidget::skip_backward(int64_t delta_ms) {
    seek_to(m_position_ms - delta_ms);
}

void VideoPlayerWidget::set_state(PlaybackState state) {
    if (m_state == state) return;
    
    m_state = state;
    update_play_button_icon();
    
    if (m_state_callback) {
        m_state_callback(m_state);
    }
}

void VideoPlayerWidget::update_time_label() {
    if (!m_time_label) return;
    
    auto format_time = [](int64_t ms) -> std::string {
        int64_t total_seconds = ms / 1000;
        int64_t minutes = total_seconds / 60;
        int64_t seconds = total_seconds % 60;
        
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << minutes << ":"
            << std::setw(2) << seconds;
        return oss.str();
    };
    
    std::string text = format_time(m_position_ms) + " / " + format_time(m_duration_ms);
    gtk_label_set_text(GTK_LABEL(m_time_label), text.c_str());
    
    // Update scale without triggering callback
    if (m_scale && m_duration_ms > 0) {
        m_position_updating = true;
        double value = (double)m_position_ms / (double)m_duration_ms * 100.0;
        gtk_range_set_value(GTK_RANGE(m_scale), value);
        m_position_updating = false;
    }
}

void VideoPlayerWidget::update_play_button_icon() {
    if (!m_play_btn) return;
    
    switch (m_state) {
        case PlaybackState::PLAYING:
            gtk_button_set_icon_name(GTK_BUTTON(m_play_btn), "media-playback-pause-symbolic");
            break;
        case PlaybackState::ENDED:
            gtk_button_set_icon_name(GTK_BUTTON(m_play_btn), "media-playback-start-symbolic");
            gtk_widget_set_sensitive(m_play_btn, TRUE);
            break;
        default:
            gtk_button_set_icon_name(GTK_BUTTON(m_play_btn), "media-playback-start-symbolic");
            break;
    }
}

void VideoPlayerWidget::update_volume_icon() {
    if (!m_volume_btn) return;
    
    if (m_muted || m_volume <= 0) {
        gtk_button_set_icon_name(GTK_BUTTON(m_volume_btn), "audio-volume-muted-symbolic");
    } else if (m_volume > 0.5) {
        gtk_button_set_icon_name(GTK_BUTTON(m_volume_btn), "audio-volume-high-symbolic");
    } else {
        gtk_button_set_icon_name(GTK_BUTTON(m_volume_btn), "audio-volume-low-symbolic");
    }
}

void VideoPlayerWidget::start_position_timer() {
    // Remove existing timer if any
    if (m_tick_source_id > 0) {
        g_source_remove(m_tick_source_id);
        m_tick_source_id = 0;  // Reset ID after removal
    }
    m_tick_source_id = g_timeout_add(100, on_position_timer, this);
    LUCID_DEBUG("[VideoPlayer] Position timer started, source_id=%u", m_tick_source_id);
}

gboolean VideoPlayerWidget::on_position_timer(gpointer data) {
    auto* player = static_cast<VideoPlayerWidget*>(data);
    
    if (!player->m_media_stream) {
        return FALSE;
    }
    
    // Check if the media is prepared
    if (!gtk_media_stream_is_prepared(player->m_media_stream)) {
        return TRUE;  // Continue waiting
    }
    
    // Check for error
    const GError* error = gtk_media_stream_get_error(player->m_media_stream);
    if (error) {
        LUCID_ERROR("[VideoPlayer] Media stream error: %s", error->message);
        player->set_state(PlaybackState::ERROR);
        if (player->m_error_callback) {
            player->m_error_callback(error->message);
        }
        return FALSE;  // Stop timer
    }
    
    // Get duration if not already set
    if (player->m_duration_ms <= 0) {
        player->m_duration_ms = gtk_media_stream_get_duration(player->m_media_stream) / 1000;  // Convert to ms
        player->update_time_label();
        if (player->m_duration_callback) {
            player->m_duration_callback(player->m_duration_ms);
        }
        // Update state to ready
        if (player->m_state == PlaybackState::LOADING) {
            player->set_state(PlaybackState::READY);
        }
        LUCID_DEBUG("[VideoPlayer] Media prepared, duration: %ld ms", player->m_duration_ms);
    }
    
    // Check if ended (only stop timer if we're currently playing, not if we just hit play after end)
    if (gtk_media_stream_get_ended(player->m_media_stream) && 
        player->m_state == PlaybackState::PLAYING) {
        if (player->m_state != PlaybackState::ENDED) {
            player->set_state(PlaybackState::ENDED);
            if (player->m_ended_callback) {
                player->m_ended_callback();
            }
        }
        player->m_tick_source_id = 0;  // Clear so next play() starts fresh timer
        return FALSE;  // Stop timer until play again
    }
    
    // Update position (timestamp returns microseconds)
    int64_t timestamp = gtk_media_stream_get_timestamp(player->m_media_stream);
    if (timestamp > 0) {
        int64_t new_pos_ms = timestamp / 1000;
        
        // Only update and callback if position actually changed
        if (new_pos_ms != player->m_position_ms) {
            player->m_position_ms = new_pos_ms;
            player->update_time_label();
            
            LUCID_DEBUG("[VideoPlayer] Position update: %ld ms", player->m_position_ms);
            
            if (player->m_position_callback) {
                player->m_position_callback(player->m_position_ms);
            }
        }
    } else {
        // No timestamp yet - media may still be loading
        // Don't stop the timer, just continue waiting
        LUCID_DEBUG("[VideoPlayer] Position timer tick, no timestamp yet");
    }
    
    return TRUE;  // Continue timer
}

// Signal handlers
void VideoPlayerWidget::on_play_clicked() {
    toggle_play_pause();
}

void VideoPlayerWidget::on_skip_back_clicked() {
    skip_backward();
}

void VideoPlayerWidget::on_skip_forward_clicked() {
    skip_forward();
}

void VideoPlayerWidget::on_volume_clicked() {
    toggle_mute();
}

void VideoPlayerWidget::on_scale_value_changed(double value) {
    if (m_position_updating) return;
    
    int64_t new_position = (int64_t)(value / 100.0 * m_duration_ms);
    seek_to(new_position);
}

} // namespace lucid