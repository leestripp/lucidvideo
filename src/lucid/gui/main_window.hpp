// main_window.hpp - Lucid Main Window
#pragma once

#include <gtkmm.h>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <atomic>
#include <mutex>

#include "../model/database.hpp"
#include "timeline_widget.hpp"
#include "video_player_widget.hpp"
#include "marker_panel.hpp"  // Renamed from TranscriptionPanel
#include "settings_page.hpp"  // For ThumbnailSize enum
#include "chat_panel.hpp"
#include "pagination_widget.hpp"
#include "../inference/llm_engine.hpp"
#include "../inference/chat_session.hpp"
#include "../search/llm_query.hpp"
#include "../chat/chat_context.hpp"

namespace lucid {

class Database;
class VideoIndexer;
class SemanticSearch;
class BackgroundJobManager;
class QueryParser;
class SettingsPage;
class ModelHubPage;
class VideoList;
class SearchResultsPanel;
class AnalysisQueuePanel;
class TimelineWidget;
class VideoPlayerWidget;
class MarkerPanel;  // Renamed from TranscriptionPanel
class ChatPanel;
class LLMEngine;
class ChatSession;
class ChatContext;
class PaginationWidget;  // Forward declaration for pagination
struct SearchResult;

// Navigation pages
enum class Page {
    HOME,           // Home page (welcome when 0 videos)
    VIDEO_LIST,     // Video list with pagination
    SEARCH,         // Search results page
    VIDEO_DETAIL,   // Video player with timeline
    ANALYSIS_QUEUE, // Video analysis queue panel
    SETTINGS,       // Settings page
    MODEL_HUB,      // Model download page
    CHAT            // Chat panel for natural language queries
};

// Keyboard shortcut action types
enum class ShortcutAction {
    ADD_FOLDER,
    FOCUS_SEARCH,
    REFRESH,
    GO_HOME,
    GO_SETTINGS,
    GO_MODEL_HUB,
    TOGGLE_SIDEBAR,
    QUIT
};

// Video processing state tracking
struct VideoState {
    std::string id;      // Video ID (matches database)
    std::string path;
    enum class Status { PENDING, EXTRACTING_AUDIO, TRANSCRIBING, COMPLETED, FAILED } status = Status::PENDING;
    float progress = 0.0f;
    std::string status_message;
};

class MainWindow : public Gtk::ApplicationWindow {
public:
    static Glib::RefPtr<MainWindow> create();
    ~MainWindow();
    
    // Friend classes for UI navigation
    friend class SettingsPage;

    void load_video(const Glib::ustring& path);
    
    // Navigation (public for control API access)
    void show_page(Page page);
    std::string get_current_page_name() const;
    Page get_current_page() const { return m_current_page; }
    void show_home();
    void show_video_list();
    void show_search();
    void show_video_detail(const std::string& video_id);
    void show_video_detail_with_seek(const std::string& video_id, int64_t position_ms);
    void show_analysis_queue();
    void show_settings();
    void show_model_hub();
    void show_chat();
    void show_welcome_state();
    void load_video_detail_page(const std::string& video_id, int64_t seek_position_ms = -1);
    void display_search_results();
    void clear_search();
    void on_shortcut_refresh();
    
private:
    MainWindow();
    
    // Shutdown guard for detached threads
    std::atomic<bool> m_shutdown{false};
    mutable std::mutex m_shutdown_mutex;  // Protects shutdown state
    
    // Check if window is shutting down (for detached thread safety)
    bool is_shutting_down() const {
        std::lock_guard<std::mutex> lock(m_shutdown_mutex);
        return m_shutdown;
    }
    
    // Mark window as shutting down (call from destructor)
    void set_shutting_down() {
        std::lock_guard<std::mutex> lock(m_shutdown_mutex);
        m_shutdown = true;
    }
    
    // Pending seek position (for seeking to marker when loading video)
    int64_t m_pending_seek_ms = -1;
    
