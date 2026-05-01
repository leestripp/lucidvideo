// main_window.cpp - Lucid Main Window Implementation
// Video library browser with folder scanning, semantic search, and navigation
#include "main_window.hpp"
#include "folder_browser.hpp"
#include "settings_page.hpp"
#include "model_hub_page.hpp"
#include "video_list.hpp"
#include "search_results_panel.hpp"
#include "analysis_queue_panel.hpp"
#include "chat_panel.hpp"
#include "pagination_widget.hpp"
#include "../app.hpp"
#include "../workers/background_job.hpp"
#include "../search/semantic_search.hpp"
#include "../search/query_parser.hpp"
#include "../search/llm_query.hpp"
#include "../model/database.hpp"
#include "../model/video_indexer.hpp"
#include "../model/video_analysis_worker.hpp"
#include "../logging.hpp"

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <thread>
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <tuple>
#include <functional>

namespace lucid {

MainWindow::MainWindow()
    : Gtk::ApplicationWindow() {
    LUCID_DEBUG("[MainWindow] Constructor START");
    
    set_title("Lucid - Video Library");
    property_default_width() = 1200;
    property_default_height() = 800;
    
    LUCID_DEBUG("[MainWindow] Creating Database...");
    // Initialize core components
    std::string db_path = Glib::build_filename(Glib::get_user_data_dir(), "lucid", "lucid.db");
    m_db = std::make_unique<Database>(db_path);
    LUCID_DEBUG("[MainWindow] Database created");
    
    LUCID_DEBUG("[MainWindow] Creating VideoIndexer...");
    m_indexer = std::make_unique<VideoIndexer>(*m_db);
    LUCID_DEBUG("[MainWindow] Creating SemanticSearch...");
    m_search = std::make_unique<SemanticSearch>(*m_db);

    // Initialize semantic search with embedding model from ModelManager
    if (auto embed_path = ModelManager::instance().get_selected_embedding_path()) {
        // Context size will be loaded from settings after settings_page is created
        m_search_context_size = 512;  // Default, will be updated when settings load
        
        if (!m_search->initialize(*embed_path, m_search_context_size)) {
            LUCID_WARN("[MainWindow] Failed to initialize semantic search with model: %s", embed_path->c_str());
        } else {
            LUCID_INFO("[MainWindow] Semantic search initialized with model: %s (ctx=%d)", 
                       embed_path->c_str(), m_search_context_size);
        }
    } else {
        LUCID_WARN("[MainWindow] No embedding model selected - semantic search disabled");
    }
    
    // Force flush stderr to ensure we see this
    fprintf(stderr, "[MainWindow] About to create BackgroundJobManager...\n");
    fflush(stderr);
    
    m_job_manager = std::make_unique<BackgroundJobManager>();
    LUCID_INFO("[MainWindow] BackgroundJobManager created");
    
    // Force flush stderr
    fprintf(stderr, "[MainWindow] About to create QueryParser...\n");
    fflush(stderr);
    
    // Initialize query parser and LLM enhancer
    LUCID_INFO("[MainWindow] Creating QueryParser...");
    m_query_parser = std::make_unique<QueryParser>();
    LUCID_INFO("[MainWindow] QueryParser created");
    
    // Force flush stderr
    fprintf(stderr, "[MainWindow] About to create LLMQueryEnhancer...\n");
    fflush(stderr);
    
    LUCID_INFO("[MainWindow] Creating LLMQueryEnhancer...");
    m_llm_enhancer = std::make_unique<LLMQueryEnhancer>();
    LUCID_INFO("[MainWindow] LLMQueryEnhancer created");
    
    // Force flush stderr
    fprintf(stderr, "[MainWindow] About to create SettingsPage...\n");
    fflush(stderr);
    
    LUCID_INFO("[MainWindow] Creating SettingsPage...");
    m_settings_page = std::make_unique<SettingsPage>();
    LUCID_INFO("[MainWindow] SettingsPage created");
    
    LUCID_INFO("[MainWindow] Creating ModelHubPage...");
    m_model_hub_page = std::make_unique<ModelHubPage>();
    LUCID_INFO("[MainWindow] ModelHubPage created");
    
    LUCID_INFO("[MainWindow] Creating VideoList...");
    m_video_grid = std::make_unique<VideoList>();
    LUCID_INFO("[MainWindow] VideoList created");
    
    LUCID_INFO("[MainWindow] Creating SearchResultsPanel...");
    m_search_panel = std::make_unique<SearchResultsPanel>();
    LUCID_INFO("[MainWindow] SearchResultsPanel created");
    
    LUCID_INFO("[MainWindow] Creating AnalysisQueuePanel...");
    m_analysis_queue_panel = std::make_unique<AnalysisQueuePanel>();
    LUCID_INFO("[MainWindow] AnalysisQueuePanel created");
    
    LUCID_INFO("[MainWindow] Creating TimelineWidget...");
    m_timeline_widget = std::make_unique<TimelineWidget>();
    LUCID_INFO("[MainWindow] TimelineWidget created");
    
    LUCID_INFO("[MainWindow] Creating VideoPlayerWidget...");
    m_video_player_widget = std::make_unique<VideoPlayerWidget>();
    LUCID_INFO("[MainWindow] VideoPlayerWidget created");
    
    LUCID_INFO("[MainWindow] Creating MarkerPanel...");
    m_marker_panel = std::make_unique<MarkerPanel>();
    LUCID_INFO("[MainWindow] MarkerPanel created");
    
    // Initialize LLM engine and chat session for chat panel
    LUCID_INFO("[MainWindow] Creating LLMEngine...");
    m_llm_engine = std::make_unique<LLMEngine>();
    LUCID_INFO("[MainWindow] LLMEngine created");
    
    LUCID_INFO("[MainWindow] Creating ChatSession...");
    m_chat_session = std::make_unique<ChatSession>();
    LUCID_INFO("[MainWindow] ChatSession created");
    
    // Initialize ChatContext for video library integration
    LUCID_INFO("[MainWindow] Creating ChatContext...");
    m_chat_context = std::make_unique<ChatContext>(m_db.get(), m_search.get());
    LUCID_INFO("[MainWindow] ChatContext created");
    
    LUCID_INFO("[MainWindow] Setting system message...");
    m_chat_session->set_engine(m_llm_engine.get());
    m_chat_session->set_system_message(
        "You are a helpful assistant for a video library application called Lucid. "
        "You can help users find videos, answer questions about their library, and provide information. "
        "Keep responses concise and helpful. "
        "When users ask about videos or search for content, be specific and reference actual video titles from their library.");
    LUCID_INFO("[MainWindow] System message set");
    
    // Setup UI
    LUCID_INFO("[MainWindow] Calling setup_ui()...");
    setup_ui();
    LUCID_INFO("[MainWindow] setup_ui() done");
    
    LUCID_INFO("[MainWindow] Calling setup_keyboard_shortcuts()...");
    setup_keyboard_shortcuts();
    LUCID_INFO("[MainWindow] setup_keyboard_shortcuts() done");
    
    // Initialize UI components after setup_ui() creates containers
    LUCID_INFO("[MainWindow] Initializing settings page...");
    LUCID_INFO("MainWindow: calling settings_page->initialize()");
    m_settings_page->initialize(nullptr);
    LUCID_INFO("MainWindow: settings_page->initialize() returned");
    LUCID_INFO("[MainWindow] Settings page initialized");
    
    // Sync model selections from ModelManager (which loads from config on startup)
    m_settings_page->sync_from_model_manager();
    
    // Update widgets to reflect the synced values from ModelManager
    m_settings_page->sync_to_widgets();
    
    // Connect settings page to main window for navigation
    m_settings_page->set_parent_window(this);
    
    // Wire thumbnail size changes to refresh VideoList
    m_settings_page->set_on_thumbnail_size_changed([this](ThumbnailSize size) {
        LUCID_DEBUG("[MainWindow] Thumbnail size changed: %d", static_cast<int>(size));
        m_thumbnail_size = size;
        
        // Update VideoList's thumbnail size setting and refresh
        m_video_grid->set_thumbnail_size(size);
    });
    
    m_model_hub_page->initialize(nullptr);
    LUCID_INFO("MainWindow: model_hub_page->initialize() done");
    m_video_grid->initialize(nullptr);
    LUCID_INFO("MainWindow: video_grid->initialize() done");
    m_search_panel->initialize(nullptr);
    LUCID_INFO("MainWindow: search_panel->initialize() done");
    m_analysis_queue_panel->initialize(nullptr);  // 🔄 NEW
    LUCID_INFO("MainWindow: analysis_queue_panel->initialize() done");
    
    // VideoList has its own internal pagination widget - use it directly
    // Set up page change callback to reload videos and save settings
    m_video_grid->set_on_page_changed([this](int page, int page_size) {
        m_video_page = page;
        m_video_page_size = page_size;
        load_videos_page(page, page_size);
        
        // Save page size preference if it changed
        if (page_size != AppSettings::instance().get_video_page_size()) {
            AppSettings::instance().set_video_page_size(page_size);
        }
    });
    
    // Set up video activation callback - clicking a video opens the video detail page
    m_video_grid->set_on_video_activated([this](const VideoItem& video) {
        LUCID_DEBUG("[MainWindow] Video activated: %s (%s)", video.title.c_str(), video.id.c_str());
        show_video_detail(video.id);
    });
    
    // Load initial page
    load_videos_from_database();  // Load videos first before UI setup
    
    // Add pages to stack after all components are initialized
    setup_content_area();
    
    // Show home page (which uses the list and pagination)
    show_home();
    
    // Refresh the video list to show loaded videos (call after m_video_list is set up)
    refresh_video_list();
    
    LUCID_INFO("Lucid main window initialized");
    LUCID_DEBUG("[MainWindow] Constructor END - window is ready");
}

MainWindow::~MainWindow() {
    LUCID_DEBUG("[MainWindow] Destructor START - signaling shutdown");
    
    // Signal shutdown to all detached threads
    set_shutting_down();
    
    // Wait a moment for threads to notice the flag
    // (Note: threads will check flag before accessing members)
    
    LUCID_DEBUG("[MainWindow] Destructor END");
}

Glib::RefPtr<MainWindow> MainWindow::create() {
    return Glib::RefPtr<MainWindow>(new MainWindow());
}

void MainWindow::setup_ui() {
    // Main container (vertical box: header + content)
    GtkWidget* main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(gobj()), main_box);
    
    // Connect close request signal for proper shutdown
    g_signal_connect(gobj(), "close-request", G_CALLBACK(+[](GtkWindow* window, gpointer data) -> gboolean {
        [[maybe_unused]] auto* self = static_cast<MainWindow*>(data);
        LUCID_INFO("MainWindow close requested - initiating shutdown");
        
        // Get the application and quit it
        GtkApplication* gtk_app = gtk_window_get_application(window);
        if (gtk_app) {
            LUCID_INFO("Calling g_application_quit()");
            g_application_quit(G_APPLICATION(gtk_app));
        }
        
        return false;  // Allow window to close
    }), this);
    
    // Header bar (using gtk-header-bar as the titlebar)
    GtkWidget* header = gtk_header_bar_new();
    gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(header), TRUE);
    gtk_window_set_titlebar(GTK_WINDOW(gobj()), header);
    
    // Add folder button
    GtkWidget* add_folder_btn = gtk_button_new_from_icon_name("folder-open");
    gtk_widget_set_tooltip_text(add_folder_btn, "Add Folder (Ctrl+O)");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), add_folder_btn);
    g_signal_connect(add_folder_btn, "clicked", G_CALLBACK(on_add_folder_clicked), this);
    
    // Title
    GtkWidget* title_label = gtk_label_new("Lucid");
    gtk_label_set_markup(GTK_LABEL(title_label), "<span weight='bold'>Lucid</span>");
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header), title_label);
    
    // Progress bar (shown during folder scanning)
    m_progress_bar = gtk_progress_bar_new();
    gtk_widget_set_size_request(m_progress_bar, -1, 6);
    gtk_widget_set_visible(m_progress_bar, FALSE);
    gtk_box_append(GTK_BOX(main_box), m_progress_bar);
    
    // Content area (horizontal box: sidebar + main content)
    GtkWidget* content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_vexpand(content, TRUE);
    gtk_box_append(GTK_BOX(main_box), content);
    
    // Store content_box BEFORE setup_sidebar() uses it
    m_content_box = content;
    
    setup_sidebar();
}

