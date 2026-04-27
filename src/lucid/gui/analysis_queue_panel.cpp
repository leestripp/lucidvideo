// analysis_queue_panel.cpp - Analysis Queue Panel Implementation
// Shows video analysis job queue, current job progress, and recent completions
// Supports 2-pass analysis: Pass 1 (thumbnails + whisper), Pass 2 (vision scan)

#include "analysis_queue_panel.hpp"
#include "../logging.hpp"

namespace lucid {

AnalysisQueuePanel::AnalysisQueuePanel() = default;

AnalysisQueuePanel::~AnalysisQueuePanel() = default;

void AnalysisQueuePanel::initialize(GtkWidget* /*parent*/) {
    create_ui();
}

void AnalysisQueuePanel::create_ui() {
    // Main container
    m_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(m_box, TRUE);
    gtk_widget_set_vexpand(m_box, TRUE);
    gtk_widget_set_margin_start(m_box, 12);
    gtk_widget_set_margin_end(m_box, 12);
    gtk_widget_set_margin_top(m_box, 12);
    gtk_widget_set_margin_bottom(m_box, 12);
    
    // ============================================
    // Header Section
    // ============================================
    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_bottom(header, 16);
    gtk_box_append(GTK_BOX(m_box), header);
    
    GtkWidget* title = gtk_label_new("Analysis Queue");
    gtk_label_set_markup(GTK_LABEL(title), "<span size='large' weight='bold'>Analysis Queue</span>");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(header), title);
    