    void setup_ui();
    void setup_header();
    void setup_sidebar();
    void setup_content_area();
    void setup_keyboard_shortcuts();
    
    // Shortcut handlers
    void on_shortcut_add_folder();
    void on_shortcut_focus_search();
    void on_shortcut_toggle_sidebar();
    void on_shortcut_go_home();
    void on_shortcut_go_settings();
    void on_shortcut_go_model_hub();
    
    // LLM/Chat
    void ensure_llm_engine_ready();
    void on_chat_message_submitted(const std::string& message);
    
    // Database loading
    void load_videos_from_database();
    void load_videos_page(int page, int page_size);  // Load a specific page of videos
    
    // Video management
    void refresh_video_list();
    void scan_folder(const std::string& folder_path);
    void process_videos();
    GtkWidget* create_video_item(const std::string& video_id, const std::string& video_path, 
                                  const VideoState* state = nullptr, const std::string& thumbnail_path = "");
    GtkWidget* create_marker_item(const Marker& marker);  // For transcript search results
    GtkWidget* create_semantic_result_item(const SearchResult& result);  // For semantic embedding results
    
    // Search
    void perform_search(const std::string& query);
    void show_search_results(const std::vector<SearchResult>& results);
    
    // Status updates
    void update_status_bar();
    void update_video_grid();
    void refresh_timeline();  // Refresh timeline markers (called after Pass 2 completes)
    // 🔄 REMOVED: update_video_progress() - now handled by AnalysisQueuePanel
    
public:
    // Called by App after window is added to application
    void register_analysis_callbacks(Glib::RefPtr<class LucidApp> app);
    void reset_all_videos_to_pending();  // 🔄 NEW - for re-analysis
    
    // Signal handlers
    static void on_add_folder_clicked(GtkButton* button, gpointer data);
    static void on_home_clicked(GtkButton* button, gpointer data);
    static void on_search_clicked(GtkButton* button, gpointer data);     // 🔄 NEW
    static void on_video_detail_clicked(GtkButton* button, gpointer data);  // 🔄 NEW
    static void on_analysis_queue_clicked(GtkButton* button, gpointer data);  // 🔄 NEW
    static void on_settings_clicked(GtkButton* button, gpointer data);
    static void on_model_hub_clicked(GtkButton* button, gpointer data);
    static void on_chat_clicked(GtkButton* button, gpointer data);
    static void on_search_changed(GtkSearchEntry* entry, gpointer data);
    static void on_search_activate(GtkSearchEntry* entry, gpointer data);  // 🔄 NEW - search only on Enter
    static void on_sort_changed(GtkComboBox* combo, gpointer data);
    static void on_video_item_clicked(GtkButton* button, gpointer data);  // 🔄 NEW - for clicking video cards
    
    // Core components
    std::unique_ptr<Database> m_db;
    std::unique_ptr<VideoIndexer> m_indexer;
    std::unique_ptr<SemanticSearch> m_search;
    int m_search_context_size = 512;  // Default context size for search
    std::unique_ptr<BackgroundJobManager> m_job_manager;
    std::unique_ptr<QueryParser> m_query_parser;
    std::unique_ptr<LLMQueryEnhancer> m_llm_enhancer;
    
    // UI Components
    std::unique_ptr<SettingsPage> m_settings_page;
    std::unique_ptr<ModelHubPage> m_model_hub_page;
    std::unique_ptr<VideoList> m_video_grid;
    std::unique_ptr<SearchResultsPanel> m_search_panel;
    std::unique_ptr<AnalysisQueuePanel> m_analysis_queue_panel;  // 🔄 NEW
    std::unique_ptr<TimelineWidget> m_timeline_widget;  // 🔄 NEW - for video detail page
    std::unique_ptr<VideoPlayerWidget> m_video_player_widget;  // 🔄 NEW - for video playback
    std::unique_ptr<MarkerPanel> m_marker_panel;  // Renamed from TranscriptionPanel - displays all markers (audio + vision)
    std::unique_ptr<ChatPanel> m_chat_panel;
    std::unique_ptr<LLMEngine> m_llm_engine;
    std::unique_ptr<ChatSession> m_chat_session;
    std::unique_ptr<ChatContext> m_chat_context;  // 🔄 NEW - Video library context for chat
    