void MainWindow::setup_sidebar() {
    GtkWidget* content = m_content_box;
    if (!content) return;
    
    GtkWidget* sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_size_request(sidebar, 180, -1);
    gtk_widget_set_hexpand(sidebar, FALSE);
    gtk_widget_set_vexpand(sidebar, TRUE);
    gtk_widget_set_margin_start(sidebar, 8);
    gtk_widget_set_margin_top(sidebar, 8);
    gtk_widget_set_margin_bottom(sidebar, 8);
    gtk_box_append(GTK_BOX(content), sidebar);
    m_sidebar = sidebar;
    
    // Home button
    m_home_btn = gtk_button_new();
    GtkWidget* home_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(home_box, 8);
    gtk_widget_set_margin_end(home_box, 8);
    gtk_widget_set_margin_top(home_box, 4);
    gtk_widget_set_margin_bottom(home_box, 4);
    gtk_button_set_child(GTK_BUTTON(m_home_btn), home_box);
    GtkWidget* home_icon = gtk_image_new_from_icon_name("video-x-generic");
    gtk_image_set_icon_size(GTK_IMAGE(home_icon), GTK_ICON_SIZE_NORMAL);
    gtk_box_append(GTK_BOX(home_box), home_icon);
    gtk_box_append(GTK_BOX(home_box), gtk_label_new("All Videos"));
    gtk_widget_set_halign(m_home_btn, GTK_ALIGN_FILL);
    g_signal_connect(m_home_btn, "clicked", G_CALLBACK(on_home_clicked), this);
    gtk_box_append(GTK_BOX(sidebar), m_home_btn);

    // Search button  // 🔄 NEW
    m_search_btn = gtk_button_new();
    GtkWidget* search_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(search_box, 8);
    gtk_widget_set_margin_end(search_box, 8);
    gtk_widget_set_margin_top(search_box, 4);
    gtk_widget_set_margin_bottom(search_box, 4);
    gtk_button_set_child(GTK_BUTTON(m_search_btn), search_box);
    GtkWidget* search_icon = gtk_image_new_from_icon_name("edit-find-symbolic");
    gtk_image_set_icon_size(GTK_IMAGE(search_icon), GTK_ICON_SIZE_NORMAL);
    gtk_box_append(GTK_BOX(search_box), search_icon);
    gtk_box_append(GTK_BOX(search_box), gtk_label_new("Search"));
    gtk_widget_set_halign(m_search_btn, GTK_ALIGN_FILL);
    g_signal_connect(m_search_btn, "clicked", G_CALLBACK(on_search_clicked), this);
    gtk_box_append(GTK_BOX(sidebar), m_search_btn);

    // Analysis Queue button  // 🔄 NEW
    m_analysis_queue_btn = gtk_button_new();
    GtkWidget* queue_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(queue_box, 8);
    gtk_widget_set_margin_end(queue_box, 8);
    gtk_widget_set_margin_top(queue_box, 4);
    gtk_widget_set_margin_bottom(queue_box, 4);
    gtk_button_set_child(GTK_BUTTON(m_analysis_queue_btn), queue_box);
    GtkWidget* queue_icon = gtk_image_new_from_icon_name("system-run-symbolic");
    gtk_image_set_icon_size(GTK_IMAGE(queue_icon), GTK_ICON_SIZE_NORMAL);
    gtk_box_append(GTK_BOX(queue_box), queue_icon);
    gtk_box_append(GTK_BOX(queue_box), gtk_label_new("Analysis Queue"));
    gtk_widget_set_halign(m_analysis_queue_btn, GTK_ALIGN_FILL);
    g_signal_connect(m_analysis_queue_btn, "clicked", G_CALLBACK(on_analysis_queue_clicked), this);
    gtk_box_append(GTK_BOX(sidebar), m_analysis_queue_btn);
    
    // Settings button
    m_settings_btn = gtk_button_new();
    GtkWidget* settings_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(settings_box, 8);
    gtk_widget_set_margin_end(settings_box, 8);
    gtk_widget_set_margin_top(settings_box, 4);
    gtk_widget_set_margin_bottom(settings_box, 4);
    gtk_button_set_child(GTK_BUTTON(m_settings_btn), settings_box);
    GtkWidget* settings_icon = gtk_image_new_from_icon_name("preferences-system");
    gtk_image_set_icon_size(GTK_IMAGE(settings_icon), GTK_ICON_SIZE_NORMAL);
    gtk_box_append(GTK_BOX(settings_box), settings_icon);
    gtk_box_append(GTK_BOX(settings_box), gtk_label_new("Settings"));
    gtk_widget_set_halign(m_settings_btn, GTK_ALIGN_FILL);
    g_signal_connect(m_settings_btn, "clicked", G_CALLBACK(on_settings_clicked), this);
    gtk_box_append(GTK_BOX(sidebar), m_settings_btn);
    
    // Model Hub button
    m_model_hub_btn = gtk_button_new();
    GtkWidget* model_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(model_box, 8);
    gtk_widget_set_margin_end(model_box, 8);
    gtk_widget_set_margin_top(model_box, 4);
    gtk_widget_set_margin_bottom(model_box, 4);
    gtk_button_set_child(GTK_BUTTON(m_model_hub_btn), model_box);
    GtkWidget* model_icon = gtk_image_new_from_icon_name("network-server");
    gtk_image_set_icon_size(GTK_IMAGE(model_icon), GTK_ICON_SIZE_NORMAL);
    gtk_box_append(GTK_BOX(model_box), model_icon);
    gtk_box_append(GTK_BOX(model_box), gtk_label_new("Model Hub"));
    gtk_widget_set_halign(m_model_hub_btn, GTK_ALIGN_FILL);
    g_signal_connect(m_model_hub_btn, "clicked", G_CALLBACK(on_model_hub_clicked), this);
    gtk_box_append(GTK_BOX(sidebar), m_model_hub_btn);

    // Chat button
    m_chat_btn = gtk_button_new();
    GtkWidget* chat_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(chat_box, 8);
    gtk_widget_set_margin_end(chat_box, 8);
    gtk_widget_set_margin_top(chat_box, 4);
    gtk_widget_set_margin_bottom(chat_box, 4);
    gtk_button_set_child(GTK_BUTTON(m_chat_btn), chat_box);
    GtkWidget* chat_icon = gtk_image_new_from_icon_name("chat-symbolic");
    gtk_image_set_icon_size(GTK_IMAGE(chat_icon), GTK_ICON_SIZE_NORMAL);
    gtk_box_append(GTK_BOX(chat_box), chat_icon);
    gtk_box_append(GTK_BOX(chat_box), gtk_label_new("Chat"));
    gtk_widget_set_halign(m_chat_btn, GTK_ALIGN_FILL);
    g_signal_connect(m_chat_btn, "clicked", G_CALLBACK(on_chat_clicked), this);
    gtk_box_append(GTK_BOX(sidebar), m_chat_btn);
    
    // Spacer
    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(sidebar), spacer);
    
    // Status section
    m_video_count_label = gtk_label_new("0 videos");
    gtk_widget_set_halign(m_video_count_label, GTK_ALIGN_START);
    gtk_widget_set_margin_start(m_video_count_label, 12);
    gtk_box_append(GTK_BOX(sidebar), m_video_count_label);
    
    m_status_label = gtk_label_new("Ready");
    gtk_widget_set_halign(m_status_label, GTK_ALIGN_START);
    gtk_widget_set_opacity(m_status_label, 0.7);
    gtk_widget_set_margin_start(m_status_label, 12);
    gtk_widget_set_margin_bottom(m_status_label, 8);
    gtk_box_append(GTK_BOX(sidebar), m_status_label);
}

void MainWindow::setup_content_area() {
    if (!m_content_box) return;
    
    // Find main content area (second child after sidebar)
    GtkWidget* first_child = gtk_widget_get_first_child(m_content_box);
    if (!first_child) return;
    
    GtkWidget* content_area = gtk_widget_get_next_sibling(first_child);
    if (!content_area) {
        // Create content area if it doesn't exist
        content_area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_hexpand(content_area, TRUE);
        gtk_widget_set_vexpand(content_area, TRUE);
        gtk_box_append(GTK_BOX(m_content_box), content_area);
    }
    
    // Create stacked container for pages
    m_stack = gtk_stack_new();
    gtk_widget_set_hexpand(m_stack, TRUE);
    gtk_widget_set_vexpand(m_stack, TRUE);
    gtk_box_append(GTK_BOX(content_area), m_stack);
    
    // Home view uses VideoList with pagination at the bottom
    {
        // VideoList's container includes scrolled window + list + pagination
        GtkWidget* home_container = m_video_grid->get_widget();
        if (home_container) {
            gtk_stack_add_named(GTK_STACK(m_stack), home_container, "home");
        }
    }
    
    // Settings view (initialized before add to stack)
    GtkWidget* settings_view = m_settings_page->get_widget();
    if (settings_view) {
        gtk_stack_add_named(GTK_STACK(m_stack), settings_view, "settings");
    }
    
    // Model Hub view (initialized before add to stack)
    GtkWidget* model_hub_view = m_model_hub_page->get_widget();
    if (model_hub_view) {
        gtk_stack_add_named(GTK_STACK(m_stack), model_hub_view, "model_hub");
    }
    
    // VideoList view with pagination (VideoList has pagination internally)
    {
        // VideoList's container already includes pagination at the bottom
        GtkWidget* video_list_view = m_video_grid->get_widget();
        if (video_list_view) {
            gtk_stack_add_named(GTK_STACK(m_stack), video_list_view, "videolist");
        }
    }

    // Search page - Dedicated search results page
    {
        // Create scrolled window container
        m_search_page = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(m_search_page),
                                      GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_stack_add_named(GTK_STACK(m_stack), m_search_page, "search");

        // Create inner container for search page
        GtkWidget* search_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_margin_start(search_content, 12);
        gtk_widget_set_margin_end(search_content, 12);
        gtk_widget_set_margin_top(search_content, 12);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(m_search_page), search_content);

        // Search entry at top of search page
        m_search_entry = gtk_search_entry_new();
        gtk_widget_set_hexpand(m_search_entry, TRUE);
        gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(m_search_entry), 
                                               "Search videos by content... (Enter to search)");
        gtk_box_append(GTK_BOX(search_content), m_search_entry);
        g_signal_connect(m_search_entry, "activate", G_CALLBACK(on_search_activate), this);

        // Store search entry reference for on_search_clicked
        g_object_set_data(G_OBJECT(m_search_page), "search-entry", m_search_entry);

        // Separator between search bar and results
        GtkWidget* sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_widget_set_margin_top(sep, 8);
        gtk_widget_set_margin_bottom(sep, 8);
        gtk_box_append(GTK_BOX(search_content), sep);

        // Results container (will be populated by display_search_results)
        GtkWidget* results_container = gtk_list_box_new();
        gtk_widget_set_vexpand(results_container, TRUE);
        gtk_box_append(GTK_BOX(search_content), results_container);
        g_object_set_data(G_OBJECT(m_search_page), "results-container", results_container);
    }

    // Analysis Queue view  // 🔄 NEW
    GtkWidget* analysis_queue_view = m_analysis_queue_panel->get_widget();
    if (analysis_queue_view) {
        gtk_stack_add_named(GTK_STACK(m_stack), analysis_queue_view, "analysis_queue");
    }
    
    // Video Detail page (Timeline Widget)  // 🔄 NEW
    {
        m_video_detail_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_set_margin_start(m_video_detail_page, 12);
        gtk_widget_set_margin_end(m_video_detail_page, 12);
        gtk_widget_set_margin_top(m_video_detail_page, 12);
        
        // Video player widget
        if (m_video_player_widget) {
            gtk_box_append(GTK_BOX(m_video_detail_page), GTK_WIDGET(m_video_player_widget->gobj()));
            LUCID_DEBUG("[MainWindow] Video player widget added to video_detail_page");
        }
        
        // Timeline widget
        if (m_timeline_widget) {
            GtkWidget* timeline = GTK_WIDGET(m_timeline_widget->gobj());
            gtk_widget_set_hexpand(timeline, TRUE);
            gtk_widget_set_vexpand(timeline, FALSE);
            gtk_widget_set_size_request(timeline, -1, 100);  // Force minimum height of 100px
            gtk_box_append(GTK_BOX(m_video_detail_page), timeline);
            LUCID_DEBUG("[MainWindow] Timeline widget added to video_detail_page");
        }
        
        // Add marker panel (renamed from Transcription panel)
        if (m_marker_panel) {
            GtkWidget* marker_panel = m_marker_panel->get_widget();
            gtk_widget_set_hexpand(marker_panel, TRUE);
            gtk_widget_set_vexpand(marker_panel, FALSE);
            gtk_widget_set_size_request(marker_panel, -1, 200);  // Force minimum height of 200px
            gtk_box_append(GTK_BOX(m_video_detail_page), marker_panel);
            LUCID_DEBUG("[MainWindow] Marker panel added to video_detail_page");
        }
        
        // Back button
        GtkWidget* back_btn = gtk_button_new_with_label("← Back to Videos");
        gtk_box_append(GTK_BOX(m_video_detail_page), back_btn);
        g_signal_connect(back_btn, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer data) {
            auto* self = static_cast<MainWindow*>(data);
            self->show_home();
        }), this);
        
        gtk_stack_add_named(GTK_STACK(m_stack), m_video_detail_page, "video_detail");
    }
}

void MainWindow::show_page(Page page) {
    // Release LLM engine when leaving CHAT page (allows other pages to use it)
    if (m_current_page == Page::CHAT && page != Page::CHAT) {
        LUCID_DEBUG("[MainWindow] Leaving CHAT page, releasing LLM engine");
        if (m_llm_engine && m_llm_engine->is_ready()) {
            m_llm_engine->shutdown();
        }
    }
    
    m_current_page = page;

    const char* page_name = nullptr;
    switch (page) {
        case Page::HOME:
            page_name = "home";
            break;
        case Page::VIDEO_LIST:
            page_name = "videolist";
            break;
        case Page::SEARCH:
            page_name = "search";
            break;
        case Page::VIDEO_DETAIL:
            page_name = "video_detail";
            break;
        case Page::ANALYSIS_QUEUE:
            page_name = "analysis_queue";
            break;
        case Page::SETTINGS:
            page_name = "settings";
            break;
        case Page::MODEL_HUB:
            page_name = "model_hub";
            break;
        case Page::CHAT:
            page_name = "chat";
            break;
    }
    
    if (page_name && m_stack) {
        gtk_stack_set_visible_child_name(GTK_STACK(m_stack), page_name);
    }
    
    LUCID_DEBUG("Switched to page: %s", page_name);
}