    // Spacer
    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(header), spacer);
    
    // Stats display label - SIMPLIFIED: just show total count
    m_stats_label = gtk_label_new("0 videos");
    gtk_widget_set_tooltip_text(m_stats_label, "Total videos in library");
    gtk_box_append(GTK_BOX(header), m_stats_label);
    
    // ============================================
    // Overall Progress Bar
    // ============================================
    GtkWidget* progress_section = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_bottom(progress_section, 12);
    gtk_box_append(GTK_BOX(m_box), progress_section);
    
    m_overall_progress_label = gtk_label_new("0 of 0 videos analyzed");
    gtk_widget_set_halign(m_overall_progress_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(progress_section), m_overall_progress_label);
    
    m_overall_progress_bar = gtk_progress_bar_new();
    gtk_widget_set_halign(m_overall_progress_bar, GTK_ALIGN_FILL);
    gtk_box_append(GTK_BOX(progress_section), m_overall_progress_bar);
    
    // ============================================
    // Action Buttons Bar
    // ============================================
    GtkWidget* actions_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_bottom(actions_bar, 12);
    gtk_box_append(GTK_BOX(m_box), actions_bar);
    
    // Re-analyze All button
    m_reanalyze_all_btn = gtk_button_new_with_label("Re-analyze All");
    gtk_widget_set_tooltip_text(m_reanalyze_all_btn, "Reset all completed videos to pending for re-analysis");
    g_signal_connect_swapped(m_reanalyze_all_btn, "clicked", G_CALLBACK(+[](AnalysisQueuePanel* self) {
        if (self->m_reanalyze_all_callback) {
            self->m_reanalyze_all_callback();
        }
    }), this);
    gtk_box_append(GTK_BOX(actions_bar), m_reanalyze_all_btn);
    
    // Cancel button (only visible when processing)
    m_cancel_btn = gtk_button_new_with_label("Cancel");
    gtk_widget_set_tooltip_text(m_cancel_btn, "Cancel the current analysis job");
    gtk_widget_set_visible(m_cancel_btn, FALSE);
    g_signal_connect_swapped(m_cancel_btn, "clicked", G_CALLBACK(+[](AnalysisQueuePanel* self) {
        if (self->m_cancel_callback) {
            self->m_cancel_callback();
        }
    }), this);
    gtk_box_append(GTK_BOX(actions_bar), m_cancel_btn);
    
    // Spacer
    GtkWidget* action_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(action_spacer, TRUE);
    gtk_box_append(GTK_BOX(actions_bar), action_spacer);
    
    // ============================================
    // Scroll Container
    // ============================================
    m_scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(m_scrolled),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(m_scrolled), 300);
    gtk_widget_set_vexpand(m_scrolled, TRUE);
    gtk_box_append(GTK_BOX(m_box), m_scrolled);
    
    // Inner container
    GtkWidget* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(m_scrolled), content);
    
    // ============================================
    // Current Job Section - UPDATED for 2-pass
    // ============================================
    GtkWidget* current_section = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_box_append(GTK_BOX(content), current_section);
    
    GtkWidget* current_header = gtk_label_new("Currently Processing");
    gtk_label_set_markup(GTK_LABEL(current_header), "<b>Currently Processing</b>");
    gtk_widget_set_halign(current_header, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(current_section), current_header);
    
    // Current job box
    m_current_job_box = gtk_frame_new(nullptr);
    gtk_widget_set_margin_top(m_current_job_box, 4);
    gtk_box_append(GTK_BOX(current_section), m_current_job_box);
    
    GtkWidget* current_inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(current_inner, 12);
    gtk_widget_set_margin_end(current_inner, 12);
    gtk_widget_set_margin_top(current_inner, 12);
    gtk_widget_set_margin_bottom(current_inner, 12);
    gtk_frame_set_child(GTK_FRAME(m_current_job_box), current_inner);
    
    // Current job title
    m_current_title_label = gtk_label_new("No video being processed");
    gtk_label_set_ellipsize(GTK_LABEL(m_current_title_label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_halign(m_current_title_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(current_inner), m_current_title_label);
    
    // Current job phase
    m_current_phase_label = gtk_label_new("");
    gtk_widget_set_opacity(m_current_phase_label, 0.7);
    gtk_widget_set_halign(m_current_phase_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(current_inner), m_current_phase_label);
    
    // Progress bar
    m_current_progress_bar = gtk_progress_bar_new();
    gtk_widget_set_halign(m_current_progress_bar, GTK_ALIGN_FILL);
    gtk_box_append(GTK_BOX(current_inner), m_current_progress_bar);
    
    // ============================================
    // Pending Jobs Section
    // ============================================
    GtkWidget* pending_section = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_box_append(GTK_BOX(content), pending_section);
    
    GtkWidget* pending_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(pending_section), pending_header);
    
    GtkWidget* pending_label = gtk_label_new("Queue");
    gtk_label_set_markup(GTK_LABEL(pending_label), "<b>Queue</b>");
    gtk_widget_set_halign(pending_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(pending_header), pending_label);
    
    m_pending_count_label = gtk_label_new("(0 pending)");
    gtk_widget_set_opacity(m_pending_count_label, 0.6);
    gtk_box_append(GTK_BOX(pending_header), m_pending_count_label);
    
    // Pending list
    m_pending_list = gtk_list_box_new();
    gtk_widget_set_margin_top(m_pending_list, 4);
    gtk_list_box_set_show_separators(GTK_LIST_BOX(m_pending_list), TRUE);
    gtk_box_append(GTK_BOX(pending_section), m_pending_list);
    
    // ============================================
    // Recent Completions Section
    // ============================================
    GtkWidget* recent_section = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_box_append(GTK_BOX(content), recent_section);
    
    GtkWidget* recent_header = gtk_label_new("Recently Completed");
    gtk_label_set_markup(GTK_LABEL(recent_header), "<b>Recently Completed</b>");
    gtk_widget_set_halign(recent_header, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(recent_section), recent_header);
    
    // Recent list
    m_recent_list = gtk_list_box_new();
    gtk_widget_set_margin_top(m_recent_list, 4);
    gtk_list_box_set_show_separators(GTK_LIST_BOX(m_recent_list), TRUE);
    gtk_box_append(GTK_BOX(recent_section), m_recent_list);
    
    // Initially hide sections that are empty
    gtk_widget_set_visible(current_section, FALSE);
    gtk_widget_set_visible(pending_section, FALSE);
    gtk_widget_set_visible(recent_section, FALSE);
}

void AnalysisQueuePanel::set_title_lookup(std::function<std::string(const std::string&)> cb) {
    m_title_lookup = std::move(cb);
}