    // Pagination for video grid
    std::unique_ptr<PaginationWidget> m_video_pagination;
    int m_video_page = 0;
    int m_video_page_size = 50;
    int m_video_total = 0;
    
    // Pagination getters
    int get_video_page() const { return m_video_page; }
    int get_video_page_size() const { return m_video_page_size; }
    int get_video_total() const { return m_video_total; }
    int get_video_total_pages() const {
        if (m_video_total <= 0 || m_video_page_size <= 0) return 0;
        return (m_video_total + m_video_page_size - 1) / m_video_page_size;
    }
    
    // Pagination setters
    void set_video_page(int page);
    void set_video_page_size(int page_size);
    
    // UI elements
    GtkWidget* m_stack = nullptr;           // Stacked container for pages
    GtkWidget* m_search_entry = nullptr;
    GtkWidget* m_status_label = nullptr;
    GtkWidget* m_video_count_label = nullptr;
    GtkWidget* m_progress_bar = nullptr;
    GtkWidget* m_home_view = nullptr;
    GtkWidget* m_video_list = nullptr;       // List box for videos
    
    // Content area container
    GtkWidget* m_content_box = nullptr;
    
    // Sidebar buttons (for highlighting active page)
    GtkWidget* m_home_btn = nullptr;
    GtkWidget* m_search_btn = nullptr;        // 🔄 NEW - Search page button
    GtkWidget* m_analysis_queue_btn = nullptr;
    GtkWidget* m_settings_btn = nullptr;
    GtkWidget* m_model_hub_btn = nullptr;
    GtkWidget* m_chat_btn = nullptr;

    // Search page container
    GtkWidget* m_search_page = nullptr;      // 🔄 NEW - Search results page container

    // Stored search results (persist when navigating away)
    std::string m_stored_search_query;                                    // 🔄 NEW
    std::vector<SearchResult> m_stored_search_results;                     // 🔄 NEW
    std::vector<Marker> m_stored_marker_results;                           // 🔄 NEW
    std::vector<std::string> m_stored_video_matches;                     // 🔄 NEW
    std::vector<std::string> m_stored_video_ids;                         // 🔄 NEW
    
    // Job tracking: maps job_id to video index
    std::unordered_map<std::string, size_t> m_job_to_video;
    
    // Video frames indexed by video_id (for potential future click handling)
    std::unordered_map<std::string, GtkWidget*> m_video_frames;
    
    // 🔄 REMOVED: m_progress_bars - no longer needed (progress shown in queue panel)
    
    // Current state
    Page m_current_page = Page::HOME;
    std::string m_current_folder;
    std::string m_current_video_id;  // 🔄 NEW - for video detail page
    std::vector<std::string> m_videos;         // Video paths
    std::vector<std::string> m_video_ids;      // Video IDs (parallel to m_videos)
    std::string m_search_query;
    int m_pending_jobs = 0;
    float m_overall_progress = 0.0f;
    bool m_sidebar_visible = true;
    GtkWidget* m_sidebar = nullptr;  // For sidebar toggle
    GtkWidget* m_video_detail_page = nullptr;  // 🔄 NEW - video detail container
    std::vector<VideoState> m_video_states;
    ThumbnailSize m_thumbnail_size = ThumbnailSize::MEDIUM;  // Current thumbnail display size
    
    // Reference to app for pause/resume analysis
    Glib::RefPtr<class LucidApp> m_app;
};

} // namespace lucid