std::string MainWindow::get_current_page_name() const {
    switch (m_current_page) {
        case Page::HOME:
            return "home";
        case Page::VIDEO_LIST:
            return "videolist";
        case Page::SEARCH:
            return "search";
        case Page::VIDEO_DETAIL:
            return "video_detail";
        case Page::ANALYSIS_QUEUE:
            return "analysis_queue";
        case Page::SETTINGS:
            return "settings";
        case Page::MODEL_HUB:
            return "model_hub";
        case Page::CHAT:
            return "chat";
    }
    return "unknown";
}

void MainWindow::show_home() {
    // Show video list if we have videos, otherwise show welcome state
    if (m_videos.empty()) {
        show_page(Page::HOME);
    } else {
        show_page(Page::VIDEO_LIST);
        load_videos_page(m_video_page, m_video_page_size);
    }
}

void MainWindow::show_video_list() {
    // Resume analysis when leaving settings
    if (m_app) {
        m_app->resume_analysis();
    }
    show_page(Page::VIDEO_LIST);
    // Load videos with current pagination settings
    load_videos_page(m_video_page, m_video_page_size);
}

void MainWindow::show_search() {  // 🔄 NEW
    show_page(Page::SEARCH);
    display_search_results();
}

void MainWindow::on_search_clicked(GtkButton* button, gpointer data) {  // 🔄 NEW
    auto* self = static_cast<MainWindow*>(data);
    
    // Always show the search page first
    self->show_search();
    
    // If no stored query, focus the search entry for immediate typing
    if (self->m_stored_search_query.empty()) {
        if (self->m_search_entry) {
            gtk_widget_grab_focus(self->m_search_entry);
        }
    }
    // If query exists, display_search_results() will show results
}