void AnalysisQueuePanel::set_reset_callback(std::function<void()> cb) {
    m_reset_callback = std::move(cb);
}

void AnalysisQueuePanel::set_reanalyze_all_callback(std::function<void()> cb) {
    m_reanalyze_all_callback = std::move(cb);
}

void AnalysisQueuePanel::set_clear_all_callback(std::function<void()> cb) {
    m_clear_all_callback = std::move(cb);
}

void AnalysisQueuePanel::set_cancel_callback(std::function<void()> cb) {
    m_cancel_callback = std::move(cb);
}

void AnalysisQueuePanel::set_remove_failed_callback(std::function<void()> cb) {
    m_remove_failed_callback = std::move(cb);
}

void AnalysisQueuePanel::set_stats_callback(std::function<QueueStats()> cb) {
    m_stats_callback = std::move(cb);
}

void AnalysisQueuePanel::set_processing(bool is_processing) {
    m_is_processing = is_processing;
    gtk_widget_set_visible(m_cancel_btn, is_processing);
}

void AnalysisQueuePanel::set_vision_progress_callback(
    std::function<void(int, int)> cb) {
    m_vision_progress_callback = std::move(cb);
}

void AnalysisQueuePanel::set_video_stats_callback(
    std::function<void(const std::string&, int&, int&, int&)> cb) {
    m_video_stats_callback = std::move(cb);
}

void AnalysisQueuePanel::update_stats(const QueueStats& stats) {
    // SIMPLIFIED: just show total video count
    char stats_text[64];
    snprintf(stats_text, sizeof(stats_text), "%d videos", stats.total_videos);
    gtk_label_set_text(GTK_LABEL(m_stats_label), stats_text);
    
    m_total_videos = stats.total_videos;
}

void AnalysisQueuePanel::update_overall_progress(int completed, int total) {
    m_completed_videos = completed;
    m_total_videos = total;
    
    char label_text[128];
    if (total > 0) {
        double fraction = (double)completed / total;
        snprintf(label_text, sizeof(label_text), "%d of %d videos fully analyzed (%.0f%%)",
                 completed, total, fraction * 100);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(m_overall_progress_bar), fraction);
    } else {
        snprintf(label_text, sizeof(label_text), "No videos");
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(m_overall_progress_bar), 0.0);
    }
    gtk_label_set_text(GTK_LABEL(m_overall_progress_label), label_text);
}

void AnalysisQueuePanel::refresh() {
    update_pending_list();
    clear_recent();
    
    // Also refresh stats if callback is available
    if (m_stats_callback) {
        update_stats(m_stats_callback());
    }
}

void AnalysisQueuePanel::set_pending_videos_callback(
    std::function<std::vector<std::pair<std::string, std::string>>()> cb) {
    m_pending_videos_callback = std::move(cb);
}

void AnalysisQueuePanel::update_pending_list() {
    // Clear existing pending items
    GtkListBoxRow* row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(m_pending_list), 0);
    while (row) {
        gtk_list_box_remove(GTK_LIST_BOX(m_pending_list), GTK_WIDGET(row));
        row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(m_pending_list), 0);
    }
    
    // Populate pending list using callback
    if (m_pending_videos_callback) {
        auto pending_videos = m_pending_videos_callback();
        for (const auto& [video_id, title] : pending_videos) {
            // Create simple pending item widget - just text
            GtkWidget* item = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            gtk_widget_set_margin_start(item, 8);
            gtk_widget_set_margin_end(item, 8);
            gtk_widget_set_margin_top(item, 4);
            gtk_widget_set_margin_bottom(item, 4);
            
            // Title only - simple text
            GtkWidget* label = gtk_label_new(title.c_str());
            gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
            gtk_widget_set_hexpand(label, TRUE);
            gtk_widget_set_halign(label, GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(item), label);
            
            gtk_list_box_append(GTK_LIST_BOX(m_pending_list), item);
        }
    }
}