void MainWindow::display_search_results() {  // 🔄 NEW
    if (!m_search_page) return;

    // Get results container
    GtkWidget* results_container = static_cast<GtkWidget*>(
        g_object_get_data(G_OBJECT(m_search_page), "results-container"));
    if (!results_container) return;

    // Clear existing results
    GtkListBoxRow* row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(results_container), 0);
    while (row) {
        gtk_list_box_remove(GTK_LIST_BOX(results_container), GTK_WIDGET(row));
        row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(results_container), 0);
    }

    // Check if no search results - show prompt to enter search query
    bool has_results = !m_stored_video_matches.empty() ||
                      !m_stored_marker_results.empty() ||
                      !m_stored_search_results.empty();

    if (!has_results && !m_stored_search_query.empty()) {
        // No results found
        GtkWidget* no_results = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_set_halign(no_results, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(no_results, GTK_ALIGN_CENTER);
        gtk_widget_set_vexpand(no_results, TRUE);
        gtk_box_append(GTK_BOX(no_results), gtk_image_new_from_icon_name("edit-find"));
        gtk_box_append(GTK_BOX(no_results), gtk_label_new("No videos or transcripts match your search"));
        gtk_list_box_append(GTK_LIST_BOX(results_container), no_results);

        gtk_label_set_text(GTK_LABEL(m_status_label), "No results");
        return;
    }

    if (m_stored_search_query.empty()) {
        // No search query yet - show empty state
        GtkWidget* empty_prompt = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_set_halign(empty_prompt, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(empty_prompt, GTK_ALIGN_CENTER);
        gtk_widget_set_vexpand(empty_prompt, TRUE);
        gtk_box_append(GTK_BOX(empty_prompt), gtk_image_new_from_icon_name("edit-find"));
        gtk_box_append(GTK_BOX(empty_prompt), gtk_label_new("Enter a search query above"));
        gtk_list_box_append(GTK_LIST_BOX(results_container), empty_prompt);
        
        gtk_label_set_text(GTK_LABEL(m_status_label), "Ready");
        return;
    }

    // Count result types
    int title_matches = 0, transcript_matches = 0;
    for (const auto& r : m_stored_search_results) {
        if (r.match_type == MatchType::TITLE) title_matches++;
        else transcript_matches++;
    }
    char status[128];
    snprintf(status, sizeof(status), "Found: %zu videos, %d title, %d transcript matches",
             m_stored_video_matches.size(), title_matches, transcript_matches);
    gtk_label_set_text(GTK_LABEL(m_status_label), status);

    // Section header for transcripts (FTS)
    if (!m_stored_marker_results.empty()) {
        GtkWidget* header = gtk_label_new(nullptr);
        gtk_label_set_markup(GTK_LABEL(header), "<b>Transcripts</b>");
        gtk_widget_set_halign(header, GTK_ALIGN_START);
        gtk_widget_set_margin_start(header, 12);
        gtk_widget_set_margin_top(header, 8);
        gtk_widget_set_margin_bottom(header, 4);
        gtk_list_box_append(GTK_LIST_BOX(results_container), header);

        for (const auto& marker : m_stored_marker_results) {
            GtkWidget* item = create_marker_item(marker);
            gtk_list_box_append(GTK_LIST_BOX(results_container), item);
        }
    }

    // Section header for semantic matches (llama.cpp embeddings)
    if (!m_stored_search_results.empty()) {
        GtkWidget* header = gtk_label_new(nullptr);
        gtk_label_set_markup(GTK_LABEL(header), "<b>Semantic Matches</b>");
        gtk_widget_set_halign(header, GTK_ALIGN_START);
        gtk_widget_set_margin_start(header, 12);
        gtk_widget_set_margin_top(header, m_stored_marker_results.empty() ? 8 : 16);
        gtk_widget_set_margin_bottom(header, 4);
        gtk_list_box_append(GTK_LIST_BOX(results_container), header);

        for (const auto& result : m_stored_search_results) {
            GtkWidget* item = create_semantic_result_item(result);
            gtk_list_box_append(GTK_LIST_BOX(results_container), item);
        }
    }

    // Section header for videos (if any)
    if (!m_stored_video_matches.empty()) {
        GtkWidget* header = gtk_label_new(nullptr);
        gtk_label_set_markup(GTK_LABEL(header), "<b>Videos</b>");
        gtk_widget_set_halign(header, GTK_ALIGN_START);
        gtk_widget_set_margin_start(header, 12);
        gtk_widget_set_margin_top(header, m_stored_marker_results.empty() ? 8 : 16);
        gtk_widget_set_margin_bottom(header, 4);
        gtk_list_box_append(GTK_LIST_BOX(results_container), header);

        for (size_t i = 0; i < m_stored_video_matches.size(); i++) {
            GtkWidget* item = create_video_item(
                m_stored_video_ids[i],
                m_stored_video_matches[i],
                nullptr);
            gtk_list_box_append(GTK_LIST_BOX(results_container), item);
        }
    }
}

void MainWindow::clear_search() {  // 🔄 NEW
    m_stored_search_query.clear();
    m_stored_search_results.clear();
    m_stored_marker_results.clear();
    m_stored_video_matches.clear();
    m_stored_video_ids.clear();

    // Clear the search entry
    if (m_search_entry) {
        gtk_editable_set_text(GTK_EDITABLE(m_search_entry), "");
    }
}

void MainWindow::show_analysis_queue() {  // 🔄 NEW
    // Resume analysis when leaving settings
    if (m_app) {
        m_app->resume_analysis();
    }
    show_page(Page::ANALYSIS_QUEUE);
}

void MainWindow::show_settings() {
    // Pause analysis when opening settings so user can change models without conflict
    if (m_app) {
        m_app->pause_analysis();
    }
    show_page(Page::SETTINGS);
}

void MainWindow::show_model_hub() {
    show_page(Page::MODEL_HUB);
}

void MainWindow::show_chat() {
    // Initialize chat panel if needed
    if (!m_chat_panel) {
        m_chat_panel = std::make_unique<ChatPanel>();
        m_chat_panel->set_database(m_db.get());
        m_chat_panel->set_chat_session(m_chat_session.get());
        
        // Connect chat panel message signal to handle in MainWindow
        m_chat_panel->signal_message_submitted().connect(
            sigc::mem_fun(*this, &MainWindow::on_chat_message_submitted));
        
        // Connect video click signal to navigate to video detail
        m_chat_panel->signal_video_clicked().connect([this](const std::string& video_id) {
            LUCID_DEBUG("[MainWindow] Chat video clicked: %s", video_id.c_str());
            show_video_detail(video_id);
        });
        
        // Connect search content signal - search the message text and navigate to search page
        m_chat_panel->signal_search().connect([this](const std::string& query) {
            LUCID_DEBUG("[MainWindow] Chat search triggered with: %s", query.c_str());
            // Store the query and perform search
            perform_search(query);
        });
        
        // Add to stack - need to convert Gtk::Box* to GtkWidget*
        gtk_stack_add_named(GTK_STACK(m_stack), GTK_WIDGET(m_chat_panel->gobj()), "chat");
    }
    
    // Load LLM model if not already loaded
    ensure_llm_engine_ready();
    
    show_page(Page::CHAT);
}

void MainWindow::ensure_llm_engine_ready() {
    // Load LLM model if not already ready
    if (!m_llm_engine->is_ready()) {
        auto llm_model = ModelManager::instance().get_selected_llm_model();
        auto llm_path = ModelManager::instance().get_selected_llm_path();
        LUCID_INFO("[MainWindow] ensure_llm_engine_ready: selected='%s', path='%s'", 
                   llm_model.c_str(), llm_path ? llm_path->c_str() : "none");
        
        if (llm_path && !llm_path->empty()) {
            LUCID_INFO("[MainWindow] Loading LLM model from: %s", llm_path->c_str());
            if (!m_llm_engine->initialize(*llm_path)) {
                LUCID_ERROR("[MainWindow] Failed to load LLM model: %s", llm_path->c_str());
            } else {
                LUCID_INFO("[MainWindow] LLM model loaded successfully");
            }
        } else {
            LUCID_DEBUG("[MainWindow] No LLM model path available for: %s", llm_model.c_str());
        }
    }
}

void MainWindow::on_chat_message_submitted(const std::string& message) {
    LUCID_DEBUG("[MainWindow] Chat message submitted: %s", message.c_str());
    
    // Add user message to session
    m_chat_session->add_user_message(message);
    
    // Debug: Log the conversation so far
    auto msgs = m_chat_session->get_messages();
    LUCID_DEBUG("[MainWindow] Conversation has %zu messages:", msgs.size());
    for (size_t i = 0; i < msgs.size(); i++) {
        const auto& msg = msgs[i];
        std::string role_str = (msg.role == ChatMessage::Role::USER) ? "USER" : 
                              (msg.role == ChatMessage::Role::SYSTEM) ? "SYSTEM" : "ASSISTANT";
        // Truncate long messages for logging
        std::string content = msg.content;
        if (content.length() > 200) {
            content = content.substr(0, 200) + "... [truncated]";
        }
        LUCID_DEBUG("  [%zu] %s: %s", i, role_str.c_str(), content.c_str());
    }
    
    // Ensure LLM engine is ready
    ensure_llm_engine_ready();
    
    if (!m_llm_engine->is_ready()) {
        // Show error message
        m_chat_panel->on_response_ready("No LLM model loaded. Please select an LLM model in Settings > Models.", false);
        return;
    }
    
    // Build video library context for this query
    std::string video_context;
    if (m_chat_context && m_chat_context->is_available()) {
        video_context = m_chat_context->build_query_context(message);
        if (!video_context.empty()) {
            LUCID_DEBUG("[MainWindow] Video library context:\n%s", video_context.c_str());
        }
    }
    
    // Run LLM generation in background thread
    std::thread([this, video_context]() {
        std::string response;
        bool success = false;
        
        // Use context-aware generation if we have context
        if (!video_context.empty()) {
            LUCID_DEBUG("[MainWindow] Using context-aware generation");
            
            // Create token callback to accumulate response
            auto token_callback = [&response](const std::string& token) -> bool {
                response += token;
                return true;  // Continue generation
            };
            
            // Use the context-aware method from ChatSession
            m_chat_session->generate_with_context(video_context, token_callback);
            success = !response.empty();
        } else {
            // Fall back to regular generation
            LUCID_DEBUG("[MainWindow] Using regular generation (no context)");
            success = m_llm_engine->generate(
                m_chat_session->get_messages(),
                [&response](const LLMResponse& chunk) {
                    response += chunk.text;
                    return true;  // Continue generation
                }
            );
        }
        
        LUCID_DEBUG("[MainWindow] LLM generation complete: success=%s, response_len=%zu",
                    success ? "true" : "false", 
                    response.length());
        
        // Show first 500 chars of response for debugging
        if (!response.empty()) {
            std::string debug_response = response;
            if (debug_response.length() > 500) {
                debug_response = debug_response.substr(0, 500) + "... [truncated]";
            }
            LUCID_DEBUG("[MainWindow] Response: %s", debug_response.c_str());
        } else {
            LUCID_WARN("[MainWindow] Empty response from LLM!");
        }
        
        // Add assistant response to session (if not already done by generate_with_context)
        // Note: generate_with_context already calls add_assistant_message internally
        if (success && !response.empty()) {
            // Only add if not already added (generate_with_context adds it)
            auto msgs = m_chat_session->get_messages();
            if (msgs.empty() || msgs.back().role != ChatMessage::Role::ASSISTANT ||
                msgs.back().content != response) {
                m_chat_session->add_assistant_message(response);
            }
        }
        
        // Update UI on main thread (capture this to access m_chat_panel)
        // MUST check shutdown flag before accessing UI members
        Glib::signal_idle().connect_once([this, response, success]() {
            if (is_shutting_down()) {
                LUCID_DEBUG("[MainWindow] Shutdown in progress, skipping chat response update");
                return;
            }
            if (m_chat_panel) {
                m_chat_panel->on_response_ready(response, success);
            }
        });
    }).detach();
}

void MainWindow::show_video_detail(const std::string& video_id) {
    m_current_video_id = video_id;
    load_video_detail_page(video_id);
    show_page(Page::VIDEO_DETAIL);
}

void MainWindow::show_video_detail_with_seek(const std::string& video_id, int64_t position_ms) {
    m_current_video_id = video_id;
    load_video_detail_page(video_id, position_ms);
    show_page(Page::VIDEO_DETAIL);
}

void MainWindow::load_video_detail_page(const std::string& video_id, int64_t seek_position_ms) {  // 🔄 NEW
    if (!m_video_detail_page || !m_db) return;
    
    LUCID_DEBUG("[MainWindow] Loading video detail for: %s (seek to %ld ms)", video_id.c_str(), (long)seek_position_ms);
    
    // Get video entry from database
    auto video_opt = m_db->get_video(video_id);
    if (!video_opt) {
        LUCID_DEBUG("[MainWindow] Video not found in database: %s", video_id.c_str());
        return;
    }
    
    const auto& video = video_opt.value();
    
    // Load video into player - unload any previous video first to reset state
    if (m_video_player_widget) {
        m_video_player_widget->unload();  // Reset state before loading new video
        m_video_player_widget->load_video(video.path);
        
        // Set position callback to update timeline
        m_video_player_widget->set_position_callback([this](int64_t position_ms) {
            if (m_timeline_widget) {
                m_timeline_widget->set_position(position_ms);
            }
        });
        
        // Set duration callback to update timeline AND handle pending seek
        m_video_player_widget->set_duration_callback([this](int64_t duration_ms) {
            if (m_timeline_widget) {
                LUCID_DEBUG("[MainWindow] VideoPlayer duration: %ld ms, updating timeline", duration_ms);
                m_timeline_widget->set_duration(duration_ms);
            }
            
            // If we have a pending seek, execute it now that video is prepared
            if (m_pending_seek_ms >= 0) {
                int64_t seek_pos = m_pending_seek_ms;
                m_pending_seek_ms = -1;  // Clear before seeking
                LUCID_DEBUG("[MainWindow] Executing pending seek to %ld ms", (long)seek_pos);
                this->m_video_player_widget->seek_to(seek_pos);
            }
        });
        
        // Set ended callback to reset timeline
        m_video_player_widget->set_ended_callback([this]() {
            if (m_timeline_widget) {
                m_timeline_widget->set_position(0);
            }
        });
    }
    
    // Clear timeline widget state before loading new video
    if (m_timeline_widget) {
        m_timeline_widget->clear();
    }
    
    // Get thumbnails
    auto thumbnails = m_db->get_thumbnails(video_id);
    std::vector<TimelineThumbnail> timeline_thumbs;
    for (const auto& thumb : thumbnails) {
        timeline_thumbs.emplace_back(thumb.timestamp_ms, thumb.path);
    }
    
    // Get markers
    auto markers = m_db->get_markers(video_id);
    std::vector<TimelineMarker> timeline_markers;
    for (const auto& marker : markers) {
        timeline_markers.emplace_back(marker.start_ms, marker.text);
        timeline_markers.back().confidence = marker.confidence;
        timeline_markers.back().thumbnail_path = marker.thumbnail_path;
        timeline_markers.back().marker_type = marker.marker_type;
        // A VISION marker is pending if it has no text (description not yet filled by Qwen2-VL)
        timeline_markers.back().is_pending = 
            (marker.marker_type == MarkerType::VISION && marker.text.empty());
    }
    
    // Update timeline widget (don't set duration yet - wait for VideoPlayer's actual duration via callback)
    if (m_timeline_widget) {
        m_timeline_widget->set_position(0);
        m_timeline_widget->set_thumbnails(timeline_thumbs);
        m_timeline_widget->set_markers(timeline_markers);
        
        // Set seek callback to seek video player
        m_timeline_widget->set_seek_callback([this](int64_t position_ms) {
            LUCID_DEBUG("[MainWindow] Timeline seek: %ld ms", position_ms);
            if (m_video_player_widget) {
                m_video_player_widget->seek_to(position_ms);
            }
        });
        
        // Set marker click callback to seek video player
        m_timeline_widget->set_marker_click_callback([this](const TimelineMarker& marker) {
            LUCID_DEBUG("[MainWindow] Marker clicked: %ld ms - %s", 
                       marker.position_ms, marker.text.c_str());
            if (m_video_player_widget) {
                m_video_player_widget->seek_to(marker.position_ms);
            }
        });
    }
    
    // Update marker panel with markers as segments
    if (m_marker_panel) {
        std::vector<TranscriptionSegment> segments;
        for (const auto& marker : markers) {
            TranscriptionSegment seg;
            seg.start_ms = marker.start_ms;
            seg.end_ms = marker.end_ms;
            seg.text = marker.text;
            seg.confidence = marker.confidence;
            segments.push_back(seg);
        }
        m_marker_panel->set_segments(segments);
        
        // Set seek callback to seek video player
        m_marker_panel->set_seek_callback([this](int64_t position_ms) {
            LUCID_DEBUG("[MainWindow] Marker panel seek: %ld ms", position_ms);
            if (m_video_player_widget) {
                m_video_player_widget->seek_to(position_ms);
            }
        });
    }
    
    LUCID_DEBUG("[MainWindow] Video detail loaded: %s, duration: %ld ms, %zu thumbnails, %zu markers",
                video.title.c_str(), video.duration_ms, timeline_thumbs.size(), timeline_markers.size());
    
    // Store pending seek position - the duration callback will execute it when video is ready
    if (seek_position_ms >= 0) {
        LUCID_DEBUG("[MainWindow] Storing pending seek to %ld ms", (long)seek_position_ms);
        m_pending_seek_ms = seek_position_ms;
        // DON'T call seek_to() here - wait for duration callback
    }
}

void MainWindow::show_welcome_state() {
    show_page(Page::HOME);
    
    // Clear and show welcome through VideoList
    if (m_video_grid) {
        m_video_grid->clear();
    }
    gtk_label_set_text(GTK_LABEL(m_video_count_label), "No videos");
}

void MainWindow::load_videos_from_database() {
    // Load all videos from the database
    auto db_videos = m_db->get_all_videos(1000, 0);
    
    m_videos.clear();
    m_video_ids.clear();
    
    for (const auto& v : db_videos) {
        // Verify file exists before adding
        if (std::filesystem::exists(v.path)) {
            m_videos.push_back(v.path);
            m_video_ids.push_back(v.id);
        }
    }
    
    if (!m_videos.empty()) {
        LUCID_INFO("[MainWindow] Loaded %zu videos from database on startup", m_videos.size());
    }
}

void MainWindow::load_videos_page(int page, int page_size) {
    // Calculate offset
    int offset = page * page_size;
    
    // Show loading indicator
    m_video_grid->set_loading(true);
    
    // Load total count first (for pagination)
    m_video_total = m_db->get_all_videos(0, 0).empty() ? 0 : 0;  // Will be set from actual query
    auto db_videos = m_db->get_all_videos(page_size, offset);
    
    // Count total for pagination (separate query)
    auto all_videos = m_db->get_all_videos(1000000, 0);
    m_video_total = static_cast<int>(all_videos.size());
    
    // Convert to VideoItem for VideoList
    std::vector<VideoItem> video_items;
    for (const auto& v : db_videos) {
        VideoItem item;
        item.id = v.id;
        item.path = v.path;
        item.title = v.title.empty() ? std::filesystem::path(v.path).filename().string() : v.title;
        item.duration_sec = static_cast<int>(v.duration_ms / 1000);
        item.is_indexed = (v.status == VideoStatus::COMPLETED);
        
        // Get thumbnail path
        auto thumbnails = m_db->get_thumbnails(v.id);
        if (!thumbnails.empty()) {
            item.thumbnail_path = thumbnails[0].path;
        }
        
        video_items.push_back(item);
    }
    
    // Update VideoList
    m_video_grid->set_videos(video_items);
    m_video_grid->set_total_count(m_video_total);
    m_video_grid->set_page(page);
    m_video_grid->set_page_size(page_size);
    
    // Hide loading indicator
    m_video_grid->set_loading(false);
    
    LUCID_DEBUG("[MainWindow] Loaded page %d (%d items, total: %d)", page, (int)video_items.size(), m_video_total);
}

void MainWindow::set_video_page(int page) {
    if (page < 0) page = 0;
    
    int total_pages = get_video_total_pages();
    if (total_pages > 0 && page >= total_pages) {
        page = total_pages - 1;
    }
    
    if (m_video_page != page) {
        m_video_page = page;
        load_videos_page(m_video_page, m_video_page_size);
    }
}

void MainWindow::set_video_page_size(int page_size) {
    // Validate page size
    if (page_size < 25) page_size = 25;
    if (page_size > 200) page_size = 200;
    
    if (m_video_page_size != page_size) {
        LUCID_DEBUG("[MainWindow] set_video_page_size: %d -> %d", m_video_page_size, page_size);
        
        m_video_page_size = page_size;
        m_video_page = 0;  // Reset to first page when changing page size
        
        load_videos_page(m_video_page, m_video_page_size);
        
        // Save to settings
        AppSettings::instance().set_video_page_size(m_video_page_size);
    }
}

// Signal handlers
void MainWindow::on_add_folder_clicked(GtkButton* button, gpointer data) {
    auto* self = static_cast<MainWindow*>(data);
    
    FolderBrowser browser;
    if (browser.run(GTK_WINDOW(self->gobj()))) {
        self->scan_folder(browser.get_folder_path());
    }
}

void MainWindow::on_home_clicked(GtkButton* button, gpointer data) {
    auto* self = static_cast<MainWindow*>(data);
    self->show_video_list();
}

void MainWindow::on_analysis_queue_clicked(GtkButton* button, gpointer data) {  // 🔄 NEW
    auto* self = static_cast<MainWindow*>(data);
    self->show_analysis_queue();
}

void MainWindow::on_video_detail_clicked(GtkButton* button, gpointer data) {  // 🔄 NEW
    auto* self = static_cast<MainWindow*>(data);
    // Get video_id from button action data
    const char* video_id = static_cast<const char*>(g_object_get_data(G_OBJECT(button), "video_id"));
    if (video_id) {
        self->show_video_detail(video_id);
    }
}

void MainWindow::on_video_item_clicked(GtkButton* button, gpointer data) {  // 🔄 NEW
    auto* self = static_cast<MainWindow*>(data);
    // Get video_id from button action data
    const char* video_id = static_cast<const char*>(g_object_get_data(G_OBJECT(button), "video_id"));
    if (video_id) {
        LUCID_DEBUG("[MainWindow] Video item clicked: %s", video_id);
        self->show_video_detail(video_id);
    }
}

void MainWindow::on_settings_clicked(GtkButton* button, gpointer data) {
    auto* self = static_cast<MainWindow*>(data);
    self->show_settings();
}

void MainWindow::on_model_hub_clicked(GtkButton* button, gpointer data) {
    auto* self = static_cast<MainWindow*>(data);
    self->show_model_hub();
}

void MainWindow::on_chat_clicked(GtkButton* button, gpointer data) {
    auto* self = static_cast<MainWindow*>(data);
    self->show_chat();
}

void MainWindow::on_search_changed(GtkSearchEntry* entry, gpointer data) {
    auto* self = static_cast<MainWindow*>(data);
    const gchar* text = gtk_editable_get_text(GTK_EDITABLE(entry));
    
    // Only clear/show home when search is emptied
    if (!text || strlen(text) == 0) {
        self->show_home();
        gtk_label_set_text(GTK_LABEL(self->m_status_label), "Ready");
    }
    // Don't trigger search on every keystroke - only on Enter
}

void MainWindow::on_search_activate(GtkSearchEntry* entry, gpointer data) {
    auto* self = static_cast<MainWindow*>(data);
    const gchar* text = gtk_editable_get_text(GTK_EDITABLE(entry));
    
    if (!text || strlen(text) == 0) {
        self->show_home();
        gtk_label_set_text(GTK_LABEL(self->m_status_label), "Ready");
        return;
    }
    
    // Only search when user presses Enter
    self->perform_search(text);
}

void MainWindow::scan_folder(const std::string& folder_path) {
    if (folder_path.empty()) return;
    
    gtk_label_set_text(GTK_LABEL(m_status_label), "Scanning...");
    gtk_widget_set_visible(m_progress_bar, TRUE);
    
    std::thread([this, folder_path]() {
        std::vector<std::string> video_paths;
        
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(folder_path)) {
                if (entry.is_regular_file() && FolderBrowser::is_video_file(entry.path().string())) {
                    video_paths.push_back(entry.path().string());
                }
            }
        } catch (const std::exception& e) {
            LUCID_ERROR("Error scanning: %s", e.what());
        }
        
        // Add all videos to database with "pending" status
        for (const auto& path : video_paths) {
            VideoEntry entry;
            // Generate UUID
            static const char* hex = "0123456789abcdef";
            std::string uuid;
            uuid.reserve(36);
            for (int i = 0; i < 36; ++i) {
                if (i == 8 || i == 13 || i == 18 || i == 23) {
                    uuid += '-';
                } else {
                    int r = g_random_int() % 16;
                    uuid += hex[r];
                }
            }
            uuid[14] = '4';
            entry.id = uuid;
            entry.path = path;
            entry.title = Glib::path_get_basename(path);
            entry.source = "local";
            
            try {
                m_db->add_video(entry);
            } catch (const std::exception& e) {
                // Video might already exist (path unique constraint)
                LUCID_DEBUG("Video already in database: %s", path.c_str());
            }
        }
        
        // Now load videos from database
        auto db_videos = m_db->get_all_videos(1000, 0);
        m_videos.clear();
        m_video_ids.clear();
        for (const auto& v : db_videos) {
            m_videos.push_back(v.path);
            m_video_ids.push_back(v.id);
        }
        m_current_folder = folder_path;
        
        // Update UI on main thread
        // MUST check shutdown flag before accessing UI members
        g_idle_add_full(G_PRIORITY_DEFAULT, 
            +[](gpointer user_data) -> gboolean {
                auto* self = static_cast<MainWindow*>(user_data);
                if (self->is_shutting_down()) {
                    LUCID_DEBUG("[MainWindow] Shutdown in progress, skipping folder scan UI update");
                    return G_SOURCE_REMOVE;
                }
                self->show_home();
                self->refresh_video_list();
                
                // Videos are queued for background processing via VideoAnalysisWorker
                gtk_label_set_text(GTK_LABEL(self->m_status_label), "Videos added");
                
                return G_SOURCE_REMOVE;
            }, this, nullptr);
        
    }).detach();
}

void MainWindow::process_videos() {
    if (m_videos.empty()) {
        gtk_label_set_text(GTK_LABEL(m_status_label), "No videos found");
        gtk_widget_set_visible(m_progress_bar, FALSE);
        return;
    }
    
    // Initialize video states for progress tracking
    m_video_states.clear();
    m_video_states.reserve(m_videos.size());
    for (size_t i = 0; i < m_videos.size(); ++i) {
        VideoState state;
        state.id = m_video_ids[i];  // Include video ID
        state.path = m_videos[i];
        state.status = VideoState::Status::PENDING;
        state.progress = 0.0f;
        m_video_states.push_back(state);
    }
    
    m_pending_jobs = (int)m_videos.size() * 2;  // 2 jobs per video (audio + transcribe)
    char status[128];
    snprintf(status, sizeof(status), "Queued %d jobs for %zu videos...", m_pending_jobs, m_videos.size());
    gtk_label_set_text(GTK_LABEL(m_status_label), status);
    gtk_widget_set_visible(m_progress_bar, TRUE);
    
    // Queue transcription jobs for each video
    for (size_t i = 0; i < m_videos.size(); ++i) {
        const auto& video_path = m_videos[i];
        std::string video_id = "video_" + std::to_string(i);
        
        std::string audio_path = "/tmp/lucid_" + std::to_string(i) + ".wav";
        
        // Queue audio extraction job
        m_job_manager->queue_extract_audio(video_path, audio_path);
        
        // Queue transcription job  
        m_job_manager->queue_transcribe(video_path, video_id);
    }
    
    // Set up completion callback only (not per-progress)
    m_job_manager->set_on_job_completed([this](const BackgroundJob& job) {
        m_pending_jobs--;
        
        // Find and update the corresponding video state
        for (size_t i = 0; i < m_videos.size(); ++i) {
            if (m_videos[i] == job.target_path && i < m_video_states.size()) {
                m_video_states[i].status = VideoState::Status::COMPLETED;
                m_video_states[i].progress = 1.0f;
                break;
            }
        }
        
        // Update on main thread
        g_idle_add_full(G_PRIORITY_DEFAULT, [](gpointer user_data) -> gboolean {
            auto* self = static_cast<MainWindow*>(user_data);
            self->update_status_bar();
            
            if (self->m_pending_jobs <= 0) {
                gtk_label_set_text(GTK_LABEL(self->m_status_label), "Ready");
                gtk_widget_set_visible(self->m_progress_bar, FALSE);
                // Refresh list only when all done
                self->refresh_video_list();
            }
            return G_SOURCE_REMOVE;
        }, this, nullptr);
    });
}

void MainWindow::update_status_bar() {
    auto jobs = m_job_manager->get_all_jobs();
    
    int running = 0, completed = 0, failed = 0;
    float total_progress = 0.0f;
    
    for (const auto& job : jobs) {
        if (job.status == JobStatus::RUNNING) {
            running++;
            total_progress += job.progress;
        } else if (job.status == JobStatus::COMPLETED) {
            completed++;
            total_progress += 1.0f;
        } else if (job.status == JobStatus::FAILED) {
            failed++;
        }
    }
    
    int total = running + completed + failed;
    if (total > 0) {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(m_progress_bar), total_progress / total);
        
        char status[128];
        if (running > 0) {
            snprintf(status, sizeof(status), "Processing %d of %d videos...", 
                     completed + running, (int)m_videos.size());
        } else {
            snprintf(status, sizeof(status), "Completed %d videos", completed);
        }
        gtk_label_set_text(GTK_LABEL(m_status_label), status);
    }
}

// 🔄 REMOVED: update_video_progress() - now handled by AnalysisQueuePanel
// The queue panel shows progress for the currently processing video.
// Video cards now only show static state labels (Pending/Processing/Indexed/Failed)

void MainWindow::register_analysis_callbacks(Glib::RefPtr<LucidApp> app) {
    if (!app) {
        LUCID_ERROR("[MainWindow] register_analysis_callbacks: app is NULL");
        return;
    }
    
    // Store app reference for pause/resume functionality
    m_app = app;
    
    LUCID_INFO("[MainWindow] register_analysis_callbacks with app pointer: %p", (void*)app.get());
    
    // Set progress callback - route to both queue panel and update_video_progress
    // Updated for pass-aware 2-pass analysis
    auto* self = this;
    app->set_analysis_progress_callback([self](const std::string& video_id,
                                                     const std::string& pass_state,
                                                     const std::string& phase,
                                                     float progress,
                                                     int vision_analyzed,
                                                     int vision_total) {
        // Schedule UI update on main thread - use g_timeout_add_full to ensure it runs
        g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE, 0,
            +[](gpointer data) -> gboolean {
                auto* args = static_cast<std::tuple<MainWindow*, std::string, std::string, std::string, float, int, int>*>(data);
                auto* win = std::get<0>(*args);
                std::string vid = std::get<1>(*args);
                std::string pass = std::get<2>(*args);
                std::string ph = std::get<3>(*args);
                float prog = std::get<4>(*args);
                int vis_ana = std::get<5>(*args);
                int vis_tot = std::get<6>(*args);
                
                // CRITICAL: Check shutdown flag before accessing any members
                if (win->is_shutting_down()) {
                    LUCID_DEBUG("[MainWindow] Shutdown in progress, skipping progress callback");
                    delete args;
                    return G_SOURCE_REMOVE;
                }
                
                // Update queue panel with progress - pass-aware
                if (win->m_analysis_queue_panel) {
                    win->m_analysis_queue_panel->on_progress(vid, pass, ph, prog, vis_ana, vis_tot);
                }
                
                delete args;
                return G_SOURCE_REMOVE;
            },
            new std::tuple(self, video_id, pass_state, phase, progress, vision_analyzed, vision_total),
            nullptr);
    });
    
    // Set completion callback - route to queue panel
    LUCID_INFO("Setting up analysis complete callback");
    app->set_analysis_complete_callback([self](const std::string& video_id,
                                                      bool success,
                                                      const std::string& error) {
        LUCID_INFO("Analysis complete callback triggered: video_id=%s, success=%d", 
                   video_id.c_str(), success);
        
        // Schedule UI update on main thread - use g_timeout_add_seconds to ensure it runs
        g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE, 0,
            +[](gpointer data) -> gboolean {
                auto* args = static_cast<std::tuple<MainWindow*, std::string, bool, std::string>*>(data);
                auto* win = std::get<0>(*args);
                std::string vid = std::get<1>(*args);
                bool ok = std::get<2>(*args);
                std::string err = std::get<3>(*args);
                
                // CRITICAL: Check shutdown flag before accessing any members
                if (win->is_shutting_down()) {
                    LUCID_DEBUG("[MainWindow] Shutdown in progress, skipping completion callback");
                    delete args;
                    return G_SOURCE_REMOVE;
                }
                
                LUCID_INFO("g_idle callback executing: calling on_complete for video_id=%s", vid.c_str());
                
                // Update queue panel with completion
                if (win->m_analysis_queue_panel) {
                    win->m_analysis_queue_panel->on_complete(vid, ok, err);
                }
                
                // Update status
                if (ok) {
                    gtk_label_set_text(GTK_LABEL(win->m_status_label), "Analysis complete");
                } else {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Analysis failed: %s", err.c_str());
                    gtk_label_set_text(GTK_LABEL(win->m_status_label), msg);
                }
                
                delete args;
                return G_SOURCE_REMOVE;
            },
            new std::tuple(self, video_id, success, error),
            nullptr);
    });
    
    // Set vision update callback - called after Pass 2 completes
    LUCID_DEBUG("Setting up vision update callback");
    app->set_vision_update_callback([self](const std::string& video_id) {
        LUCID_INFO("Vision update callback triggered for video: %s", video_id.c_str());
        
        // Schedule UI update on main thread
        g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE, 0,
            +[](gpointer data) -> gboolean {
                auto* args = static_cast<std::tuple<MainWindow*, std::string>*>(data);
                auto* win = std::get<0>(*args);
                std::string vid = std::get<1>(*args);
                
                // CRITICAL: Check shutdown flag before accessing any members
                if (win->is_shutting_down()) {
                    LUCID_DEBUG("[MainWindow] Shutdown in progress, skipping vision update callback");
                    delete args;
                    return G_SOURCE_REMOVE;
                }
                
                LUCID_DEBUG("Vision update: refreshing timeline for video_id=%s", vid.c_str());
                win->refresh_timeline();
                
                delete args;
                return G_SOURCE_REMOVE;
            },
            new std::tuple(self, video_id),
            nullptr);
    });
    
    // Set title lookup for queue panel (so it can show video titles)
    if (m_analysis_queue_panel && m_db) {
        m_analysis_queue_panel->set_title_lookup([this](const std::string& video_id) {
            auto video = m_db->get_video(video_id);
            if (video.has_value()) {
                return video->title;
            }
            return video_id;
        });
        
        // Set reset callback for the "Reset Queue" button
        m_analysis_queue_panel->set_reset_callback([this]() {
            // Reset all pending/processing jobs to pending status
            LUCID_DEBUG("Resetting analysis queue...");
            
            if (m_db) {
                auto videos = m_db->get_all_videos();
                for (const auto& vid : videos) {
                    if (vid.status == VideoStatus::ANALYZING || vid.status == VideoStatus::TRANSCRIBING || 
                        vid.status == VideoStatus::THUMBNAILING) {
                        m_db->update_video_status(vid.id, VideoStatus::PENDING);
                    }
                }
            }
            
            // Refresh the queue panel
            if (m_analysis_queue_panel) {
                int pending = 0, processing = 0;
                if (m_db) {
                    auto videos = m_db->get_all_videos();
                    for (const auto& vid : videos) {
                        if (vid.status == VideoStatus::PENDING) pending++;
                        else if (vid.status == VideoStatus::ANALYZING || vid.status == VideoStatus::TRANSCRIBING || 
                                 vid.status == VideoStatus::THUMBNAILING) processing++;
                    }
                }
                m_analysis_queue_panel->update_queue(pending, processing);
                m_analysis_queue_panel->refresh();
            }
            
            LUCID_INFO("Analysis queue reset complete");
        });
        
        // Set re-analyze all callback
        m_analysis_queue_panel->set_reanalyze_all_callback([this, app]() {
            LUCID_DEBUG("Re-analyzing all videos...");
            
            // Check if analysis is currently in progress
            auto worker = app->get_analysis_worker();
            if (worker && worker->is_processing()) {
                LUCID_WARN("Cannot re-analyze while analysis is in progress");
                // Optionally show a dialog or toast message
                return;
            }
            
            if (m_db) {
                // Reset completed videos to pending (clears markers/thumbnails)
                m_db->reset_completed_to_pending();
                LUCID_INFO("All completed videos reset to pending for re-analysis");
                
                // Trigger worker to pick up new jobs
                auto worker = app->get_analysis_worker();
                if (worker) {
                    worker->trigger_check();
                    LUCID_DEBUG("Triggered analysis worker to pick up pending videos");
                }
            }
            
            // Refresh the queue panel
            if (m_analysis_queue_panel) {
                m_analysis_queue_panel->refresh();
                if (m_db) {
                    QueueStats stats;
                    stats.total_videos = m_db->get_total_video_count();
                    stats.pending = m_db->get_video_count_by_status(VideoStatus::PENDING);
                    stats.completed = m_db->get_video_count_by_status(VideoStatus::COMPLETED);
                    stats.failed = m_db->get_video_count_by_status(VideoStatus::FAILED);
                    stats.total_markers = m_db->get_marker_count();
                    m_analysis_queue_panel->update_stats(stats);
                }
            }
        });
        
        // Set clear all data callback
        m_analysis_queue_panel->set_clear_all_callback([this, app]() {
            LUCID_INFO("CLEAR ALL: Killing video processing and clearing database...");
            
            // IMMEDIATELY kill all processing threads - NO GRACEFUL WAIT
            auto worker = app->get_analysis_worker();
            if (worker) {
                LUCID_INFO("CLEAR ALL: Forcing worker stop - KILLING THREADS");
                worker->stop();  // This kills the threads immediately
                std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Brief pause for cleanup
                worker->restart();  // RESTART - clear all flags so new videos can be processed
                LUCID_INFO("CLEAR ALL: Worker threads killed and restarted");
            }
            
            // Delete ALL videos and all associated data from database
            if (m_db) {
                m_db->delete_all_videos();
                LUCID_INFO("CLEAR ALL: All database entries deleted");
            }
            
            // Clear the Analysis Queue panel display
            if (m_analysis_queue_panel) {
                m_analysis_queue_panel->refresh();
                QueueStats stats;
                stats.total_videos = 0;
                stats.pending = 0;
                stats.completed = 0;
                stats.failed = 0;
                stats.total_markers = 0;
                m_analysis_queue_panel->update_stats(stats);
                LUCID_INFO("CLEAR ALL: Analysis Queue panel cleared");
            }
            
            LUCID_INFO("CLEAR ALL: Complete - database cleared, queue emptied");
        });
        
        // Set cancel callback
        m_analysis_queue_panel->set_cancel_callback([this, app]() {
            LUCID_DEBUG("Cancel requested - will stop at next checkpoint");
            // The worker will check for cancellation at each phase boundary
            if (app) {
                app->request_cancel_analysis();
            }
        });
        
        // Set remove failed callback
        m_analysis_queue_panel->set_remove_failed_callback([this]() {
            LUCID_DEBUG("Removing failed videos...");
            
            if (m_db) {
                int failed_count = m_db->get_video_count_by_status(VideoStatus::FAILED);
                m_db->delete_failed_videos();
                LUCID_INFO("Removed %d failed videos", failed_count);
            }
            
            // Refresh the queue panel
            if (m_analysis_queue_panel) {
                m_analysis_queue_panel->refresh();
                if (m_db) {
                    QueueStats stats;
                    stats.total_videos = m_db->get_total_video_count();
                    stats.pending = m_db->get_video_count_by_status(VideoStatus::PENDING);
                    stats.completed = m_db->get_video_count_by_status(VideoStatus::COMPLETED);
                    stats.failed = m_db->get_video_count_by_status(VideoStatus::FAILED);
                    stats.total_markers = m_db->get_marker_count();
                    m_analysis_queue_panel->update_stats(stats);
                }
            }
        });
        
        // Set stats callback for QueueStats - updated for 2-pass system
        m_analysis_queue_panel->set_stats_callback([this]() -> QueueStats {
            QueueStats stats;
            if (m_db) {
                stats.total_videos = m_db->get_total_video_count();
                stats.pending = m_db->get_video_count_by_status(VideoStatus::PENDING);
                
                // Pass 1 active = ANALYZING, TRANSCRIBING, THUMBNAILING
                stats.pass1_active = m_db->get_video_count_by_status(VideoStatus::ANALYZING) +
                                    m_db->get_video_count_by_status(VideoStatus::TRANSCRIBING) +
                                    m_db->get_video_count_by_status(VideoStatus::THUMBNAILING);
                
                // Pass 2 active = VISION_SCANNING
                stats.pass2_active = m_db->get_video_count_by_status(VideoStatus::VISION_SCANNING);
                
                // completed = COMPLETED (both passes done)
                stats.completed = m_db->get_video_count_by_status(VideoStatus::COMPLETED);
                
                // partial = COMPLETED but with 0 vision markers (Pass 1 done, Pass 2 skipped)
                // Approximate: COMPLETED videos with no VISION markers
                // For simplicity, count COMPLETED as fully done for now
                stats.partial_complete = 0;
                
                stats.failed = m_db->get_video_count_by_status(VideoStatus::FAILED);
                
                // Get real marker counts by type from database
                auto all_videos = m_db->get_all_videos();
                int audio_count = 0;
                int vision_count = 0;
                for (const auto& vid : all_videos) {
                    auto markers = m_db->get_markers(vid.id);
                    for (const auto& m : markers) {
                        if (m.marker_type == MarkerType::AUDIO) {
                            audio_count++;
                        } else if (m.marker_type == MarkerType::VISION) {
                            vision_count++;
                        }
                    }
                }
                stats.audio_markers = audio_count;
                stats.vision_markers = vision_count;
                stats.total_markers = audio_count + vision_count;
            }
            return stats;
        });
        
        // Set pending videos callback for populating the pending list
        m_analysis_queue_panel->set_pending_videos_callback([this]() {
            std::vector<std::pair<std::string, std::string>> result;
            if (m_db) {
                auto videos = m_db->get_all_videos();
                for (const auto& vid : videos) {
                    if (vid.status == VideoStatus::PENDING) {
                        result.push_back({vid.id, vid.title});
                    }
                }
            }
            return result;
        });
        
        // Set video stats callback for getting actual counts from database
        m_analysis_queue_panel->set_video_stats_callback([this](const std::string& video_id, int& thumb_count, int& marker_count, int& vision_count) {
            if (m_db) {
                auto thumbs = m_db->get_thumbnails(video_id);
                thumb_count = (int)thumbs.size();
                
                auto markers = m_db->get_markers(video_id);
                marker_count = (int)markers.size();
                
                auto visions = m_db->get_vision_analyses(video_id);
                vision_count = (int)visions.size();
            } else {
                thumb_count = 0;
                marker_count = 0;
                vision_count = 0;
            }
        });
    }
    
    // Set queue update callback for pending count updates
    app->set_queue_update_callback([self](int pending_count, int processing_count) {
        g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE, 0,
            +[](gpointer data) -> gboolean {
                auto* args = static_cast<std::tuple<MainWindow*, int, int>*>(data);
                auto* win = std::get<0>(*args);
                int pending = std::get<1>(*args);
                int processing = std::get<2>(*args);
                
                // CRITICAL: Check shutdown flag before accessing any members
                if (win->is_shutting_down()) {
                    LUCID_DEBUG("[MainWindow] Shutdown in progress, skipping queue update callback");
                    delete args;
                    return G_SOURCE_REMOVE;
                }
                
                if (win->m_analysis_queue_panel) {
                    win->m_analysis_queue_panel->update_queue(pending, processing);
                    win->m_analysis_queue_panel->update_pending_list();  // Refresh the list
                }
                
                delete args;
                return G_SOURCE_REMOVE;
            },
            new std::tuple(self, pending_count, processing_count),
            nullptr);
    });
    
    LUCID_INFO("Analysis callbacks registered");
}