void AnalysisQueuePanel::update_queue(int pending_count, int processing_count) {
    m_pending_count = pending_count;
    m_processing_count = processing_count;
    
    // Update pending count label
    char count_text[64];
    if (pending_count > 0) {
        snprintf(count_text, sizeof(count_text), "(%d pending)", pending_count);
    } else {
        snprintf(count_text, sizeof(count_text), "(%s)",
                 processing_count > 0 ? "processing" : "empty");
    }
    gtk_label_set_text(GTK_LABEL(m_pending_count_label), count_text);
    
    // Show/hide cancel button based on processing state
    gtk_widget_set_visible(m_cancel_btn, processing_count > 0);
    
    // Show/hide pending section based on content
    GtkWidget* parent = gtk_widget_get_parent(m_pending_list);
    if (parent) {
        gtk_widget_set_visible(parent, pending_count > 0);
    }
}

void AnalysisQueuePanel::on_progress(const std::string& video_id,
                                    PassState pass_state,
                                    const std::string& phase,
                                    float progress,
                                    int vision_analyzed,
                                    int vision_total) {
    m_current_video_id = video_id;
    m_current_pass_state = pass_state;
    m_current_phase = phase;
    m_current_progress = progress;
    m_vision_analyzed_count = vision_analyzed;
    m_vision_total_count = vision_total;
    
    // Get title if we have a lookup function
    m_current_video_title = get_title(video_id);
    if (m_current_video_title.empty()) {
        m_current_video_title = video_id;
    }
    
    update_current_job_display();
}

void AnalysisQueuePanel::on_progress(const std::string& video_id,
                                    const std::string& pass_state_str,
                                    const std::string& phase,
                                    float progress,
                                    int vision_analyzed,
                                    int vision_total) {
    PassState pass_state = pass_state_from_string(pass_state_str);
    on_progress(video_id, pass_state, phase, progress, vision_analyzed, vision_total);
}

void AnalysisQueuePanel::update_current_job_display() {
    // Update title
    gtk_label_set_text(GTK_LABEL(m_current_title_label), m_current_video_title.c_str());
    
    // Update phase label
    char phase_text[256];
    if (m_current_phase.empty()) {
        phase_text[0] = '\0';
    } else {
        snprintf(phase_text, sizeof(phase_text), "%s (%.0f%%)",
                 m_current_phase.c_str(), m_current_progress * 100);
    }
    gtk_label_set_text(GTK_LABEL(m_current_phase_label), phase_text);
    
    // Update progress bar
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(m_current_progress_bar), m_current_progress);
    
    // Show/hide current job section
    bool has_job = !m_current_video_id.empty();
    gtk_widget_set_visible(m_current_job_box, has_job);
    
    // Show/hide parent section
    GtkWidget* parent = gtk_widget_get_parent(m_current_job_box);
    if (parent) {
        gtk_widget_set_visible(parent, has_job);
    }
}

void AnalysisQueuePanel::on_complete(const std::string& video_id, bool success,
                                     const std::string& error) {
    LUCID_INFO("[AnalysisQueuePanel] on_complete called: video_id=%s, success=%d",
               video_id.c_str(), success);
    
    QueueJob job;
    job.video_id = video_id;
    job.pass_state = success ? PassState::PASS2_COMPLETE : PassState::FAILED;
    job.error_message = error;
    job.thumbnail_count = m_thumbnail_count;
    job.audio_marker_count = m_audio_marker_count;
    job.vision_marker_count = m_vision_marker_count;
    
    // Get title
    job.video_title = get_title(video_id);
    if (job.video_title.empty()) {
        job.video_title = video_id;
    }
    
    LUCID_INFO("[AnalysisQueuePanel] on_complete: title=%s", job.video_title.c_str());
    
    // If this was the current job, clear it
    if (video_id == m_current_video_id) {
        m_current_video_id.clear();
        m_current_video_title.clear();
        m_current_pass_state = PassState::PENDING;
        m_current_phase.clear();
        m_current_progress = 0.0f;
        m_vision_analyzed_count = 0;
        m_vision_total_count = 0;
        m_thumbnail_count = 0;
        m_audio_marker_count = 0;
        m_vision_marker_count = 0;
        update_current_job_display();
    }
    
    // Add to recent completions
    LUCID_INFO("[AnalysisQueuePanel] on_complete: adding to recent");
    add_to_recent(job);
    
    // Refresh overall progress
    LUCID_INFO("[AnalysisQueuePanel] on_complete: refreshing stats");
    if (m_stats_callback) {
        auto stats = m_stats_callback();
        update_overall_progress(stats.completed, stats.total_videos);
    }
    
    // Refresh the pending list to show updated state
    LUCID_INFO("[AnalysisQueuePanel] on_complete: calling update_pending_list");
    update_pending_list();
    LUCID_INFO("[AnalysisQueuePanel] on_complete: done");
}