void MainWindow::refresh_video_list() {
    if (!m_video_grid) return;
    
    // Convert main_window's video data to VideoItem objects
    std::vector<VideoItem> items;
    for (size_t i = 0; i < m_videos.size(); ++i) {
        const auto& video_id = m_video_ids[i];
        const auto& video_path = m_videos[i];
        
        // Get thumbnail path from database
        std::string thumbnail_path;
        auto thumbnails = m_db->get_thumbnails(video_id);
        if (!thumbnails.empty()) {
            thumbnail_path = thumbnails[0].path;
        }
        
        // Create VideoItem
        VideoItem item;
        item.id = video_id;
        item.path = video_path;
        item.title = std::filesystem::path(video_path).filename().string();
        item.thumbnail_path = thumbnail_path;
        
        // Get processing state
        if (i < m_video_states.size()) {
            item.is_transcribing = (m_video_states[i].status == VideoState::Status::TRANSCRIBING);
            item.is_indexed = (m_video_states[i].status == VideoState::Status::COMPLETED);
            item.transcribe_progress = m_video_states[i].progress;
        }
        
        items.push_back(item);
    }
    
    // Set videos in VideoList (this handles pagination internally)
    m_video_grid->set_videos(items);
    m_video_grid->set_total_count(m_videos.size());
    
    // Update video count label
    char count_str[64];
    snprintf(count_str, sizeof(count_str), "%lu videos", (unsigned long)m_videos.size());
    gtk_label_set_text(GTK_LABEL(m_video_count_label), count_str);
}

GtkWidget* MainWindow::create_video_item(const std::string& video_id, const std::string& video_path, 
                                         const VideoState* state, const std::string& thumbnail_path) {
    GtkWidget* frame = gtk_frame_new(nullptr);
    gtk_widget_add_css_class(frame, "video-card");  // Apply card styling
    gtk_widget_set_margin_start(frame, 4);
    gtk_widget_set_margin_end(frame, 4);
    gtk_widget_set_margin_top(frame, 2);
    gtk_widget_set_margin_bottom(frame, 2);
    
    // Store video_id for click handling
    g_object_set_data(G_OBJECT(frame), "video-id", g_strdup(video_id.c_str()));
    
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);
    gtk_frame_set_child(GTK_FRAME(frame), box);
    
    // Thumbnail or video icon - use Picture widget for proper GTK4 scaling
    GtkWidget* thumb_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(thumb_container, FALSE);
    gtk_widget_set_valign(thumb_container, GTK_ALIGN_START);
    
    // Try to load thumbnail image using GdkTexture (GTK4 native)
    if (!thumbnail_path.empty()) {
        if (std::filesystem::exists(thumbnail_path)) {
            // Load the full-size thumbnail - GTK will handle scaling
            GError* error = nullptr;
            GdkTexture* texture = gdk_texture_new_from_file(
                g_file_new_for_path(thumbnail_path.c_str()), &error);
            if (texture) {
                LUCID_DEBUG("Thumbnail loaded successfully: %s", thumbnail_path.c_str());
                // Use GtkPicture for automatic scaling in GTK4
                GtkWidget* picture = gtk_picture_new();
                gtk_picture_set_paintable(GTK_PICTURE(picture), GDK_PAINTABLE(texture));
                
                // Use configurable thumbnail size
                auto [thumb_w, thumb_h] = get_thumbnail_dimensions(m_thumbnail_size);
                gtk_widget_set_size_request(picture, thumb_w, thumb_h);
                gtk_widget_set_valign(picture, GTK_ALIGN_START);
                gtk_box_append(GTK_BOX(thumb_container), picture);
                g_object_unref(texture);
            } else {
                // Fallback icon
                if (error) {
                    LUCID_WARN("Failed to load thumbnail: %s (%s)", thumbnail_path.c_str(), error->message);
                    g_error_free(error);
                } else {
                    LUCID_WARN("Failed to load thumbnail: %s", thumbnail_path.c_str());
                }
                GtkWidget* icon = gtk_image_new_from_icon_name("video-x-generic");
                gtk_image_set_icon_size(GTK_IMAGE(icon), GTK_ICON_SIZE_LARGE);
                gtk_widget_set_valign(icon, GTK_ALIGN_START);
                gtk_box_append(GTK_BOX(thumb_container), icon);
            }
        } else {
            // No thumbnail - show generic icon
            LUCID_WARN("Thumbnail file not found: %s", thumbnail_path.c_str());
            GtkWidget* icon = gtk_image_new_from_icon_name("video-x-generic");
            gtk_image_set_icon_size(GTK_IMAGE(icon), GTK_ICON_SIZE_LARGE);
            gtk_widget_set_valign(icon, GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(thumb_container), icon);
        }
    } else {
        // No thumbnail path - show generic icon
        GtkWidget* icon = gtk_image_new_from_icon_name("video-x-generic");
        gtk_image_set_icon_size(GTK_IMAGE(icon), GTK_ICON_SIZE_LARGE);
        gtk_widget_set_valign(icon, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(thumb_container), icon);
    }
    gtk_box_append(GTK_BOX(box), thumb_container);
    
    // File info
    std::string filename = video_path.substr(video_path.find_last_of('/') + 1);
    
    GtkWidget* info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(info, TRUE);
    gtk_box_append(GTK_BOX(box), info);
    
    GtkWidget* name = gtk_label_new(filename.c_str());
    gtk_label_set_ellipsize(GTK_LABEL(name), PANGO_ELLIPSIZE_END);
    gtk_widget_set_halign(name, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(info), name);
    
    GtkWidget* path_label = gtk_label_new(video_path.c_str());
    gtk_label_set_ellipsize(GTK_LABEL(path_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(path_label), 50);
    gtk_widget_set_halign(path_label, GTK_ALIGN_START);
    gtk_widget_set_opacity(path_label, 0.6);
    gtk_box_append(GTK_BOX(info), path_label);
    
    // =========================================================
    // 🔄 SIMPLIFIED: Removed inline progress bar from video cards
    // Progress is now shown in Analysis Queue panel instead
    // =========================================================
    
    // Status label (simplified - just shows state, no progress bar)
    if (state) {
        const char* status_text = nullptr;
        
        switch (state->status) {
            case VideoState::Status::PENDING:
                status_text = "Pending";
                break;
            case VideoState::Status::EXTRACTING_AUDIO:
            case VideoState::Status::TRANSCRIBING:
                status_text = "Processing";
                break;
            case VideoState::Status::COMPLETED:
                status_text = "Indexed";
                break;
            case VideoState::Status::FAILED:
                status_text = "Failed";
                break;
            default:
                status_text = "";
        }
        
        if (status_text && strlen(status_text) > 0) {
            GtkWidget* status_label = gtk_label_new(status_text);
            gtk_widget_set_halign(status_label, GTK_ALIGN_START);
            
            // Apply color via opacity (GTK4 doesn't have easy color theming)
            // Green-ish for indexed
            if (state->status == VideoState::Status::COMPLETED) {
                gtk_widget_set_opacity(status_label, 0.9);
            }
            // Gray-ish for pending
            else if (state->status == VideoState::Status::PENDING) {
                gtk_widget_set_opacity(status_label, 0.5);
            }
            // Orange-ish for processing
            else if (state->status == VideoState::Status::EXTRACTING_AUDIO ||
                     state->status == VideoState::Status::TRANSCRIBING) {
                gtk_widget_set_opacity(status_label, 0.7);
            }
            // Red-ish for failed
            else if (state->status == VideoState::Status::FAILED) {
                gtk_widget_set_opacity(status_label, 0.8);
            }
            
            gtk_box_append(GTK_BOX(info), status_label);
        }
    }
    
    // File size
    try {
        uintmax_t size = std::filesystem::file_size(video_path);
        char size_str[32];
        if (size > 1024ULL * 1024 * 1024) {
            snprintf(size_str, sizeof(size_str), "%.1f GB", size / (1024.0 * 1024.0 * 1024.0));
        } else if (size > 1024 * 1024) {
            snprintf(size_str, sizeof(size_str), "%.1f MB", size / (1024.0 * 1024.0));
        } else {
            snprintf(size_str, sizeof(size_str), "%.0f KB", size / 1024.0);
        }
        GtkWidget* size_label = gtk_label_new(size_str);
        gtk_widget_set_halign(size_label, GTK_ALIGN_END);
        gtk_box_append(GTK_BOX(box), size_label);
    } catch (...) {}
    
    // Make the row clickable
    gtk_widget_set_cursor_from_name(frame, "pointer");
    
    // Add click gesture to make video item clickable  // 🔄 NEW
    GtkGesture* click_gesture = gtk_gesture_click_new();
    g_signal_connect(click_gesture, "released", G_CALLBACK(+[](GtkGestureClick* self, int n_press, double x, double y, gpointer data) {
        if (n_press == 1) {
            GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(self));
            const char* video_id = static_cast<const char*>(g_object_get_data(G_OBJECT(widget), "video-id"));
            if (video_id) {
                LUCID_DEBUG("[MainWindow] Video card clicked: %s", video_id);
                auto* main_window = static_cast<MainWindow*>(data);
                main_window->show_video_detail(video_id);
            }
        }
    }), this);
    gtk_widget_add_controller(frame, GTK_EVENT_CONTROLLER(click_gesture));
    
    // Track frame for direct updates (🔄 Will be removed in Task 6)
    m_video_frames[video_id] = frame;
    // Note: m_progress_bars no longer used for video cards
    
    return frame;
}

void MainWindow::perform_search(const std::string& raw_query) {
    m_search_query = raw_query;

    if (raw_query.empty()) {
        clear_search();
        refresh_video_list();
        return;
    }

    LUCID_DEBUG("Performing search: '%s'", raw_query.c_str());

    // Only use LLM enhancer if engine is ready and not busy
    std::string query = raw_query;
    std::vector<std::string> expanded_terms;
    
    if (m_llm_engine && m_llm_engine->is_ready() && !m_llm_engine->is_generating()) {
        // Use LLM enhancer
        LLMQueryResult result = m_llm_enhancer->enhance(raw_query);
        query = result.parsed_query.main_text.empty() ? raw_query : result.parsed_query.main_text;
        expanded_terms = result.expanded_terms;
    } else {
        LUCID_DEBUG("[MainWindow] Skipping LLM enhancement (engine not ready or busy)");
    }

    // Search through video list (filename search)
    std::vector<std::string> video_matches;
    std::vector<std::string> video_ids_for_matches;
    std::vector<std::string> search_terms = {query};
    search_terms.insert(search_terms.end(), expanded_terms.begin(), expanded_terms.end());

    for (const auto& v : m_videos) {
        std::string filename = v.substr(v.find_last_of('/') + 1);
        std::string lower_filename = filename;
        std::transform(lower_filename.begin(), lower_filename.end(), lower_filename.begin(), ::tolower);

        bool match = false;
        for (const auto& term : search_terms) {
            std::string lower_term = term;
            std::transform(lower_term.begin(), lower_term.end(), lower_term.begin(), ::tolower);

            if (lower_filename.find(lower_term) != std::string::npos ||
                v.find(term) != std::string::npos) {
                match = true;
                break;
            }
        }

        if (match) {
            video_matches.push_back(v);
            size_t idx = std::find(m_videos.begin(), m_videos.end(), v) - m_videos.begin();
            video_ids_for_matches.push_back(idx < m_video_ids.size() ? m_video_ids[idx] : "");
        }
    }

    // Search markers (transcripts) via FTS
    std::vector<Marker> marker_matches;
    std::vector<SearchResult> semantic_results;

    if (m_db) {
        marker_matches = m_db->search_markers(query, 50);
        LUCID_DEBUG("Search found %zu video matches and %zu marker matches",
                   video_matches.size(), marker_matches.size());
    }

    // Also search via semantic embeddings (llama.cpp)
    if (m_search && m_search->is_initialized()) {
        semantic_results = m_search->search_text(query, 20);
        LUCID_DEBUG("Semantic search found %zu results", semantic_results.size());
    }

    // Store results in member variables 🔄 NEW
    m_stored_search_query = raw_query;
    m_stored_video_matches = video_matches;
    m_stored_video_ids = video_ids_for_matches;
    m_stored_marker_results = marker_matches;
    m_stored_search_results = semantic_results;

    // Navigate to search page and display results 🔄 NEW
    show_search();
}

GtkWidget* MainWindow::create_marker_item(const Marker& marker) {
    // Get video title for this marker
    std::string video_title = marker.video_id;
    if (m_db) {
        auto video_opt = m_db->get_video(marker.video_id);
        if (video_opt) {
            video_title = video_opt->title;
        }
    }
    
    GtkWidget* frame = gtk_frame_new(nullptr);
    gtk_widget_add_css_class(frame, "marker-card");  // Apply card styling
    gtk_widget_set_margin_start(frame, 4);
    gtk_widget_set_margin_end(frame, 4);
    gtk_widget_set_margin_top(frame, 2);
    gtk_widget_set_margin_bottom(frame, 2);
    
    // Store marker data for click handling (use member vars instead of gpointer)
    g_object_set_data(G_OBJECT(frame), "marker-video-id", g_strdup(marker.video_id.c_str()));
    // Store position as string since gpointer can't hold int64 directly
    char pos_str[32];
    snprintf(pos_str, sizeof(pos_str), "%ld", (long)marker.start_ms);
    g_object_set_data(G_OBJECT(frame), "marker-position", g_strdup(pos_str));
    
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);
    gtk_frame_set_child(GTK_FRAME(frame), box);
    
    // Header row: icon + video title + timestamp
    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_hexpand(header, TRUE);
    gtk_box_append(GTK_BOX(box), header);
    
    // Transcript icon
    GtkWidget* icon = gtk_image_new_from_icon_name("text-x-generic");
    gtk_widget_set_size_request(icon, 24, 24);
    gtk_box_append(GTK_BOX(header), icon);
    
    // Video title
    GtkWidget* title = gtk_label_new(video_title.c_str());
    gtk_label_set_ellipsize(GTK_LABEL(title), PANGO_ELLIPSIZE_END);
    gtk_widget_set_hexpand(title, TRUE);
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(header), title);
    
    // Timestamp badge
    GtkWidget* timestamp = gtk_label_new(marker.timecode_start.c_str());
    gtk_label_set_selectable(GTK_LABEL(timestamp), TRUE);
    gtk_widget_set_halign(timestamp, GTK_ALIGN_END);
    gtk_widget_set_valign(timestamp, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(header), timestamp);
    
    // Transcript text snippet
    std::string text = marker.text;
    if (text.length() > 120) {
        text = text.substr(0, 120) + "...";
    }
    GtkWidget* transcript = gtk_label_new(text.c_str());
    gtk_label_set_ellipsize(GTK_LABEL(transcript), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(transcript), 60);
    gtk_widget_set_halign(transcript, GTK_ALIGN_START);
    gtk_widget_set_margin_start(transcript, 32);  // Indent under icon
    gtk_box_append(GTK_BOX(box), transcript);
    
    // Make clickable
    gtk_widget_set_cursor_from_name(frame, "pointer");
    
    GtkGesture* click_gesture = gtk_gesture_click_new();
    g_signal_connect(click_gesture, "released", G_CALLBACK(+[](GtkGestureClick* self, int n_press, double x, double y, gpointer data) {
        if (n_press == 1) {
            GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(self));
            const char* video_id = static_cast<const char*>(g_object_get_data(G_OBJECT(widget), "marker-video-id"));
            
            // Get position from string
            const char* pos_str = static_cast<const char*>(g_object_get_data(G_OBJECT(widget), "marker-position"));
            int64_t position_ms = 0;
            if (pos_str) {
                position_ms = atoll(pos_str);
            }
            
            if (video_id) {
                LUCID_DEBUG("[MainWindow] Marker clicked: video=%s, position=%ld ms", video_id, (long)position_ms);
                auto* main_window = static_cast<MainWindow*>(data);
                main_window->show_video_detail_with_seek(video_id, position_ms);
            }
        }
    }), this);
    gtk_widget_add_controller(frame, GTK_EVENT_CONTROLLER(click_gesture));

    return frame;
}

GtkWidget* MainWindow::create_semantic_result_item(const SearchResult& result) {
    // Get video title for this result
    std::string video_title = result.video_id;
    if (m_db) {
        auto video_opt = m_db->get_video(result.video_id);
        if (video_opt) {
            video_title = video_opt->title;
        }
    }

    GtkWidget* frame = gtk_frame_new(nullptr);
    gtk_widget_add_css_class(frame, "semantic-result");  // Apply card styling
    gtk_widget_set_margin_start(frame, 4);
    gtk_widget_set_margin_end(frame, 4);
    gtk_widget_set_margin_top(frame, 2);
    gtk_widget_set_margin_bottom(frame, 2);

    // Store data for click handling
    g_object_set_data(G_OBJECT(frame), "semantic-video-id", g_strdup(result.video_id.c_str()));
    char pos_str[32];
    snprintf(pos_str, sizeof(pos_str), "%lld", (long long)result.timestamp_ms);
    g_object_set_data(G_OBJECT(frame), "semantic-position", g_strdup(pos_str));

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);
    gtk_frame_set_child(GTK_FRAME(frame), box);

    // Header row: icon + video title + score badge
    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_hexpand(header, TRUE);
    gtk_box_append(GTK_BOX(box), header);

    // Choose icon based on match type
    const char* icon_name = "face-cool-symbolic";  // Default: transcript
    const char* match_label = nullptr;

    if (result.match_type == MatchType::TITLE) {
        icon_name = "audio-volume-high-symbolic";  // Title match - movie/music icon
        match_label = "title";
    } else if (result.match_type == MatchType::TRANSCRIPT) {
        icon_name = "text-x-generic-symbolic";  // Transcript match - document icon
        match_label = "transcript";
    }

    // Match type icon
    GtkWidget* icon = gtk_image_new_from_icon_name(icon_name);
    gtk_widget_set_size_request(icon, 24, 24);
    gtk_box_append(GTK_BOX(header), icon);

    // Video title
    GtkWidget* title = gtk_label_new(video_title.c_str());
    gtk_label_set_ellipsize(GTK_LABEL(title), PANGO_ELLIPSIZE_END);
    gtk_widget_set_hexpand(title, TRUE);
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(header), title);

    // Match type badge
    if (match_label) {
        GtkWidget* badge = gtk_label_new(match_label);
        gtk_widget_add_css_class(badge, "chip");
        gtk_widget_set_halign(badge, GTK_ALIGN_END);
        gtk_box_append(GTK_BOX(header), badge);
    }

    // Relevance score badge
    char score_text[16];
    snprintf(score_text, sizeof(score_text), "%.0f%%", result.relevance_score * 100);
    GtkWidget* score_label = gtk_label_new(score_text);
    gtk_widget_set_halign(score_label, GTK_ALIGN_END);
    gtk_widget_set_valign(score_label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(header), score_label);

    // Timecode badge (if timestamp is available)
    if (result.timestamp_ms > 0) {
        int total_seconds = result.timestamp_ms / 1000;
        int hours = total_seconds / 3600;
        int minutes = (total_seconds % 3600) / 60;
        int seconds = total_seconds % 60;
        char timecode[32];
        if (hours > 0) {
            snprintf(timecode, sizeof(timecode), "%d:%02d:%02d", hours, minutes, seconds);
        } else {
            snprintf(timecode, sizeof(timecode), "%d:%02d", minutes, seconds);
        }
        GtkWidget* timecode_label = gtk_label_new(timecode);
        gtk_widget_set_halign(timecode_label, GTK_ALIGN_END);
        gtk_widget_set_valign(timecode_label, GTK_ALIGN_CENTER);
        gtk_box_append(GTK_BOX(header), timecode_label);
    }

    // Transcript snippet
    std::string text = result.snippet;
    if (text.length() > 120) {
        text = text.substr(0, 120) + "...";
    }
    GtkWidget* snippet_label = gtk_label_new(text.c_str());
    gtk_label_set_ellipsize(GTK_LABEL(snippet_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(snippet_label), 60);
    gtk_widget_set_halign(snippet_label, GTK_ALIGN_START);
    gtk_widget_set_margin_start(snippet_label, 32);
    gtk_box_append(GTK_BOX(box), snippet_label);

    // Make clickable
    gtk_widget_set_cursor_from_name(frame, "pointer");

    GtkGesture* click_gesture = gtk_gesture_click_new();
    g_signal_connect(click_gesture, "released", G_CALLBACK(+[](GtkGestureClick* self, int n_press, double x, double y, gpointer data) {
        if (n_press == 1) {
            GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(self));
            const char* video_id = static_cast<const char*>(g_object_get_data(G_OBJECT(widget), "semantic-video-id"));
            const char* pos_str = static_cast<const char*>(g_object_get_data(G_OBJECT(widget), "semantic-position"));
            int64_t position_ms = pos_str ? atoll(pos_str) : 0;

            if (video_id) {
                LUCID_DEBUG("[MainWindow] Semantic result clicked: video=%s, position=%ld ms", video_id, (long)position_ms);
                auto* main_window = static_cast<MainWindow*>(data);
                main_window->show_video_detail_with_seek(video_id, position_ms);
            }
        }
    }), this);
    gtk_widget_add_controller(frame, GTK_EVENT_CONTROLLER(click_gesture));

    return frame;
}