void AnalysisQueuePanel::add_to_recent(const QueueJob& job) {
    // Create a copy we can modify
    QueueJob job_copy = job;
    
    // If we have a video stats callback, get actual counts from database
    if (m_video_stats_callback) {
        int thumb_count = 0, marker_count = 0, vision_count = 0;
        m_video_stats_callback(job.video_id, thumb_count, marker_count, vision_count);
        job_copy.thumbnail_count = thumb_count;
        job_copy.audio_marker_count = marker_count;
        job_copy.vision_marker_count = vision_count;
        LUCID_DEBUG("[AnalysisQueuePanel] add_to_recent: fetched stats for %s: thumb=%d, markers=%d, vision=%d",
                     job.video_id.c_str(), thumb_count, marker_count, vision_count);
    }
    
    // Add to front
    m_recent_completions.insert(m_recent_completions.begin(), job_copy);
    
    // Keep max 5
    while (m_recent_completions.size() > 5) {
        m_recent_completions.pop_back();
    }
    
    // Update the list
    // Clear existing rows
    GtkListBoxRow* row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(m_recent_list), 0);
    while (row) {
        gtk_list_box_remove(GTK_LIST_BOX(m_recent_list), GTK_WIDGET(row));
        row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(m_recent_list), 0);
    }
    
    // Add items
    for (const auto& j : m_recent_completions) {
        GtkWidget* item = create_job_item(j);
        gtk_list_box_append(GTK_LIST_BOX(m_recent_list), item);
    }
    
    // Show/hide recent section
    GtkWidget* parent = gtk_widget_get_parent(m_recent_list);
    if (parent) {
        gtk_widget_set_visible(parent, !m_recent_completions.empty());
    }
}

GtkWidget* AnalysisQueuePanel::create_job_item(const QueueJob& job) {
    GtkWidget* item = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(item, 8);
    gtk_widget_set_margin_end(item, 8);
    gtk_widget_set_margin_top(item, 4);
    gtk_widget_set_margin_bottom(item, 4);
    
    // Simple icon: ✓ for success, ✗ for failed
    const char* icon_markup = nullptr;
    if (job.pass_state == PassState::PASS2_COMPLETE) {
        icon_markup = "✓";
    } else if (job.pass_state == PassState::FAILED) {
        icon_markup = "✗";
    }
    
    if (icon_markup) {
        GtkWidget* icon = gtk_label_new(nullptr);
        gtk_label_set_markup(GTK_LABEL(icon), icon_markup);
        gtk_widget_set_halign(icon, GTK_ALIGN_CENTER);
        gtk_widget_set_size_request(icon, 24, -1);
        gtk_box_append(GTK_BOX(item), icon);
    }
    
    // Title only
    GtkWidget* title = gtk_label_new(job.video_title.c_str());
    gtk_label_set_ellipsize(GTK_LABEL(title), PANGO_ELLIPSIZE_END);
    gtk_widget_set_hexpand(title, TRUE);
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(item), title);
    
    return item;
}

void AnalysisQueuePanel::clear_recent() {
    m_recent_completions.clear();
    
    // Clear the list
    GtkListBoxRow* row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(m_recent_list), 0);
    while (row) {
        gtk_list_box_remove(GTK_LIST_BOX(m_recent_list), GTK_WIDGET(row));
        row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(m_recent_list), 0);
    }
    
    // Hide section
    GtkWidget* parent = gtk_widget_get_parent(m_recent_list);
    if (parent) {
        gtk_widget_set_visible(parent, FALSE);
    }
}

} // namespace lucid