void MainWindow::load_video(const Glib::ustring& path) {
    LUCID_INFO("Loading video: %s", path.raw().c_str());
}

void MainWindow::setup_keyboard_shortcuts() {
    // Create controller for keyboard shortcuts
    GtkEventController* controller = gtk_shortcut_controller_new();
    
    // Set scope to window (handles shortcuts globally)
    gtk_shortcut_controller_set_scope(GTK_SHORTCUT_CONTROLLER(controller),
                                       GTK_SHORTCUT_SCOPE_GLOBAL);
    
    // Add shortcuts to controller using GAction names
    // Ctrl+O - Open folder
    gtk_shortcut_controller_add_shortcut(
        GTK_SHORTCUT_CONTROLLER(controller),
        gtk_shortcut_new(
            gtk_shortcut_trigger_parse_string("<Ctrl>o"),
            gtk_named_action_new("win.add-folder")
        )
    );
    
    // Ctrl+F - Focus search
    gtk_shortcut_controller_add_shortcut(
        GTK_SHORTCUT_CONTROLLER(controller),
        gtk_shortcut_new(
            gtk_shortcut_trigger_parse_string("<Ctrl>f"),
            gtk_named_action_new("win.focus-search")
        )
    );
    
    // Ctrl+R - Refresh
    gtk_shortcut_controller_add_shortcut(
        GTK_SHORTCUT_CONTROLLER(controller),
        gtk_shortcut_new(
            gtk_shortcut_trigger_parse_string("<Ctrl>r"),
            gtk_named_action_new("win.refresh")
        )
    );
    
    // Ctrl+1 - Go Home
    gtk_shortcut_controller_add_shortcut(
        GTK_SHORTCUT_CONTROLLER(controller),
        gtk_shortcut_new(
            gtk_shortcut_trigger_parse_string("<Ctrl>1"),
            gtk_named_action_new("win.go-home")
        )
    );
    
    // Ctrl+2 - Go Settings
    gtk_shortcut_controller_add_shortcut(
        GTK_SHORTCUT_CONTROLLER(controller),
        gtk_shortcut_new(
            gtk_shortcut_trigger_parse_string("<Ctrl>2"),
            gtk_named_action_new("win.go-settings")
        )
    );
    
    // Ctrl+3 - Go Model Hub
    gtk_shortcut_controller_add_shortcut(
        GTK_SHORTCUT_CONTROLLER(controller),
        gtk_shortcut_new(
            gtk_shortcut_trigger_parse_string("<Ctrl>3"),
            gtk_named_action_new("win.go-model-hub")
        )
    );
    
    // Playback shortcuts - Space: Play/Pause
    gtk_shortcut_controller_add_shortcut(
        GTK_SHORTCUT_CONTROLLER(controller),
        gtk_shortcut_new(
            gtk_shortcut_trigger_parse_string("space"),
            gtk_named_action_new("win.play-pause")
        )
    );
    
    // Left Arrow: Skip back 5 seconds
    gtk_shortcut_controller_add_shortcut(
        GTK_SHORTCUT_CONTROLLER(controller),
        gtk_shortcut_new(
            gtk_shortcut_trigger_parse_string("Left"),
            gtk_named_action_new("win.skip-back")
        )
    );
    
    // Right Arrow: Skip forward 5 seconds
    gtk_shortcut_controller_add_shortcut(
        GTK_SHORTCUT_CONTROLLER(controller),
        gtk_shortcut_new(
            gtk_shortcut_trigger_parse_string("Right"),
            gtk_named_action_new("win.skip-forward")
        )
    );
    
    // M: Toggle mute
    gtk_shortcut_controller_add_shortcut(
        GTK_SHORTCUT_CONTROLLER(controller),
        gtk_shortcut_new(
            gtk_shortcut_trigger_parse_string("m"),
            gtk_named_action_new("win.toggle-mute")
        )
    );
    
    // Ctrl+B - Toggle sidebar
    gtk_shortcut_controller_add_shortcut(
        GTK_SHORTCUT_CONTROLLER(controller),
        gtk_shortcut_new(
            gtk_shortcut_trigger_parse_string("<Ctrl>b"),
            gtk_named_action_new("win.toggle-sidebar")
        )
    );
    
    // Ctrl+Q - Quit
    gtk_shortcut_controller_add_shortcut(
        GTK_SHORTCUT_CONTROLLER(controller),
        gtk_shortcut_new(
            gtk_shortcut_trigger_parse_string("<Ctrl>q"),
            gtk_named_action_new("app.quit")
        )
    );
    
    // Add controller to window
    gtk_widget_add_controller(GTK_WIDGET(gobj()), controller);
    
    // Setup GAction-based shortcuts using GtkActionable
    // Each action is created and connected to a handler
    
    // Helper lambda for creating and adding actions
    auto add_action = [this](const char* name, GCallback handler) {
        GAction* action = G_ACTION(g_simple_action_new(name, nullptr));
        g_signal_connect(action, "activate", handler, this);
        
        // Use the widget's action group
        GActionMap* map = G_ACTION_MAP(gobj());
        if (map) {
            g_action_map_add_action(map, action);
        }
        g_object_unref(action);
    };
    
    // Add all the action handlers
    add_action("add-folder", G_CALLBACK(+[](GSimpleAction* action, GVariant* param, gpointer data) {
        (void)action; (void)param;
        auto* self = static_cast<MainWindow*>(data);
        self->on_shortcut_add_folder();
    }));
    
    add_action("focus-search", G_CALLBACK(+[](GSimpleAction* action, GVariant* param, gpointer data) {
        (void)action; (void)param;
        auto* self = static_cast<MainWindow*>(data);
        self->on_shortcut_focus_search();
    }));
    
    add_action("refresh", G_CALLBACK(+[](GSimpleAction* action, GVariant* param, gpointer data) {
        (void)action; (void)param;
        auto* self = static_cast<MainWindow*>(data);
        self->on_shortcut_refresh();
    }));
    
    add_action("go-home", G_CALLBACK(+[](GSimpleAction* action, GVariant* param, gpointer data) {
        (void)action; (void)param;
        auto* self = static_cast<MainWindow*>(data);
        self->on_shortcut_go_home();
    }));
    
    add_action("go-settings", G_CALLBACK(+[](GSimpleAction* action, GVariant* param, gpointer data) {
        (void)action; (void)param;
        auto* self = static_cast<MainWindow*>(data);
        self->on_shortcut_go_settings();
    }));
    
    add_action("go-model-hub", G_CALLBACK(+[](GSimpleAction* action, GVariant* param, gpointer data) {
        (void)action; (void)param;
        auto* self = static_cast<MainWindow*>(data);
        self->on_shortcut_go_model_hub();
    }));
    
    add_action("play-pause", G_CALLBACK(+[](GSimpleAction* action, GVariant* param, gpointer data) {
        (void)action; (void)param;
        auto* self = static_cast<MainWindow*>(data);
        if (self->m_video_player_widget) {
            self->m_video_player_widget->toggle_play_pause();
        }
    }));
    
    add_action("skip-back", G_CALLBACK(+[](GSimpleAction* action, GVariant* param, gpointer data) {
        (void)action; (void)param;
        auto* self = static_cast<MainWindow*>(data);
        if (self->m_video_player_widget) {
            self->m_video_player_widget->skip_backward(5000);  // 5 seconds
        }
    }));
    
    add_action("skip-forward", G_CALLBACK(+[](GSimpleAction* action, GVariant* param, gpointer data) {
        (void)action; (void)param;
        auto* self = static_cast<MainWindow*>(data);
        if (self->m_video_player_widget) {
            self->m_video_player_widget->skip_forward(5000);  // 5 seconds
        }
    }));
    
    add_action("toggle-mute", G_CALLBACK(+[](GSimpleAction* action, GVariant* param, gpointer data) {
        (void)action; (void)param;
        auto* self = static_cast<MainWindow*>(data);
        if (self->m_video_player_widget) {
            self->m_video_player_widget->toggle_mute();
        }
    }));
    
    add_action("toggle-sidebar", G_CALLBACK(+[](GSimpleAction* action, GVariant* param, gpointer data) {
        (void)action; (void)param;
        auto* self = static_cast<MainWindow*>(data);
        self->on_shortcut_toggle_sidebar();
    }));
    
    LUCID_DEBUG("Keyboard shortcuts initialized");
}

// Shortcut handler implementations
void MainWindow::on_shortcut_add_folder() {
    LUCID_DEBUG("Shortcut: Add folder");
    FolderBrowser browser;
    if (browser.run(GTK_WINDOW(gobj()))) {
        scan_folder(browser.get_folder_path());
    }
}

void MainWindow::on_shortcut_focus_search() {
    LUCID_DEBUG("Shortcut: Focus search");
    if (m_search_entry) {
        gtk_widget_grab_focus(m_search_entry);
    }
}

void MainWindow::on_shortcut_refresh() {
    LUCID_DEBUG("Shortcut: Refresh");
    if (!m_current_folder.empty()) {
        scan_folder(m_current_folder);
    } else {
        refresh_video_list();
    }
}

void MainWindow::on_shortcut_toggle_sidebar() {
    LUCID_DEBUG("Shortcut: Toggle sidebar");
    if (m_sidebar) {
        m_sidebar_visible = !m_sidebar_visible;
        gtk_widget_set_visible(m_sidebar, m_sidebar_visible);
    }
}

void MainWindow::on_shortcut_go_home() {
    LUCID_DEBUG("Shortcut: Go home");
    show_home();
}

void MainWindow::on_shortcut_go_settings() {
    LUCID_DEBUG("Shortcut: Go settings");
    show_settings();
}

void MainWindow::on_shortcut_go_model_hub() {
    LUCID_DEBUG("Shortcut: Go model hub");
    show_model_hub();
}

void MainWindow::reset_all_videos_to_pending() {
    // Get the application from the window
    GtkApplication* gtk_app = gtk_window_get_application(GTK_WINDOW(gobj()));
    if (!gtk_app) {
        LUCID_ERROR("Failed to get application from window");
        return;
    }
    
    Glib::RefPtr<LucidApp> app = Glib::make_refptr_for_instance<LucidApp>(
        dynamic_cast<LucidApp*>(Glib::wrap(G_OBJECT(gtk_app)).get()));
    if (!app) {
        LUCID_ERROR("Failed to cast application to LucidApp");
        return;
    }
    
    app->reset_all_videos_to_pending();
    
    // Update the queue panel
    if (m_analysis_queue_panel) {
        m_analysis_queue_panel->update_queue(0, 0);  // Will be updated by callback
    }
}

void MainWindow::refresh_timeline() {
    // Refresh timeline markers if we're on the video detail page
    if (m_current_page != Page::VIDEO_DETAIL || m_current_video_id.empty()) {
        return;
    }
    
    if (!m_db || !m_timeline_widget) {
        return;
    }
    
    LUCID_DEBUG("[MainWindow] Refreshing timeline markers for: %s", m_current_video_id.c_str());
    
    // Get updated markers from database
    auto markers = m_db->get_markers(m_current_video_id);
    std::vector<TimelineMarker> timeline_markers;
    for (const auto& marker : markers) {
        timeline_markers.emplace_back(marker.start_ms, marker.text);
        timeline_markers.back().confidence = marker.confidence;
        timeline_markers.back().thumbnail_path = marker.thumbnail_path;
        timeline_markers.back().marker_type = marker.marker_type;
        // A VISION marker is pending if it has no text (description not yet filled by Qwen2-VL)
        timeline_markers.back().is_pending = 
            (marker.marker_type == MarkerType::VISION && marker.text.empty());
    }
    
    m_timeline_widget->set_markers(timeline_markers);
    LUCID_DEBUG("[MainWindow] Timeline refreshed with %zu markers", timeline_markers.size());
    
    // Also refresh marker panel
    if (m_marker_panel) {
        std::vector<TranscriptionSegment> segments;
        for (const auto& marker : markers) {
            TranscriptionSegment seg;
            seg.start_ms = marker.start_ms;
            seg.end_ms = marker.end_ms;
            seg.text = marker.text;
            seg.confidence = marker.confidence;
            segments.push_back(seg);
        }
        m_marker_panel->set_segments(segments);
    }
}

} // namespace lucid