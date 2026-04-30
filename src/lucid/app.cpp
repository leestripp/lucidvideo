// app.cpp - Lucid Application Implementation
//
// GTK4 Application with MainWindow integration

#include "app.hpp"
#include "logging.hpp"
#include "control/control_server.hpp"
#include "model/database.hpp"
#include "model/video_indexer.hpp"
#include "model/video_analysis_worker.hpp"
#include "model/model_manager.hpp"
#include "gui/main_window.hpp"

#include <adwaita.h>

#include <glib.h>

#include <sys/stat.h>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace lucid {

// Global pointer to the current app instance - needed for static callback functions
// that can't capture context (like g_timeout_add lambdas used in stress testing)
// This is declared extern in app.hpp and defined here
Glib::RefPtr<LucidApp> g_app;

// Helper to dispatch calls to the GTK main thread from control server
// The control server runs in a separate thread, so GTK calls must be dispatched
// This version uses g_idle_add which schedules for when the main loop is idle
// Wrapped in try-catch to prevent crashes from exceptions in dispatched functions
static gboolean control_server_dispatch_helper(gpointer data) {
    try {
        auto* func = static_cast<std::function<void()>*>(data);
        if (func) {
            (*func)();
            delete func;
        }
    } catch (const std::exception& e) {
        LUCID_ERROR("[ControlServer] Exception in dispatch helper: %s", e.what());
    } catch (...) {
        LUCID_ERROR("[ControlServer] Unknown exception in dispatch helper");
    }
    return FALSE;  // One-shot, remove the source
}

static void dispatch_to_main_thread(std::function<void()> func) {
    try {
        auto* func_copy = new std::function<void()>(std::move(func));
        g_idle_add_full(G_PRIORITY_HIGH, control_server_dispatch_helper, func_copy, nullptr);
    } catch (const std::exception& e) {
        LUCID_ERROR("[ControlServer] Failed to dispatch to main thread: %s", e.what());
    } catch (...) {
        LUCID_ERROR("[ControlServer] Failed to dispatch to main thread (unknown error)");
    }
}

// Load CSS styles for the application
static void load_css_styles() {
    const char* css = R"(
        /* Transcription Panel Styles */
        .transcript-segment {
            padding: 8px 12px;
            border-radius: 6px;
            margin: 2px 8px;
            background: transparent;
            transition: background-color 0.15s ease;
        }
        
        .transcript-segment:hover,
        .transcript-segment.hover {
            background: rgba(48, 132, 214, 0.15);
        }
        
        .transcript-segment:active {
            background: rgba(48, 132, 214, 0.25);
        }
        
        /* Timestamp styling */
        .transcript-timestamp {
            font-family: 'SF Mono', 'Monaco', 'Inconsolata', monospace;
            font-size: 0.85em;
            color: #5c8fdb;
            min-width: 50px;
            padding-right: 8px;
        }
        
        /* Transcript text styling */
        .transcript-text {
            color: #e0e0e0;
            line-height: 1.4;
        }
        
        /* Panel header styling */
        .panel-header {
            font-weight: bold;
            font-size: 1.1em;
            padding: 12px;
            color: #ffffff;
        }
        
        /* Empty state styling */
        .empty-state {
            color: #888888;
            font-style: italic;
            padding: 24px;
            text-align: center;
        }
        
        /* Search result hover */
        .search-result:hover {
            background: rgba(255, 255, 255, 0.05);
        }
        
        /* Analysis queue item styling */
        .queue-item {
            padding: 8px 12px;
            border-radius: 4px;
            margin: 2px 4px;
        }
        
        .queue-item:hover {
            background: rgba(255, 255, 255, 0.05);
        }
        
        .queue-item.completed {
            color: #4caf50;
        }
        
        .queue-item.failed {
            color: #f44336;
        }
        
        .queue-item.processing {
            color: #2196f3;
        }
        
        .queue-item.pending {
            color: #ff9800;
        }
        
        /* Progress bar styling */
        .progress-text {
            font-size: 0.9em;
            color: #888888;
        }
        
        /* Video card hover */
        .video-card:hover {
            background: rgba(255, 255, 255, 0.08);
        }
        
        .video-card:active {
            background: rgba(255, 255, 255, 0.12);
        }
        
        /* Video card with rounded corners and subtle border */
        .video-card {
            border-radius: 8px;
            border: 1px solid rgba(255, 255, 255, 0.1);
            background: rgba(255, 255, 255, 0.03);
            margin: 4px;
            transition: all 0.2s ease;
        }
        
        .video-card:hover {
            border-color: rgba(255, 255, 255, 0.2);
            background: rgba(255, 255, 255, 0.06);
            box-shadow: 0 2px 8px rgba(0, 0, 0, 0.3);
        }
        
        /* Marker card styling */
        .marker-card {
            border-radius: 6px;
            border: 1px solid rgba(255, 255, 255, 0.08);
            background: rgba(255, 255, 255, 0.02);
            margin: 2px 8px;
            transition: all 0.15s ease;
        }
        
        .marker-card:hover {
            background: rgba(48, 132, 214, 0.15);
            border-color: rgba(48, 132, 214, 0.4);
        }
        
        /* Semantic result card */
        .semantic-result {
            border-radius: 6px;
            border: 1px solid rgba(255, 255, 255, 0.08);
            background: rgba(255, 255, 255, 0.02);
            margin: 2px 8px;
            padding: 8px 12px;
            transition: all 0.15s ease;
        }
        
        .semantic-result:hover {
            background: rgba(156, 39, 176, 0.15);
            border-color: rgba(156, 39, 176, 0.4);
        }
        
        /* Sidebar button styling */
        .sidebar-btn {
            border-radius: 6px;
            transition: background-color 0.15s ease;
        }
        
        .sidebar-btn:hover {
            background: rgba(255, 255, 255, 0.1);
        }
        
        .sidebar-btn:active {
            background: rgba(255, 255, 255, 0.15);
        }
        
        /* Match type badges */
        .badge-title {
            background: rgba(76, 175, 80, 0.2);
            color: #81c784;
            padding: 2px 8px;
            border-radius: 12px;
            font-size: 0.75em;
        }
        
        .badge-transcript {
            background: rgba(33, 150, 243, 0.2);
            color: #64b5f6;
            padding: 2px 8px;
            border-radius: 12px;
            font-size: 0.75em;
        }
        
        /* Status badge colors */
        .status-pending {
            color: #9e9e9e;
        }
        
        .status-processing {
            color: #2196f3;
        }
        
        .status-completed {
            color: #4caf50;
        }
        
        .status-failed {
            color: #f44336;
        }
        
        /* Toast notification styling */
        .toast {
            border-radius: 8px;
            padding: 12px 16px;
            box-shadow: 0 4px 12px rgba(0, 0, 0, 0.3);
            font-weight: 500;
        }
        
        /* Chat bubble styles */
        .chat-bubble-user {
            background: rgba(48, 132, 214, 0.8);
            border-radius: 16px 16px 4px 16px;
            padding: 10px 14px;
            margin: 4px 0;
        }
        
        .chat-bubble-assistant {
            background: rgba(60, 60, 65, 0.9);
            border-radius: 16px 16px 16px 4px;
            padding: 10px 14px;
            margin: 4px 0;
        }
        
        .chat-bubble-assistant.error {
            background: rgba(244, 67, 54, 0.2);
            border: 1px solid rgba(244, 67, 54, 0.4);
        }
    )";
    
    GtkCssProvider* provider = gtk_css_provider_new();
    GdkDisplay* display = gdk_display_get_default();
    if (display) {
        gtk_css_provider_load_from_string(provider, css);
        gtk_style_context_add_provider_for_display(
            display,
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
        g_object_unref(provider);
        LUCID_DEBUG("[LucidApp] CSS styles loaded");
    }
}

Glib::RefPtr<LucidApp> LucidApp::create() {
    return Glib::RefPtr<LucidApp>(new LucidApp());
}

LucidApp::LucidApp()
    : Gtk::Application("org.lucidvideo.LucidVideo", Gio::Application::Flags::NON_UNIQUE) {
    
    LUCID_DEBUG("LucidApp constructing...");
    
    // Initialize Adwaita dark mode support - use system preference
    AdwStyleManager* style_manager = adw_style_manager_get_default();
    adw_style_manager_set_color_scheme(style_manager, ADW_COLOR_SCHEME_PREFER_DARK);
    LUCID_DEBUG("[LucidApp] Adwaita dark mode initialized with PREFER_DARK");
    
    // Load CSS styles
    load_css_styles();
    
    // Create user data directory
    auto data_dir = Glib::build_filename(Glib::get_user_data_dir(), "lucid");
    std::filesystem::create_directories(data_dir);
    
    // Initialize database
    auto db_path = Glib::build_filename(data_dir, "lucid.db");
    
    try {
        m_database = std::make_unique<Database>(db_path);
        LUCID_INFO("Database initialized at: %s", db_path.c_str());
    } catch (const std::exception& e) {
        LUCID_ERROR("Failed to initialize database: %s", e.what());
        // Don't throw - allow app to continue without database
    }
    
    try {
        m_indexer = std::make_unique<VideoIndexer>(*m_database);
        LUCID_INFO("Video indexer initialized");
    } catch (const std::exception& e) {
        LUCID_ERROR("Failed to initialize video indexer: %s", e.what());
    }
    
    // Initialize model manager (builds catalog, loads config)
    ModelManager::instance().initialize();
    LUCID_INFO("Model manager initialized");
    
    // Initialize semantic search for API access (AFTER ModelManager is ready)
    try {
        // Get embedding model path from ModelManager
        auto embed_path = ModelManager::instance().get_selected_embedding_path();
        if (embed_path.has_value() && !embed_path->empty()) {
            int context_size = ModelManager::instance().get_embedding_context_size();
            m_semantic_search = std::make_unique<SemanticSearch>(*m_database);
            if (m_semantic_search->initialize(embed_path.value(), context_size)) {
                LUCID_INFO("Semantic search initialized (embedding: %s, dim=%d)", 
                          embed_path.value().c_str(), m_semantic_search->get_embedding_dim());
            } else {
                LUCID_WARN("Semantic search initialization failed - search may not work");
            }
        } else {
            LUCID_WARN("Semantic search not initialized - no embedding model found");
        }
    } catch (const std::exception& e) {
        LUCID_ERROR("Failed to initialize semantic search: %s", e.what());
    }
    
    // NOTE: setup_analysis_worker() is called AFTER create_window() in on_activate()
    // This ensures the worker is started AFTER callbacks are registered
    
    // Register quit action for Ctrl+Q
    add_action("quit", [this]() { quit(); });
    
    LUCID_DEBUG("LucidApp constructed");
}

LucidApp::~LucidApp() {
    LUCID_DEBUG("LucidApp DESTRUCTOR called");
    // Stop analysis worker on shutdown
    if (m_analysis_worker) {
        m_analysis_worker->stop();
    }
    LUCID_DEBUG("LucidApp DESTRUCTOR done");
}

void LucidApp::on_activate() {
    LUCID_INFO("on_activate() called");
    LUCID_INFO("Current window count: %zu", get_windows().size());
    
    // Start control server for external API access
    start_control_server();
    
    LUCID_INFO("on_activate: starting window creation");
    
    // Create window if not exists
    if (get_windows().empty()) {
        LUCID_INFO("No windows, calling create_window()");
        create_window();
        LUCID_INFO("create_window() returned, window count: %zu", get_windows().size());
    } else {
        LUCID_INFO("on_activate: window already exists, skipping create");
    }
    
    // Present the first window
    if (!get_windows().empty()) {
        auto win = get_windows().front();
        if (win) {
            LUCID_INFO("Presenting window");
            win->present();
            LUCID_INFO("Window presented");
        } else {
            LUCID_ERROR("Window pointer is null");
        }
    } else {
        LUCID_ERROR("No window to present - window list is empty after create_window()");
    }
}

void LucidApp::on_open(const std::vector<Glib::RefPtr<Gio::File>>& files,
                       const Glib::ustring& /* hint */) {
    LUCID_DEBUG("on_open() called with %zu files", files.size());
    
    on_activate();
    
    for (const auto& file : files) {
        auto path = file->get_path();
        if (!path.empty()) {
            open_video(path);
        }
    }
}

void LucidApp::on_shutdown() {
    LUCID_INFO("Application shutting down...");
    
    // Save settings before shutdown
    AppSettings::instance().save();
    
    // Stop control server
    stop_control_server();
    
    // Stop analysis worker
    if (m_analysis_worker) {
        m_analysis_worker->stop();
    }
    
    // Call parent shutdown
    Gtk::Application::on_shutdown();
}

void LucidApp::create_window() {
    LUCID_DEBUG("Creating main window with MainWindow");
    
    try {
        // Create the main window using MainWindow::create()
        // Store in member variable to prevent destruction
        LUCID_DEBUG("Calling MainWindow::create()...");
        m_window = MainWindow::create();
        LUCID_DEBUG("MainWindow::create() returned, about to add_window");
        
        // Add window to application
        add_window(*m_window);
        LUCID_DEBUG("Window added to application");
        
        // First, create the analysis worker (but don't start it yet)
        create_analysis_worker();  // Creates worker without starting
        LUCID_INFO("create_analysis_worker() completed, m_analysis_worker is %s", 
                   m_analysis_worker ? "SET" : "NULL");
        
        // Now register analysis callbacks (worker is already created)
        m_window->register_analysis_callbacks(Glib::make_refptr_for_instance<LucidApp>(this));
        
        // Finally, start the worker (callbacks are now registered)
        if (m_analysis_worker) {
            m_analysis_worker->start();
            LUCID_INFO("Video analysis worker started");
            
            // Fix: Reset videos that are marked complete but missing thumbnails
            // This handles the case where videos were incorrectly marked complete
            m_analysis_worker->reset_incomplete_videos();
        }
        
        // Show the window
        LUCID_DEBUG("Calling m_window->show()...");
        m_window->show();
        LUCID_DEBUG("m_window->show() returned");
        
        // Check if window is in application
        auto windows = get_windows();
        LUCID_DEBUG("Window count from get_windows(): %zu", windows.size());
        
        // Check window properties
        LUCID_DEBUG("Window visible: %d, mapped: %d, realized: %d", 
                    gtk_widget_get_visible(GTK_WIDGET(m_window->gobj())),
                    gtk_widget_get_mapped(GTK_WIDGET(m_window->gobj())),
                    gtk_widget_get_realized(GTK_WIDGET(m_window->gobj())));
        
        LUCID_INFO("Main window created and displayed");
    } catch (const std::exception& e) {
        LUCID_ERROR("Exception during window creation: %s", e.what());
    } catch (...) {
        LUCID_ERROR("Unknown exception during window creation");
    }
}

void LucidApp::open_video(const Glib::ustring& path) {
    LUCID_INFO("Opening video: %s", path.raw().c_str());
    m_indexer->index_video_async(path.raw());
}

void LucidApp::setup_analysis_worker() {
    if (!m_database) {
        LUCID_WARN("Database not initialized, cannot start analysis worker");
        return;
    }
    
    // Create cache directory
    auto cache_dir = Glib::build_filename(Glib::get_user_cache_dir(), "lucid", "analysis");
    std::filesystem::create_directories(cache_dir);
    
    try {
        m_analysis_worker = std::make_unique<VideoAnalysisWorker>(*m_database, cache_dir);
        
        // Configure idle/active poll intervals to reduce CPU usage when idle
        m_analysis_worker->set_idle_poll_interval(30);   // 30 seconds when idle
        m_analysis_worker->set_active_poll_interval(2);  // 2 seconds when work is available
        
        // Start the worker (callbacks will be set later by MainWindow via set_analysis_*_callback)
        m_analysis_worker->start();
        
        LUCID_INFO("Video analysis worker started");
        LUCID_INFO("  Cache directory: %s", cache_dir.c_str());
        LUCID_INFO("  Idle poll interval: %d seconds", m_analysis_worker->get_idle_poll_interval());
        LUCID_INFO("  Active poll interval: %d seconds", m_analysis_worker->get_active_poll_interval());
        
    } catch (const std::exception& e) {
        LUCID_ERROR("Failed to start analysis worker: %s", e.what());
    }
}

void LucidApp::create_analysis_worker() {
    if (!m_database) {
        LUCID_WARN("Database not initialized, cannot create analysis worker");
        return;
    }
    
    // Create cache directory
    auto cache_dir = Glib::build_filename(Glib::get_user_cache_dir(), "lucid", "analysis");
    std::filesystem::create_directories(cache_dir);
    
    try {
        m_analysis_worker = std::make_unique<VideoAnalysisWorker>(*m_database, cache_dir);
        
        LUCID_INFO("Analysis worker created");
        LUCID_INFO("  Cache directory: %s", cache_dir.c_str());
        LUCID_INFO("  Poll interval: %d seconds", m_analysis_worker->get_poll_interval());
        
    } catch (const std::exception& e) {
        LUCID_ERROR("Failed to create analysis worker: %s", e.what());
    }
}

void LucidApp::set_analysis_progress_callback(
    std::function<void(const std::string&, const std::string&, const std::string&, float, int, int)> cb) {
    if (m_analysis_worker) {
        // Convert from App-level callback (passes all params) to Worker-level callback
        m_analysis_worker->set_progress_callback(
            [cb](const std::string& video_id, const std::string& pass_state,
                 const std::string& phase, float progress, int vision_analyzed, int vision_total) {
                cb(video_id, pass_state, phase, progress, vision_analyzed, vision_total);
            });
    }
}

void LucidApp::set_analysis_complete_callback(std::function<void(const std::string&, bool, const std::string&)> cb) {
    LUCID_INFO("LucidApp::set_analysis_complete_callback called");
    LUCID_INFO("LucidApp: m_analysis_worker is %s", m_analysis_worker ? "SET" : "NULL");
    if (m_analysis_worker) {
        LUCID_INFO("LucidApp: m_analysis_worker exists, calling set_completion_callback");
        m_analysis_worker->set_completion_callback(std::move(cb));
        LUCID_INFO("LucidApp: set_completion_callback done");
    } else {
        LUCID_WARN("LucidApp: m_analysis_worker is NULL - callbacks will NOT be set!");
    }
}

void LucidApp::set_queue_update_callback(std::function<void(int, int)> cb) {
    if (m_analysis_worker) {
        m_analysis_worker->set_queue_update_callback(std::move(cb));
    }
}

void LucidApp::set_vision_update_callback(std::function<void(const std::string&)> cb) {
    if (m_analysis_worker) {
        m_analysis_worker->set_vision_update_callback(std::move(cb));
    }
}

void LucidApp::reset_all_videos_to_pending() {
    if (m_analysis_worker) {
        m_analysis_worker->reset_all_to_pending();
    }
}

void LucidApp::request_cancel_analysis() {
    if (m_analysis_worker) {
        m_analysis_worker->request_cancel();
    }
}

void LucidApp::pause_analysis() {
    if (m_analysis_worker) {
        m_analysis_worker->pause();
    }
}

void LucidApp::resume_analysis() {
    if (m_analysis_worker) {
        m_analysis_worker->resume();
    }
}

bool LucidApp::is_analysis_paused() const {
    return m_analysis_worker && m_analysis_worker->is_paused();
}

void LucidApp::start_control_server() {
    if (m_control_server) {
        LUCID_WARN("[ControlServer] Already running");
        return;
    }
    
    m_control_server = std::make_unique<ControlServer>();
    
    // Setup command handlers
    setup_control_server_handlers();
    
    // Start the server
    if (!m_control_server->start()) {
        LUCID_ERROR("[ControlServer] Failed to start");
        m_control_server.reset();
        return;
    }
    
    LUCID_INFO("[ControlServer] Control server started on http://127.0.0.1:8765");
}

void LucidApp::stop_control_server() {
    if (m_control_server) {
        m_control_server->stop();
        m_control_server.reset();
    }
}

void LucidApp::setup_control_server_handlers() {
    if (!m_control_server) return;
    
    // GET /api/settings - Get all settings
    m_control_server->register_handler("get_settings", [this](const Json::Value& /*args*/) -> Json::Value {
        Json::Value settings;
        auto& mm = ModelManager::instance();
        
        settings["whisper_model"] = mm.get_selected_whisper_model();
        settings["embedding_model"] = mm.get_selected_embedding_model();
        settings["llm_model"] = mm.get_selected_llm_model();
        settings["use_gpu"] = mm.get_use_gpu();
        settings["max_parallel_jobs"] = mm.get_max_parallel_jobs();
        settings["embedding_context_size"] = mm.get_embedding_context_size();
        settings["whisper_context_size"] = mm.get_whisper_context_size();
        settings["thumbnail_size"] = mm.get_thumbnail_size();
        settings["vision_scan_enabled"] = true;  // Always enabled
        settings["vision_scan_interval"] = mm.get_vision_scan_interval();
        settings["vision_max_per_video"] = mm.get_vision_max_per_video();
        settings["watched_folders"] = Json::arrayValue;
        for (const auto& folder : mm.get_watched_folders()) {
            settings["watched_folders"].append(folder);
        }
        
        return settings;
    });
    
    // POST /api/settings/vision - Set vision scan settings (always enabled)
    m_control_server->register_handler("set_vision_scan", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        auto& mm = ModelManager::instance();
        
        result["vision_scan_enabled"] = true;  // Always enabled
        
        if (args.isMember("interval")) {
            int interval = args["interval"].asInt();
            mm.set_vision_scan_interval(interval);
            result["vision_scan_interval"] = interval;
        }
        
        if (args.isMember("max_per_video")) {
            int max_per_video = args["max_per_video"].asInt();
            mm.set_vision_max_per_video(max_per_video);
            result["vision_max_per_video"] = max_per_video;
        }
        
        result["success"] = true;
        return result;
    });
    
    // POST /api/command - Generic command
    m_control_server->register_handler("set_whisper_model", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        if (args.isMember("model")) {
            std::string model = args["model"].asString();
            ModelManager::instance().set_selected_whisper_model(model);
            result["success"] = true;
            result["whisper_model"] = model;
        } else {
            result["error"] = "Missing 'model' parameter";
        }
        return result;
    });
    
    // Set LLM model (for vision scanning - switch between Q4_K_M and f16)
    m_control_server->register_handler("set_llm_model", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        if (args.isMember("model")) {
            std::string model = args["model"].asString();
            ModelManager::instance().set_selected_llm_model(model);
            result["success"] = true;
            result["llm_model"] = model;
            
            // Log the new model path for vision
            auto path = ModelManager::instance().get_vision_model_path();
            if (path.has_value()) {
                result["model_path"] = path.value();
            }
            auto mmproj = ModelManager::instance().get_vision_mmproj_path();
            if (mmproj.has_value()) {
                result["mmproj_path"] = mmproj.value();
            }
        } else {
            result["error"] = "Missing 'model' parameter";
        }
        return result;
    });
    
    m_control_server->register_handler("set_embedding_model", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        if (args.isMember("model")) {
            std::string model = args["model"].asString();
            ModelManager::instance().set_selected_embedding_model(model);
            result["success"] = true;
            result["embedding_model"] = model;
        } else {
            result["error"] = "Missing 'model' parameter";
        }
        return result;
    });
    
    m_control_server->register_handler("set_use_gpu", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        if (args.isMember("enabled")) {
            bool enabled = args["enabled"].asBool();
            ModelManager::instance().set_use_gpu(enabled);
            result["success"] = true;
            result["use_gpu"] = enabled;
        } else {
            result["error"] = "Missing 'enabled' parameter";
        }
        return result;
    });
    
    // Thumbnail size: 0=small, 1=medium, 2=large
    m_control_server->register_handler("set_thumbnail_size", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        if (args.isMember("size")) {
            int size = args["size"].asInt();
            if (size < 0 || size > 2) {
                result["error"] = "Invalid size. Use 0 (small), 1 (medium), or 2 (large)";
                return result;
            }
            ModelManager::instance().set_thumbnail_size(size);
            result["success"] = true;
            result["thumbnail_size"] = size;
            result["thumbnail_size_name"] = (size == 0 ? "small" : size == 1 ? "medium" : "large");
        } else {
            result["error"] = "Missing 'size' parameter (0=small, 1=medium, 2=large)";
        }
        return result;
    });
    
    // Max parallel jobs for processing
    m_control_server->register_handler("set_max_parallel_jobs", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        if (args.isMember("jobs")) {
            int jobs = args["jobs"].asInt();
            if (jobs < 1 || jobs > 16) {
                result["error"] = "Invalid jobs count. Use 1-16";
                return result;
            }
            ModelManager::instance().set_max_parallel_jobs(jobs);
            result["success"] = true;
            result["max_parallel_jobs"] = jobs;
        } else {
            result["error"] = "Missing 'jobs' parameter (1-16)";
        }
        return result;
    });
    
    // Whisper context size
    m_control_server->register_handler("set_whisper_context_size", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        if (args.isMember("size")) {
            int size = args["size"].asInt();
            if (size < 1024 || size > 65536) {
                result["error"] = "Invalid size. Use 1024-65536";
                return result;
            }
            ModelManager::instance().set_whisper_context_size(size);
            result["success"] = true;
            result["whisper_context_size"] = size;
        } else {
            result["error"] = "Missing 'size' parameter (1024-65536)";
        }
        return result;
    });
    
    // Embedding context size
    m_control_server->register_handler("set_embedding_context_size", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        if (args.isMember("size")) {
            int size = args["size"].asInt();
            if (size < 256 || size > 8192) {
                result["error"] = "Invalid size. Use 256-8192";
                return result;
            }
            ModelManager::instance().set_embedding_context_size(size);
            result["success"] = true;
            result["embedding_context_size"] = size;
        } else {
            result["error"] = "Missing 'size' parameter (256-8192)";
        }
        return result;
    });
    
    // ==================== Pagination Control Commands ====================
    
    // Set current video page
    m_control_server->register_handler("set_page", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        if (args.isMember("page")) {
            int page = args["page"].asInt();
            if (page < 0) {
                result["error"] = "Page must be >= 0";
                return result;
            }
            
            // Access MainWindow through m_window
            if (m_window) {
                // Dispatch GTK call to main thread (async)
                dispatch_to_main_thread([this, page]() {
                    m_window->set_video_page(page);
                });
                
                result["success"] = true;
                result["message"] = "Page change dispatched";
                // Return the values
                result["current_page"] = page;
                result["page_size"] = m_window->get_video_page_size();
                result["total_pages"] = m_window->get_video_total_pages();
                result["total_items"] = m_window->get_video_total();
                
                LUCID_INFO("[ControlServer] set_page: %d", page);
            } else {
                result["error"] = "MainWindow not available";
            }
        } else {
            result["error"] = "Missing 'page' parameter";
        }
        return result;
    });
    
    // Set page size (videos per page)
    m_control_server->register_handler("set_page_size", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        LUCID_INFO("[ControlServer] set_page_size command received");
        
        if (args.isMember("page_size")) {
            int page_size = args["page_size"].asInt();
            
            // Validate page size
            if (page_size != 25 && page_size != 50 && page_size != 100 && page_size != 200) {
                result["error"] = "Invalid page_size. Use 25, 50, 100, or 200";
                return result;
            }
            
            LUCID_INFO("[ControlServer] set_page_size: %d", page_size);
            
            // Access MainWindow through m_window
            if (m_window) {
                // Dispatch GTK call to main thread (async)
                // The change will happen asynchronously
                dispatch_to_main_thread([this, page_size]() {
                    m_window->set_video_page_size(page_size);
                });
                
                result["success"] = true;
                result["message"] = "Page size change dispatched";
                result["page_size"] = page_size;
                result["current_page"] = 0;  // Page resets on page size change
                result["total_pages"] = 0;   // Unknown until change completes
                result["total_items"] = 0;  // Unknown until change completes
                
                LUCID_INFO("[ControlServer] set_page_size: %d", page_size);
            } else {
                result["error"] = "MainWindow not available";
            }
        } else {
            result["error"] = "Missing 'page_size' parameter (25, 50, 100, or 200)";
        }
        return result;
    });
    
    // Get current pagination status
    m_control_server->register_handler("get_pagination_status", [this](const Json::Value& /*args*/) -> Json::Value {
        Json::Value result;
        
        if (m_window) {
            result["success"] = true;
            result["current_page"] = m_window->get_video_page();
            result["page_size"] = m_window->get_video_page_size();
            result["total_pages"] = m_window->get_video_total_pages();
            result["total_items"] = m_window->get_video_total();
        } else {
            result["success"] = false;
            result["error"] = "MainWindow not available";
        }
        
        return result;
    });
    
    // Add watched folder
    m_control_server->register_handler("add_folder", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        if (args.isMember("path")) {
            std::string path = args["path"].asString();
            if (path.empty()) {
                result["error"] = "Folder path cannot be empty";
                return result;
            }
            
            // Get current folders
            auto folders = ModelManager::instance().get_watched_folders();
            
            // Check if already exists
            if (std::find(folders.begin(), folders.end(), path) != folders.end()) {
                result["success"] = true;
                result["message"] = "Folder already exists";
                result["folder"] = path;
                return result;
            }
            
            // Add folder
            folders.push_back(path);
            ModelManager::instance().set_watched_folders(folders);
            
            result["success"] = true;
            result["folder"] = path;
            result["message"] = "Folder added. Use 'rescan_all' to scan videos.";
        } else {
            result["error"] = "Missing 'path' parameter";
        }
        return result;
    });
    
    // Remove watched folder
    m_control_server->register_handler("remove_folder", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        if (args.isMember("path")) {
            std::string path = args["path"].asString();
            
            // Get current folders
            auto folders = ModelManager::instance().get_watched_folders();
            
            // Find and remove
            auto it = std::find(folders.begin(), folders.end(), path);
            if (it == folders.end()) {
                result["success"] = false;
                result["error"] = "Folder not found: " + path;
                return result;
            }
            
            folders.erase(it);
            ModelManager::instance().set_watched_folders(folders);
            
            result["success"] = true;
            result["folder"] = path;
            result["message"] = "Folder removed";
        } else {
            result["error"] = "Missing 'path' parameter";
        }
        return result;
    });
    
    // Get watched folders
    m_control_server->register_handler("get_watched_folders", [this](const Json::Value& /*args*/) -> Json::Value {
        Json::Value result;
        result["success"] = true;
        for (const auto& folder : ModelManager::instance().get_watched_folders()) {
            result["folders"].append(folder);
        }
        return result;
    });
    
    m_control_server->register_handler("get_status", [this](const Json::Value& /*args*/) -> Json::Value {
        Json::Value status;
        status["app"] = "lucid";
        status["version"] = "1.0.0";
        status["control_server"] = "running";
        
        if (m_analysis_worker) {
            status["analysis_worker"] = "running";
            status["pending_videos"] = 0; // Would need accessor
        } else {
            status["analysis_worker"] = "stopped";
        }
        
        return status;
    });
    
    m_control_server->register_handler("rescan_all", [this](const Json::Value& /*args*/) -> Json::Value {
        Json::Value result;
        if (m_analysis_worker) {
            // First, scan watched folders for any new videos
            auto folders = ModelManager::instance().get_watched_folders();
            int new_videos = 0;
            for (const auto& folder : folders) {
                std::cout << "[INFO] Indexing videos from: " << folder << std::endl;
                if (m_indexer) {
                    m_indexer->index_directory_async(folder);
                    new_videos++;
                }
            }
            
            // Then reset all videos to pending for re-analysis
            m_analysis_worker->reset_all_to_pending();
            
            result["success"] = true;
            if (new_videos > 0) {
                result["message"] = "Indexed " + std::to_string(new_videos) + " folders and reset all videos to pending";
            } else {
                result["message"] = "All videos reset to pending";
            }
        } else {
            result["error"] = "Analysis worker not initialized";
        }
        return result;
    });
    
    m_control_server->register_handler("regenerate_embeddings", [this](const Json::Value& /*args*/) -> Json::Value {
        Json::Value result;
        result["success"] = true;
        
        if (!m_database) {
            result["error"] = "Database not initialized";
            return result;
        }
        
        if (!m_semantic_search) {
            result["error"] = "Semantic search not initialized";
            return result;
        }
        
        int count = 0;
        auto videos = m_database->get_all_videos();
        for (const auto& video : videos) {
            auto markers = m_database->get_markers(video.id);
            for (const auto& marker : markers) {
                // Only regenerate for non-empty markers (audio transcripts and vision descriptions)
                if (!marker.text.empty() && marker.text != "[NO AUDIO]") {
                    // Check if embedding already exists
                    std::string emb_id = video.id + ":" + std::to_string(marker.id);
                    auto existing = m_database->get_embeddings_for_video(video.id);
                    bool found = false;
                    for (const auto& e : existing) {
                        if (e.id == emb_id) {
                            found = true;
                            break;
                        }
                    }
                    
                    if (!found) {
                        // Generate embedding using SemanticSearch's embed_text method
                        auto emb = m_semantic_search->embed_text(marker.text);
                        if (emb.has_value() && !emb->empty()) {
                            m_database->add_embedding(emb_id, marker.text, *emb, marker.start_ms);
                            count++;
                        }
                    }
                }
            }
        }
        
        result["embeddings_generated"] = count;
        result["message"] = "Generated " + std::to_string(count) + " embeddings for existing markers";
        return result;
    });
    
    m_control_server->register_handler("quit", [this](const Json::Value& /*args*/) -> Json::Value {
        LUCID_INFO("[ControlServer] Quit command received");
        Json::Value result;
        result["success"] = true;
        result["message"] = "Application will quit";
        
        // Schedule quit in the main thread (GMainContext)
        g_idle_add([](gpointer user_data) -> gboolean {
            auto* app = static_cast<LucidApp*>(user_data);
            app->quit();
            return G_SOURCE_REMOVE;
        }, this);
        
        return result;
    });
    
    // Navigation commands
    m_control_server->register_handler("navigate", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        
        std::string page = args.get("page", "").asString();
        LUCID_INFO("[ControlServer] Navigate command: page=%s", page.c_str());
        
        if (page.empty()) {
            result["error"] = "Missing 'page' argument";
            return result;
        }
        
        // Map page name to Page enum
        Page target_page;
        if (page == "home") {
            target_page = Page::HOME;
        } else if (page == "search") {
            target_page = Page::SEARCH;
        } else if (page == "video_detail") {
            target_page = Page::VIDEO_DETAIL;
        } else if (page == "analysis_queue") {
            target_page = Page::ANALYSIS_QUEUE;
        } else if (page == "settings") {
            target_page = Page::SETTINGS;
        } else if (page == "model_hub") {
            target_page = Page::MODEL_HUB;
        } else if (page == "chat") {
            target_page = Page::CHAT;
        } else {
            result["error"] = "Unknown page: " + page + ". Valid pages: home, search, video_detail, analysis_queue, settings, model_hub, chat";
            return result;
        }
        
        // Schedule navigation in main thread
        auto* data = new std::pair<Glib::RefPtr<MainWindow>, Page>(m_window, target_page);
        g_idle_add([](gpointer user_data) -> gboolean {
            auto* data = static_cast<std::pair<Glib::RefPtr<MainWindow>, Page>*>(user_data);
            data->first->show_page(data->second);
            delete data;
            return G_SOURCE_REMOVE;
        }, data);
        
        result["success"] = true;
        result["page"] = page;
        result["message"] = "Navigating to: " + page;
        return result;
    });
    
    // Refresh videos command - reloads video list from database
    m_control_server->register_handler("refresh_videos", [this](const Json::Value& /*args*/) -> Json::Value {
        Json::Value result;
        
        // Schedule refresh in main thread via show_home
        g_idle_add([](gpointer user_data) -> gboolean {
            auto* app = static_cast<LucidApp*>(user_data);
            if (app->m_window) {
                app->m_window->show_home();
            }
            return G_SOURCE_REMOVE;
        }, this);
        
        result["success"] = true;
        result["message"] = "Videos refreshed";
        return result;
    });
    
    m_control_server->register_handler("get_current_page", [this](const Json::Value& /*args*/) -> Json::Value {
        Json::Value result;
        if (m_window) {
            result["success"] = true;
            result["current_page"] = m_window->get_current_page_name();
        } else {
            result["error"] = "Window not initialized";
        }
        return result;
    });
    
    // Button click simulation
    m_control_server->register_handler("click_button", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        
        std::string button = args.get("button", "").asString();
        LUCID_INFO("[ControlServer] Click button command: button=%s", button.c_str());
        
        if (button.empty()) {
            result["error"] = "Missing 'button' argument";
            return result;
        }
        
        // Map button names to actions
        std::function<void()> action;
        
        if (button == "add_folder") {
            action = [this]() { 
                if (m_window) m_window->on_add_folder_clicked(nullptr, m_window.get());
            };
        } else if (button == "home") {
            action = [this]() { 
                if (m_window) m_window->show_home();
            };
        } else if (button == "search") {
            action = [this]() { 
                if (m_window) m_window->on_search_clicked(nullptr, m_window.get());
            };
        } else if (button == "analysis_queue") {
            action = [this]() { 
                if (m_window) m_window->show_analysis_queue();
            };
        } else if (button == "settings") {
            action = [this]() { 
                if (m_window) m_window->show_settings();
            };
        } else if (button == "model_hub") {
            action = [this]() { 
                if (m_window) m_window->show_model_hub();
            };
        } else if (button == "chat") {
            action = [this]() { 
                if (m_window) m_window->show_chat();
            };
        } else if (button == "refresh") {
            action = [this]() { 
                if (m_window) m_window->on_shortcut_refresh();
            };
        } else {
            result["error"] = "Unknown button: " + button + ". Valid buttons: add_folder, home, search, analysis_queue, settings, model_hub, chat, refresh";
            return result;
        }
        
        // Schedule button click in main thread
        g_idle_add([](gpointer user_data) -> gboolean {
            auto* func = static_cast<std::function<void()>*>(user_data);
            (*func)();
            delete func;
            return G_SOURCE_REMOVE;
        }, new std::function<void()>(action));
        
        result["success"] = true;
        result["button"] = button;
        result["message"] = "Clicked: " + button;
        return result;
    });
    
    // ========== SETTINGS TESTING COMMANDS ==========
    // These commands simulate user interactions with the settings page for testing
    
    // Navigate to settings and optionally select a specific tab
    m_control_server->register_handler("navigate_to_settings_tab", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        
        std::string tab = args.get("tab", "general").asString();
        LUCID_INFO("[ControlServer] navigate_to_settings_tab: tab=%s", tab.c_str());
        
        // Validate tab name
        std::vector<std::string> valid_tabs = {"general", "folders", "models", "advanced"};
        if (std::find(valid_tabs.begin(), valid_tabs.end(), tab) == valid_tabs.end()) {
            result["error"] = "Invalid tab: " + tab + ". Valid tabs: general, folders, models, advanced";
            return result;
        }
        
        // First navigate to settings page, then switch to tab
        if (m_window) {
            dispatch_to_main_thread([this, tab]() {
                // First show settings page
                m_window->show_settings();
                
                // Then switch to the requested tab (if settings_page exists)
                auto& settings = m_window->m_settings_page;
                if (settings) {
                    settings->set_active_tab(tab);
                    LUCID_INFO("[ControlServer] Switched to settings tab: %s", tab.c_str());
                }
            });
            
            result["success"] = true;
            result["tab"] = tab;
            result["message"] = "Navigated to settings/" + tab;
        } else {
            result["error"] = "Window not available";
        }
        
        return result;
    });
    
    // Get current settings page state (which tab is active, etc.)
    m_control_server->register_handler("get_settings_state", [this](const Json::Value& /*args*/) -> Json::Value {
        Json::Value result;
        
        if (m_window && m_window->m_settings_page) {
            std::string state_json = m_window->m_settings_page->get_settings_state_json();
            
            // Parse the JSON string back into result
            Json::CharReaderBuilder builder;
            std::istringstream iss(state_json);
            std::string errs;
            if (Json::parseFromStream(builder, iss, &result, &errs)) {
                result["success"] = true;
            } else {
                result["success"] = false;
                result["error"] = "Failed to parse settings state: " + errs;
            }
        } else {
            result["success"] = false;
            result["error"] = "Settings page not available";
        }
        
        return result;
    });
    
    // Simulate changing a settings widget value
    // This is for testing - it will trigger the widget's callback
    m_control_server->register_handler("simulate_settings_change", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        
        std::string widget = args.get("widget", "").asString();
        std::string value = args.get("value", "").asString();
        
        if (widget.empty() || value.empty()) {
            result["error"] = "Missing 'widget' or 'value' parameter";
            return result;
        }
        
        LUCID_INFO("[ControlServer] simulate_settings_change: widget=%s, value=%s", widget.c_str(), value.c_str());
        
        // Validate widget name
        std::vector<std::string> valid_widgets = {
            "whisper_combo", "embedding_combo", "thumbnail_size",
            "gpu_switch", "vision_switch", "vision_interval", "vision_max"
        };
        if (std::find(valid_widgets.begin(), valid_widgets.end(), widget) == valid_widgets.end()) {
            result["error"] = "Invalid widget: " + widget;
            result["valid_widgets"] = Json::arrayValue;
            for (const auto& w : valid_widgets) {
                result["valid_widgets"].append(w);
            }
            return result;
        }
        
        if (m_window && m_window->m_settings_page) {
            dispatch_to_main_thread([this, widget, value]() {
                m_window->m_settings_page->simulate_widget_interaction(widget, value);
            });
            
            result["success"] = true;
            result["widget"] = widget;
            result["value"] = value;
            result["message"] = "Settings change simulated";
        } else {
            result["error"] = "Settings page not available";
        }
        
        return result;
    });
    
    // Batch settings change - change multiple settings in one call
    // Useful for testing settings that might interact
    m_control_server->register_handler("batch_settings_change", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        
        // Parse all the settings to change
        std::vector<std::pair<std::string, std::string>> changes;
        
        if (args.isMember("whisper_model")) {
            changes.push_back({"whisper_combo", args["whisper_model"].asString()});
        }
        if (args.isMember("embedding_model")) {
            changes.push_back({"embedding_combo", args["embedding_model"].asString()});
        }
        if (args.isMember("thumbnail_size")) {
            changes.push_back({"thumbnail_size", std::to_string(args["thumbnail_size"].asInt())});
        }
        if (args.isMember("use_gpu")) {
            changes.push_back({"gpu_switch", args["use_gpu"].asBool() ? "true" : "false"});
        }
        if (args.isMember("vision_interval") || args.isMember("vision_scan_interval")) {
            int interval = args.isMember("vision_interval") ? args["vision_interval"].asInt() : args["vision_scan_interval"].asInt();
            changes.push_back({"vision_interval", std::to_string(interval)});
        }
        if (args.isMember("vision_max") || args.isMember("vision_max_per_video")) {
            int max_val = args.isMember("vision_max") ? args["vision_max"].asInt() : args["vision_max_per_video"].asInt();
            changes.push_back({"vision_max", std::to_string(max_val)});
        }
        
        if (changes.empty()) {
            result["error"] = "No settings to change";
            return result;
        }
        
        LUCID_INFO("[ControlServer] batch_settings_change: %d changes", (int)changes.size());
        
        if (m_window && m_window->m_settings_page) {
            // First go to settings page
            dispatch_to_main_thread([this]() {
                m_window->show_settings();
            });
            
            // Then apply all changes with small delays
            int delay_ms = args.get("delay_ms", 100).asInt();
            auto window_ref = m_window;
            
            for (size_t i = 0; i < changes.size(); i++) {
                auto& [widget, value] = changes[i];
                
                // Schedule each change with increasing delay
                int this_delay = (i + 1) * delay_ms;
                g_timeout_add(this_delay, [](gpointer data) -> gboolean {
                    auto* params = static_cast<std::tuple<std::string, std::string, Glib::RefPtr<MainWindow>>*>(data);
                    auto& widget_name = std::get<0>(*params);
                    auto& widget_value = std::get<1>(*params);
                    auto& window = std::get<2>(*params);
                    
                    if (window && window->m_settings_page) {
                        window->m_settings_page->simulate_widget_interaction(widget_name, widget_value);
                        LUCID_INFO("[ControlServer] Applied change: %s = %s", widget_name.c_str(), widget_value.c_str());
                    }
                    delete params;
                    return G_SOURCE_REMOVE;
                }, new std::tuple<std::string, std::string, Glib::RefPtr<MainWindow>>(widget, value, window_ref));
            }
            
            result["success"] = true;
            result["changes_count"] = (int)changes.size();
            result["message"] = "Batch settings change scheduled";
        } else {
            result["error"] = "Settings page not available";
        }
        
        return result;
    });
    
    // Settings stress test - rapid navigation and setting changes
    // This helps expose race conditions and crashes
    m_control_server->register_handler("settings_stress_test", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        
        int iterations = args.get("iterations", 5).asInt();
        int delay_ms = args.get("delay_ms", 300).asInt();
        std::string test_type = args.get("test_type", "full").asString();
        
        if (iterations < 1 || iterations > 100) {
            result["error"] = "Invalid iterations (1-100)";
            return result;
        }
        
        LUCID_INFO("[ControlServer] Starting settings stress test: iterations=%d, delay=%dms, type=%s",
                   iterations, delay_ms, test_type.c_str());
        
        if (!m_window) {
            result["error"] = "Window not available";
            return result;
        }
        
        // Store test parameters for the test loop
        struct StressTestState {
            int iterations;
            int delay_ms;
            int current_iteration;
            std::string test_type;
            Glib::RefPtr<MainWindow> window;
            // Pages and tabs to cycle through
            const char* pages[5];
            const char* tabs[5];
        };
        
        auto state = std::make_shared<StressTestState>();
        state->iterations = iterations;
        state->delay_ms = delay_ms;
        state->current_iteration = 0;
        state->test_type = test_type;
        state->window = m_window;
        state->pages[0] = "settings"; state->pages[1] = "home"; 
        state->pages[2] = "settings"; state->pages[3] = "model_hub"; 
        state->pages[4] = "settings";
        state->tabs[0] = "general"; state->tabs[1] = "models"; 
        state->tabs[2] = "advanced"; state->tabs[3] = "folders"; 
        state->tabs[4] = "general";
        
        // Schedule iterations using g_timeout_add
        // Each iteration schedules the next one
        auto run_iteration = [state]() -> bool {
            if (state->current_iteration >= state->iterations) {
                LUCID_INFO("[ControlServer] Stress test completed: %d iterations", state->iterations);
                return false;  // Stop scheduling
            }
            
            state->current_iteration++;
            int iter = state->current_iteration - 1;
            LUCID_INFO("[ControlServer] Stress test iteration %d/%d", state->current_iteration, state->iterations);
            
            // Cycle through pages
            int page_idx = iter % 5;
            std::string target_page = state->pages[page_idx];
            
            // Execute actions based on test type
            if (state->test_type == "full" || state->test_type == "navigation") {
                if (target_page == "settings") {
                    state->window->show_settings();
                    if (state->window->m_settings_page) {
                        state->window->m_settings_page->set_active_tab(state->tabs[page_idx]);
                    }
                } else if (target_page == "model_hub") {
                    state->window->show_model_hub();
                } else if (target_page == "home") {
                    state->window->show_home();
                } else if (target_page == "chat") {
                    state->window->show_chat();
                }
            }
            
            if (state->test_type == "full" || state->test_type == "settings_changes") {
                if (state->window->m_settings_page) {
                    // Change a setting on each iteration
                    const char* widgets[] = {"thumbnail_size", "vision_interval"};
                    const char* values[] = {((iter % 3) == 0) ? "0" : ((iter % 3) == 1) ? "1" : "2", 
                                            std::to_string(5 + (iter % 10)).c_str()};
                    int w_idx = iter % 2;
                    state->window->m_settings_page->simulate_widget_interaction(widgets[w_idx], values[w_idx]);
                }
            }
            
            return true;  // Continue scheduling
        };
        
        // Run the first iteration immediately, then schedule subsequent ones
        run_iteration();
        
        // Schedule subsequent iterations using g_timeout_add
        if (state->current_iteration < state->iterations) {
            g_timeout_add(state->delay_ms, [](gpointer data) -> gboolean {
                auto* s = static_cast<StressTestState*>(data);
                if (s->current_iteration < s->iterations) {
                    bool continue_test = [&]() -> bool {
                        if (s->test_type == "full" || s->test_type == "navigation") {
                            int page_idx = (s->current_iteration) % 5;
                            std::string target_page = s->pages[page_idx];
                            
                            if (target_page == "settings") {
                                s->window->show_settings();
                                if (s->window->m_settings_page) {
                                    s->window->m_settings_page->set_active_tab(s->tabs[page_idx]);
                                }
                            } else if (target_page == "model_hub") {
                                s->window->show_model_hub();
                            } else if (target_page == "home") {
                                s->window->show_home();
                            } else if (target_page == "chat") {
                                s->window->show_chat();
                            }
                        }
                        
                        if (s->test_type == "full" || s->test_type == "settings_changes") {
                            if (s->window->m_settings_page) {
                                const char* widgets[] = {"thumbnail_size", "vision_interval"};
                                const char* values[] = {(s->current_iteration % 3 == 0) ? "0" : 
                                    (s->current_iteration % 3 == 1) ? "1" : "2", 
                                    std::to_string(5 + (s->current_iteration % 10)).c_str()};
                                int w_idx = s->current_iteration % 2;
                                s->window->m_settings_page->simulate_widget_interaction(widgets[w_idx], values[w_idx]);
                            }
                        }
                        
                        s->current_iteration++;
                        LUCID_INFO("[ControlServer] Stress test iteration %d/%d", s->current_iteration, s->iterations);
                        return s->current_iteration < s->iterations;
                    }();
                    return continue_test ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE;
                }
                LUCID_INFO("[ControlServer] Stress test completed: %d iterations", s->iterations);
                return G_SOURCE_REMOVE;
            }, state.get());
        } else {
            LUCID_INFO("[ControlServer] Stress test completed in first iteration");
        }
        
        result["success"] = true;
        result["iterations"] = iterations;
        result["delay_ms"] = delay_ms;
        result["test_type"] = test_type;
        result["message"] = "Stress test started - results will be logged";
        
        return result;
    });
    
    // Simulate page transition with timing - helps test timing-dependent crashes
    m_control_server->register_handler("simulate_page_transition", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        
        std::string to_page = args.get("to", "settings").asString();
        std::string from_page = args.get("from", "").asString();
        int delay_ms = args.get("delay_ms", 200).asInt();
        bool return_to_from = args.get("return_to_from", false).asBool();
        
        LUCID_INFO("[ControlServer] simulate_page_transition: %s -> %s (delay=%dms, return=%d)",
                   from_page.c_str(), to_page.c_str(), delay_ms, return_to_from);
        
        if (!m_window) {
            result["error"] = "Window not available";
            return result;
        }
        
        // Navigate to target page
        dispatch_to_main_thread([this, to_page]() {
            if (to_page == "settings") {
                m_window->show_settings();
            } else if (to_page == "model_hub") {
                m_window->show_model_hub();
            } else if (to_page == "home") {
                m_window->show_home();
            } else if (to_page == "analysis_queue") {
                m_window->show_analysis_queue();
            } else if (to_page == "chat") {
                m_window->show_chat();
            } else if (to_page == "search") {
                m_window->show_search();
            }
        });
        
        // Optionally return to original page
        if (return_to_from && !from_page.empty()) {
            g_timeout_add(delay_ms, [](gpointer data) -> gboolean {
                auto* pages = static_cast<std::pair<std::string, Glib::RefPtr<MainWindow>>*>(data);
                const std::string& page = pages->first;
                Glib::RefPtr<MainWindow>& win = pages->second;
                
                if (page == "settings") {
                    win->show_settings();
                } else if (page == "model_hub") {
                    win->show_model_hub();
                } else if (page == "home") {
                    win->show_home();
                } else if (page == "analysis_queue") {
                    win->show_analysis_queue();
                } else if (page == "chat") {
                    win->show_chat();
                }
                
                delete pages;
                return G_SOURCE_REMOVE;
            }, new std::pair<std::string, Glib::RefPtr<MainWindow>>(from_page, m_window));
        }
        
        result["success"] = true;
        result["to"] = to_page;
        if (!from_page.empty()) {
            result["from"] = from_page;
        }
        result["delay_ms"] = delay_ms;
        result["message"] = "Page transition simulated";
        
        return result;
    });
    
    // Simulate user flow - complete user journey through the app
    m_control_server->register_handler("simulate_user_flow", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        
        std::string flow_name = args.get("flow", "settings_test").asString();
        int step_delay_ms = args.get("step_delay_ms", 500).asInt();
        
        LUCID_INFO("[ControlServer] simulate_user_flow: %s (delay=%dms)", flow_name.c_str(), step_delay_ms);
        
        if (!m_window) {
            result["error"] = "Window not available";
            return result;
        }
        
        // Define the user flows
        struct FlowStep {
            std::string action;  // "click", "change_setting", "wait"
            std::string target;   // button name, widget name, or delay in ms
            std::string value;    // for change_setting
        };
        
        std::vector<FlowStep> steps;
        
        if (flow_name == "settings_test") {
            // Test settings navigation
            steps = {
                {"click", "home", ""},
                {"wait", std::to_string(step_delay_ms), ""},
                {"click", "settings", ""},
                {"wait", std::to_string(step_delay_ms), ""},
                {"change_setting", "thumbnail_size", "1"},
                {"wait", std::to_string(step_delay_ms), ""},
                {"click", "model_hub", ""},
                {"wait", std::to_string(step_delay_ms), ""},
                {"click", "settings", ""},
                {"wait", std::to_string(step_delay_ms), ""},
                {"change_setting", "thumbnail_size", "2"},
                {"wait", std::to_string(step_delay_ms), ""},
                {"click", "home", ""},
            };
        } else if (flow_name == "rapid_settings_change") {
            // Rapidly change settings while on the page
            steps = {
                {"click", "settings", ""},
                {"wait", std::to_string(step_delay_ms), ""},
                {"change_setting", "thumbnail_size", "0"},
                {"change_setting", "thumbnail_size", "1"},
                {"change_setting", "thumbnail_size", "2"},
                {"change_setting", "thumbnail_size", "0"},
                {"wait", std::to_string(step_delay_ms), ""},
                {"click", "home", ""},
            };
        } else if (flow_name == "tab_navigation") {
            // Cycle through all settings tabs
            steps = {
                {"click", "settings", ""},
                {"wait", std::to_string(step_delay_ms), ""},
                {"click", "home", ""},
                {"wait", std::to_string(step_delay_ms/2), ""},
                {"click", "settings", ""},
                {"wait", std::to_string(step_delay_ms), ""},
                {"click", "home", ""},
                {"wait", std::to_string(step_delay_ms/2), ""},
                {"click", "model_hub", ""},
                {"wait", std::to_string(step_delay_ms), ""},
                {"click", "settings", ""},
                {"wait", std::to_string(step_delay_ms), ""},
                {"click", "home", ""},
            };
        } else {
            result["error"] = "Unknown flow: " + flow_name;
            result["valid_flows"] = Json::arrayValue;
            result["valid_flows"].append("settings_test");
            result["valid_flows"].append("rapid_settings_change");
            result["valid_flows"].append("tab_navigation");
            return result;
        }
        
        // Execute the flow step by step
        int step_index = 0;
        for (const auto& step : steps) {
            int delay = (step.action == "wait") ? std::stoi(step.target) : (step_index + 1) * step_delay_ms;
            
            g_timeout_add(delay, [](gpointer data) -> gboolean {
                auto* params = static_cast<std::tuple<std::string, std::string, std::string, Glib::RefPtr<MainWindow>>*>(data);
                const std::string& action = std::get<0>(*params);
                const std::string& target = std::get<1>(*params);
                const std::string& value = std::get<2>(*params);
                Glib::RefPtr<MainWindow>& win = std::get<3>(*params);
                delete params;
                
                if (action == "click") {
                    if (target == "home") {
                        win->show_home();
                    } else if (target == "settings") {
                        win->show_settings();
                    } else if (target == "model_hub") {
                        win->show_model_hub();
                    } else if (target == "chat") {
                        win->show_chat();
                    }
                    LUCID_INFO("[ControlServer] Flow step: click %s", target.c_str());
                } else if (action == "change_setting") {
                    if (win->m_settings_page) {
                        win->m_settings_page->simulate_widget_interaction(target, value);
                        LUCID_INFO("[ControlServer] Flow step: change_setting %s = %s", target.c_str(), value.c_str());
                    }
                }
                
                return G_SOURCE_REMOVE;
            }, new std::tuple<std::string, std::string, std::string, Glib::RefPtr<MainWindow>>(
                step.action, step.target, step.value, m_window));
            
            step_index++;
        }
        
        result["success"] = true;
        result["flow"] = flow_name;
        result["steps"] = (int)steps.size();
        result["step_delay_ms"] = step_delay_ms;
        result["message"] = "User flow started";
        
        return result;
    });
    
    // ========== ANALYSIS QUEUE COMMANDS ==========
    
    // Get analysis queue status
    m_control_server->register_handler("get_analysis_queue", [this](const Json::Value& /*args*/) -> Json::Value {
        Json::Value result;
        result["success"] = true;
        
        if (m_analysis_worker) {
            // Get video statistics from database
            if (m_database) {
                auto videos = m_database->get_all_videos();
                int pending = 0, processing = 0, completed = 0, failed = 0;
                
                for (const auto& video : videos) {
                    switch (video.status) {
                        case VideoStatus::PENDING:
                            pending++;
                            break;
                        case VideoStatus::ANALYZING:
                        case VideoStatus::TRANSCRIBING:
                        case VideoStatus::THUMBNAILING:
                        case VideoStatus::VISION_SCANNING:
                            processing++;
                            break;
                        case VideoStatus::COMPLETED:
                            completed++;
                            break;
                        case VideoStatus::FAILED:
                            failed++;
                            break;
                        case VideoStatus::STALE:
                            // STALE videos are also pending for reprocessing
                            pending++;
                            break;
                        case VideoStatus::BROKEN:
                            // BROKEN videos need manual intervention - don't add to any count
                            break;
                    }
                }
                
                result["total_videos"] = (int)videos.size();
                result["pending"] = pending;
                result["processing"] = processing;
                result["completed"] = completed;
                result["failed"] = failed;
            }
        } else {
            result["error"] = "Analysis worker not initialized";
        }
        
        return result;
    });
    
    // Get list of videos with their status
    m_control_server->register_handler("get_videos", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        result["success"] = true;
        
        int limit = args.get("limit", 50).asInt();
        int offset = args.get("offset", 0).asInt();
        std::string filter = args.get("status", "").asString();
        
        if (m_database) {
            auto videos = m_database->get_all_videos();
            
            int count = 0;
            for (const auto& video : videos) {
                if (offset > 0 && count < offset) {
                    count++;
                    continue;
                }
                if ((int)result["videos"].size() >= limit) break;
                
                // Filter by status if specified
                if (!filter.empty()) {
                    bool match = false;
                    switch (video.status) {
                        case VideoStatus::PENDING:
                            match = (filter == "pending");
                            break;
                        case VideoStatus::ANALYZING:
                        case VideoStatus::TRANSCRIBING:
                        case VideoStatus::THUMBNAILING:
                        case VideoStatus::VISION_SCANNING:
                            match = (filter == "processing");
                            break;
                        case VideoStatus::COMPLETED:
                            match = (filter == "completed");
                            break;
                        case VideoStatus::FAILED:
                            match = (filter == "failed");
                            break;
                        case VideoStatus::STALE:
                            match = (filter == "pending");
                            break;
                        case VideoStatus::BROKEN:
                            match = (filter == "broken");
                            break;
                    }
                    if (!match) continue;
                }
                
                Json::Value v;
                v["id"] = video.id;
                v["path"] = video.path;
                v["title"] = video.title;
                v["duration_ms"] = video.duration_ms;
                v["status"] = video.status == VideoStatus::PENDING ? "pending" :
                              video.status == VideoStatus::COMPLETED ? "completed" :
                              video.status == VideoStatus::FAILED ? "failed" : "processing";
                v["thumbnail"] = video.thumbnail ? *video.thumbnail : "";
                
                // Get additional stats for each video
                if (m_database) {
                    auto thumbs = m_database->get_thumbnails(video.id);
                    v["thumbnail_count"] = (int)thumbs.size();
                    v["marker_count"] = (int)m_database->get_markers(video.id).size();
                    v["vision_count"] = (int)m_database->get_vision_analyses(video.id).size();
                    // Set thumbnail to first thumbnail path if video.thumbnail is empty
                    if (v["thumbnail"].asString().empty() && !thumbs.empty()) {
                        v["thumbnail"] = thumbs[0].path;
                    }
                } else {
                    v["thumbnail_count"] = 0;
                    v["marker_count"] = 0;
                    v["vision_count"] = 0;
                }
                
                result["videos"].append(v);
            }
            result["total"] = (int)videos.size();
        } else {
            result["error"] = "Database not initialized";
        }
        
        return result;
    });
    
    // Trigger analysis for a specific video
    m_control_server->register_handler("trigger_analysis", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        
        std::string video_id = args.get("video_id", "").asString();
        if (video_id.empty()) {
            result["error"] = "Missing 'video_id' parameter";
            return result;
        }
        
        if (m_database) {
            m_database->update_video_status(video_id, VideoStatus::PENDING);
            result["success"] = true;
            result["video_id"] = video_id;
            result["message"] = "Video queued for analysis";
        } else {
            result["error"] = "Database not initialized";
        }
        
        return result;
    });
    
    // Semantic search - search videos by query text
    m_control_server->register_handler("semantic_search", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        result["success"] = true;
        
        std::string query = args.get("query", "").asString();
        int max_results = args.get("max_results", 10).asInt();
        
        if (query.empty()) {
            result["error"] = "Missing 'query' parameter";
            return result;
        }
        
        if (!m_semantic_search) {
            result["error"] = "Semantic search not initialized";
            return result;
        }
        
        auto search_results = m_semantic_search->search_text(query, max_results);
        
        result["query"] = query;
        result["result_count"] = (int)search_results.size();
        
        for (const auto& r : search_results) {
            Json::Value item;
            item["video_id"] = r.video_id;
            item["title"] = r.title;
            item["timestamp_ms"] = r.timestamp_ms;
            item["relevance_score"] = r.relevance_score;
            item["snippet"] = r.snippet;
            item["match_type"] = r.match_type == MatchType::TITLE ? "title" : 
                                 r.match_type == MatchType::TRANSCRIPT ? "transcript" : "unknown";
            result["results"].append(item);
        }
        
        return result;
    });
    
    // Hybrid search - keyword + semantic
    m_control_server->register_handler("hybrid_search", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        result["success"] = true;
        
        std::string query = args.get("query", "").asString();
        int max_results = args.get("max_results", 10).asInt();
        float semantic_weight = args.get("semantic_weight", 0.7f).asFloat();
        float keyword_weight = args.get("keyword_weight", 0.3f).asFloat();
        
        if (query.empty()) {
            result["error"] = "Missing 'query' parameter";
            return result;
        }
        
        if (!m_semantic_search) {
            result["error"] = "Semantic search not initialized";
            return result;
        }
        
        auto search_results = m_semantic_search->hybrid_search(query, max_results, semantic_weight, keyword_weight);
        
        result["query"] = query;
        result["result_count"] = (int)search_results.size();
        
        for (const auto& r : search_results) {
            Json::Value item;
            item["video_id"] = r.video_id;
            item["title"] = r.title;
            item["timestamp_ms"] = r.timestamp_ms;
            item["relevance_score"] = r.relevance_score;
            item["snippet"] = r.snippet;
            item["match_type"] = r.match_type == MatchType::TITLE ? "title" : 
                                 r.match_type == MatchType::TRANSCRIPT ? "transcript" : "unknown";
            result["results"].append(item);
        }
        
        return result;
    });
    
    // Search within a specific video
    m_control_server->register_handler("search_in_video", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        result["success"] = true;
        
        std::string video_id = args.get("video_id", "").asString();
        std::string query = args.get("query", "").asString();
        int max_results = args.get("max_results", 10).asInt();
        
        if (video_id.empty() || query.empty()) {
            result["error"] = "Missing 'video_id' or 'query' parameter";
            return result;
        }
        
        if (!m_semantic_search) {
            result["error"] = "Semantic search not initialized";
            return result;
        }
        
        auto search_results = m_semantic_search->search_in_video(video_id, query, max_results);
        
        result["video_id"] = video_id;
        result["query"] = query;
        result["result_count"] = (int)search_results.size();
        
        for (const auto& r : search_results) {
            Json::Value item;
            item["video_id"] = r.video_id;
            item["title"] = r.title;
            item["timestamp_ms"] = r.timestamp_ms;
            item["relevance_score"] = r.relevance_score;
            item["snippet"] = r.snippet;
            result["results"].append(item);
        }
        
        return result;
    });
    
    // Get search index stats
    m_control_server->register_handler("get_search_stats", [this](const Json::Value& /*args*/) -> Json::Value {
        Json::Value result;
        result["success"] = true;
        
        if (!m_semantic_search) {
            result["error"] = "Semantic search not initialized";
            return result;
        }
        
        result["initialized"] = m_semantic_search->is_initialized();
        result["embedding_dim"] = m_semantic_search->get_embedding_dim();
        result["context_size"] = m_semantic_search->get_context_size();
        result["model_path"] = m_semantic_search->get_model_path();
        
        // Count indexed items from database
        if (m_database) {
            auto videos = m_database->get_all_videos();
            int indexed_videos = 0;
            int total_embeddings = 0;
            
            for (const auto& v : videos) {
                auto markers = m_database->get_markers(v.id);
                if (!markers.empty() || v.thumbnail.has_value()) {
                    indexed_videos++;
                }
                total_embeddings += (int)markers.size();
            }
            
            result["total_videos"] = (int)videos.size();
            result["indexed_videos"] = indexed_videos;
            result["total_embeddings"] = total_embeddings;
        }
        
        return result;
    });
    
    // Reset all videos to pending
    m_control_server->register_handler("reset_videos", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        
        std::string filter = args.get("status", "").asString();
        
        if (m_database) {
            auto videos = m_database->get_all_videos();
            int reset_count = 0;
            
            for (const auto& video : videos) {
                bool should_reset = filter.empty();
                if (!should_reset) {
                    switch (video.status) {
                        case VideoStatus::PENDING:
                            should_reset = (filter == "pending");
                            break;
                        case VideoStatus::ANALYZING:
                        case VideoStatus::TRANSCRIBING:
                        case VideoStatus::THUMBNAILING:
                        case VideoStatus::VISION_SCANNING:
                            should_reset = (filter == "processing");
                            break;
                        case VideoStatus::COMPLETED:
                            should_reset = (filter == "completed");
                            break;
                        case VideoStatus::FAILED:
                            should_reset = (filter == "failed");
                            break;
                        case VideoStatus::STALE:
                            should_reset = (filter == "pending");
                            break;
                        case VideoStatus::BROKEN:
                            should_reset = (filter == "broken");
                            break;
                    }
                }
                
                if (should_reset) {
                    m_database->update_video_status(video.id, VideoStatus::PENDING);
                    reset_count++;
                }
            }
            
            result["success"] = true;
            result["reset_count"] = reset_count;
            result["message"] = "Videos reset to pending";
        } else {
            result["error"] = "Database not initialized";
        }
        
        return result;
    });
    
    // Get vision scan status and models
    m_control_server->register_handler("get_vision_status", [this](const Json::Value& /*args*/) -> Json::Value {
        Json::Value result;
        auto& mm = ModelManager::instance();
        
        result["success"] = true;
        result["vision_scan_enabled"] = true;  // Always enabled
        result["vision_scan_interval"] = mm.get_vision_scan_interval();
        result["vision_max_per_video"] = mm.get_vision_max_per_video();
        
        // Check if vision model is available (f16 model required for vision)
        bool has_vision_model = std::filesystem::exists(Glib::build_filename(Glib::get_user_data_dir(), "lucid", "models", "Qwen2-VL-2B-Instruct-f16.gguf")) ||
                                std::filesystem::exists(Glib::build_filename(Glib::get_user_data_dir(), "lucid", "models", "qwen2-vl-2b-q4_k_m.gguf"));
        bool has_mmproj = std::filesystem::exists(Glib::build_filename(Glib::get_user_data_dir(), "lucid", "models", "qwen2-vl-2b-mmproj.gguf"));
        
        result["vision_model_available"] = has_vision_model;
        result["mmproj_available"] = has_mmproj;
        result["models_dir"] = Glib::build_filename(Glib::get_user_data_dir(), "lucid", "models");
        
        if (!has_vision_model || !has_mmproj) {
            result["warning"] = "Vision models not found. Download from HuggingFace: Qwen2-VL-2B";
        }
        
        return result;
    });
    
    // Full test cycle for vision scan
    m_control_server->register_handler("test_vision_scan", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        result["success"] = true;
        
        // Vision scan is always enabled
        int interval = args.get("interval", 15).asInt();
        int max_per_video = args.get("max_per_video", 50).asInt();
        
        auto& mm = ModelManager::instance();
        mm.set_vision_scan_interval(interval);
        mm.set_vision_max_per_video(max_per_video);
        
        result["vision_scan_enabled"] = true;
        result["interval"] = interval;
        result["max_per_video"] = max_per_video;
        
        // Step 2: Verify settings
        result["step2_verified_enabled"] = true;  // Always enabled
        result["step2_verified_interval"] = mm.get_vision_scan_interval();
        result["step2_verified_max"] = mm.get_vision_max_per_video();
        
        // Step 3: Check models (f16 model required for vision)
        bool has_vision_model = std::filesystem::exists(Glib::build_filename(Glib::get_user_data_dir(), "lucid", "models", "Qwen2-VL-2B-Instruct-f16.gguf")) ||
                                std::filesystem::exists(Glib::build_filename(Glib::get_user_data_dir(), "lucid", "models", "qwen2-vl-2b-q4_k_m.gguf"));
        bool has_mmproj = std::filesystem::exists(Glib::build_filename(Glib::get_user_data_dir(), "lucid", "models", "qwen2-vl-2b-mmproj.gguf"));
        
        result["step3_vision_model_exists"] = has_vision_model;
        result["step3_mmproj_exists"] = has_mmproj;
        
        // Step 4: Reset videos to pending for analysis
        int reset_count = 0;
        if (m_database) {
            auto videos = m_database->get_all_videos();
            for (const auto& video : videos) {
                if (video.status == VideoStatus::COMPLETED || 
                    video.status == VideoStatus::FAILED) {
                    m_database->update_video_status(video.id, VideoStatus::PENDING);
                    reset_count++;
                }
            }
        }
        result["step4_videos_reset"] = reset_count;
        
        // Navigate to analysis queue to show status
        if (m_window) {
            auto* data = new std::pair<Glib::RefPtr<MainWindow>, Page>(m_window, Page::ANALYSIS_QUEUE);
            g_idle_add([](gpointer user_data) -> gboolean {
                auto* data = static_cast<std::pair<Glib::RefPtr<MainWindow>, Page>*>(user_data);
                data->first->show_page(data->second);
                delete data;
                return G_SOURCE_REMOVE;
            }, data);
        }
        result["step5_navigated_to"] = "analysis_queue";
        
        result["message"] = "Vision scan test configuration complete";
        
        return result;
    });
    
    // Get models catalog
    m_control_server->register_handler("get_models_catalog", [this](const Json::Value& /*args*/) -> Json::Value {
        Json::Value result;
        result["success"] = true;
        
        auto& mm = ModelManager::instance();
        const auto& catalog = mm.get_catalog();
        
        for (const auto& model : catalog) {
            Json::Value m;
            m["id"] = model.id;
            m["name"] = model.name;
            m["type"] = model.type == ModelType::WHISPER ? "whisper" :
                        model.type == ModelType::EMBEDDING ? "embedding" :
                        model.type == ModelType::LLM ? "llm" : "vision";
            m["size"] = (int)model.size_bytes;
            m["state"] = model.state == ModelState::DOWNLOADED ? "downloaded" :
                         model.state == ModelState::DOWNLOADING ? "downloading" : "not_downloaded";
            m["download_progress"] = model.download_progress;
            m["local_path"] = model.local_path;
            result["models"].append(m);
        }
        
        return result;
    });
    
    // ========== VIDEO MANAGEMENT COMMANDS ==========
    
    // Get detailed info about a specific video
    m_control_server->register_handler("get_video_info", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        
        std::string video_id = args.get("id", "").asString();
        if (video_id.empty()) {
            result["success"] = false;
            result["error"] = "Video ID is required";
            return result;
        }
        
        if (m_database) {
            auto video = m_database->get_video(video_id);
            if (video) {
                result["success"] = true;
                result["id"] = video->id;
                result["path"] = video->path;
                result["title"] = video->title;
                result["duration_ms"] = (int)video->duration_ms;
                result["status"] = video->status == VideoStatus::PENDING ? "pending" :
                                  video->status == VideoStatus::COMPLETED ? "completed" :
                                  video->status == VideoStatus::FAILED ? "failed" :
                                  video->status == VideoStatus::STALE ? "stale" : "processing";
                result["error_message"] = video->error_message;
                result["thumbnail"] = video->thumbnail ? *video->thumbnail : "";
                result["source"] = video->source;
                
                // Get markers count
                auto markers = m_database->get_markers(video_id);
                result["marker_count"] = (int)markers.size();
                
                // Get thumbnails count
                auto thumbs = m_database->get_thumbnails(video_id);
                result["thumbnail_count"] = (int)thumbs.size();
                
                // Get vision analyses count
                auto visions = m_database->get_vision_analyses(video_id);
                result["vision_analysis_count"] = (int)visions.size();
                
                // Format duration
                int64_t ms = video->duration_ms;
                int hours = ms / 3600000;
                int minutes = (ms % 3600000) / 60000;
                int seconds = (ms % 60000) / 1000;
                std::ostringstream oss;
                oss << std::setfill('0') << std::setw(2) << hours << ":"
                    << std::setw(2) << minutes << ":" << std::setw(2) << seconds;
                result["duration_formatted"] = oss.str();
            } else {
                result["success"] = false;
                result["error"] = "Video not found";
            }
        } else {
            result["success"] = false;
            result["error"] = "Database not initialized";
        }
        
        return result;
    });
    
    // Add a video to the library manually
    m_control_server->register_handler("add_video", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        
        std::string path = args.get("path", "").asString();
        if (path.empty()) {
            result["success"] = false;
            result["error"] = "Video path is required";
            return result;
        }
        
        if (m_database) {
            // Check if video already exists
            auto existing = m_database->find_by_path(path);
            if (existing) {
                result["success"] = false;
                result["error"] = "Video already exists in library";
                result["existing_id"] = *existing;
                return result;
            }
            
            // Create new video entry
            VideoEntry video;
            // Generate unique ID using GLib's random functions
            static const char* hex = "0123456789abcdef";
            std::string uuid;
            uuid.reserve(36);
            guint32 r1 = g_random_int();
            guint32 r2 = g_random_int();
            for (int i = 0; i < 36; ++i) {
                if (i == 8 || i == 13 || i == 18 || i == 23) {
                    uuid += '-';
                } else {
                    int idx = (i < 14) ? ((r1 >> ((13 - i) * 4)) & 0xF) : ((r2 >> ((29 - i) * 4)) & 0xF);
                    uuid += hex[idx];
                }
            }
            video.id = uuid;
            video.path = path;
            
            // Extract title from filename
            std::string filename = Glib::path_get_basename(path);
            video.title = Glib::path_get_basename(filename);
            video.status = VideoStatus::PENDING;
            video.source = "manual";
            
            try {
                std::string id = m_database->add_video(video);
                result["success"] = true;
                result["id"] = id;
                result["message"] = "Video added to library";
                
                // Trigger analysis if requested
                if (args.get("trigger_analysis", false).asBool()) {
                    m_analysis_worker->trigger_check();
                }
            } catch (const std::exception& e) {
                result["success"] = false;
                result["error"] = std::string("Failed to add video: ") + e.what();
            }
        } else {
            result["success"] = false;
            result["error"] = "Database not initialized";
        }
        
        return result;
    });
    
    // Remove a video from the library
    m_control_server->register_handler("remove_video", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        
        std::string video_id = args.get("id", "").asString();
        if (video_id.empty()) {
            result["success"] = false;
            result["error"] = "Video ID is required";
            return result;
        }
        
        if (m_database) {
            // Verify video exists
            auto video = m_database->get_video(video_id);
            if (!video) {
                result["success"] = false;
                result["error"] = "Video not found";
                return result;
            }
            
            try {
                // Clear related data first
                m_database->clear_markers(video_id);
                m_database->clear_thumbnails(video_id);
                m_database->clear_vision_analyses(video_id);
                m_database->delete_embeddings_for_video(video_id);
                
                // Delete the video entry
                m_database->delete_video(video_id);
                
                result["success"] = true;
                result["message"] = "Video removed from library";
                result["removed_id"] = video_id;
            } catch (const std::exception& e) {
                result["success"] = false;
                result["error"] = std::string("Failed to remove video: ") + e.what();
            }
        } else {
            result["success"] = false;
            result["error"] = "Database not initialized";
        }
        
        return result;
    });
    
    // ========== ANALYSIS CONTROL COMMANDS ==========
    
    // Cancel current analysis job
    m_control_server->register_handler("cancel_analysis", [this](const Json::Value& /*args*/) -> Json::Value {
        Json::Value result;
        
        if (m_analysis_worker) {
            if (m_analysis_worker->is_processing()) {
                m_analysis_worker->request_cancel();
                result["success"] = true;
                result["message"] = "Cancel requested for current job";
            } else {
                result["success"] = false;
                result["error"] = "No video currently being processed";
            }
        } else {
            result["success"] = false;
            result["error"] = "Analysis worker not initialized";
        }
        
        return result;
    });
    
    // Get current analysis progress
    m_control_server->register_handler("get_analysis_progress", [this](const Json::Value& /*args*/) -> Json::Value {
        Json::Value result;
        result["success"] = true;
        
        if (m_analysis_worker) {
            auto stats = m_analysis_worker->get_stats();
            
            result["is_running"] = m_analysis_worker->is_running();
            result["is_processing"] = m_analysis_worker->is_processing();
            result["pending"] = stats.pending_count;
            result["processing"] = stats.processing_count;
            result["completed"] = stats.completed_count;
            result["failed"] = stats.failed_count;
            
            if (!stats.current_video_id.empty()) {
                result["current_video_id"] = stats.current_video_id;
                result["current_phase"] = stats.current_phase;
                result["current_progress"] = stats.current_progress;
                
                // Get video title for current video
                if (m_database) {
                    auto video = m_database->get_video(stats.current_video_id);
                    if (video) {
                        result["current_video_title"] = video->title;
                    }
                }
            }
        } else {
            result["error"] = "Analysis worker not initialized";
        }
        
        return result;
    });
    
    // Pause the analysis worker
    m_control_server->register_handler("pause_analysis", [this](const Json::Value& /*args*/) -> Json::Value {
        Json::Value result;
        
        if (m_analysis_worker) {
            if (m_analysis_worker->is_paused()) {
                result["success"] = false;
                result["error"] = "Analysis worker is already paused";
            } else {
                m_analysis_worker->pause();
                result["success"] = true;
                result["message"] = "Analysis worker paused";
            }
        } else {
            result["success"] = false;
            result["error"] = "Analysis worker not initialized";
        }
        
        return result;
    });
    
    // Resume the analysis worker
    m_control_server->register_handler("resume_analysis", [this](const Json::Value& /*args*/) -> Json::Value {
        Json::Value result;
        
        if (m_analysis_worker) {
            if (!m_analysis_worker->is_paused()) {
                result["success"] = false;
                result["error"] = "Analysis worker is not paused";
            } else {
                m_analysis_worker->resume();
                result["success"] = true;
                result["message"] = "Analysis worker resumed";
            }
        } else {
            result["success"] = false;
            result["error"] = "Analysis worker not initialized";
        }
        
        return result;
    });
    
    // ========== ANALYSIS QUEUE COMMANDS ==========
    
    // Get detailed queue statistics
    m_control_server->register_handler("get_queue_stats", [this](const Json::Value& /*args*/) -> Json::Value {
        Json::Value result;
        result["success"] = true;
        
        if (m_database) {
            result["total_videos"] = m_database->get_video_count();
            result["pending"] = m_database->get_video_count_by_status(VideoStatus::PENDING);
            
            // Count processing, but check for stale entries
            int processing_count = m_database->get_video_count_by_status(VideoStatus::ANALYZING) +
                                  m_database->get_video_count_by_status(VideoStatus::TRANSCRIBING) +
                                  m_database->get_video_count_by_status(VideoStatus::THUMBNAILING) +
                                  m_database->get_video_count_by_status(VideoStatus::VISION_SCANNING);
            
            // If worker isn't processing, mark stale entries
            if (m_analysis_worker && !m_analysis_worker->is_processing() && processing_count > 0) {
                LUCID_WARN("[ControlServer] Found %d stale processing entries, will auto-clean on next analysis", processing_count);
                result["stale_entries"] = processing_count;
                processing_count = 0;
            }
            
            result["processing"] = processing_count;
            result["completed"] = m_database->get_video_count_by_status(VideoStatus::COMPLETED);
            result["failed"] = m_database->get_video_count_by_status(VideoStatus::FAILED);
            
            // Get marker count
            auto videos = m_database->get_all_videos();
            int total_markers = 0;
            for (const auto& vid : videos) {
                auto markers = m_database->get_markers(vid.id);
                total_markers += (int)markers.size();
            }
            result["total_markers"] = total_markers;
        } else {
            result["error"] = "Database not initialized";
        }
        
        return result;
    });
    
    // Re-analyze all videos (reset completed to pending)
    m_control_server->register_handler("reanalyze_all", [this](const Json::Value& /*args*/) -> Json::Value {
        Json::Value result;
        
        if (m_analysis_worker && m_analysis_worker->is_processing()) {
            result["success"] = false;
            result["error"] = "Cannot re-analyze while analysis is in progress";
            return result;
        }
        
        if (m_database) {
            int reset_count = 0;
            auto videos = m_database->get_all_videos();
            for (const auto& vid : videos) {
                if (vid.status == VideoStatus::COMPLETED) {
                    m_database->update_video_status(vid.id, VideoStatus::PENDING);
                    // Clear markers and thumbnails for re-analysis
                    m_database->clear_markers(vid.id);
                    m_database->clear_thumbnails(vid.id);
                    m_database->delete_embeddings_for_video(vid.id);
                    reset_count++;
                }
            }
            
            result["success"] = true;
            result["reset_count"] = reset_count;
            result["message"] = std::to_string(reset_count) + " videos reset to pending for re-analysis";
            
            // Trigger worker to pick up new jobs
            if (m_analysis_worker) {
                m_analysis_worker->trigger_check();
            }
        } else {
            result["success"] = false;
            result["error"] = "Database not initialized";
        }
        
        return result;
    });
    
    // Clear all analysis data (markers, thumbnails, embeddings) and reset all videos
    m_control_server->register_handler("clear_all_analysis_data", [this](const Json::Value& /*args*/) -> Json::Value {
        Json::Value result;
        
        if (m_analysis_worker && m_analysis_worker->is_processing()) {
            result["success"] = false;
            result["error"] = "Cannot clear data while analysis is in progress";
            return result;
        }
        
        if (m_database) {
            auto videos = m_database->get_all_videos();
            int cleared_count = 0;
            
            for (const auto& vid : videos) {
                // Clear all analysis data
                m_database->clear_markers(vid.id);
                m_database->clear_thumbnails(vid.id);
                m_database->clear_vision_analyses(vid.id);
                m_database->delete_embeddings_for_video(vid.id);
                
                // Reset video to pending
                m_database->update_video_status(vid.id, VideoStatus::PENDING);
                cleared_count++;
            }
            
            result["success"] = true;
            result["cleared_count"] = cleared_count;
            result["message"] = std::to_string(cleared_count) + " videos: all analysis data cleared";
        } else {
            result["success"] = false;
            result["error"] = "Database not initialized";
        }
        
        return result;
    });
    
    // Remove all failed videos from the library
    m_control_server->register_handler("remove_failed_videos", [this](const Json::Value& /*args*/) -> Json::Value {
        Json::Value result;
        
        if (m_database) {
            // Get all failed videos and delete them
            auto videos = m_database->get_all_videos();
            int removed = 0;
            for (const auto& vid : videos) {
                if (vid.status == VideoStatus::FAILED) {
                    // Clear related data first
                    m_database->clear_markers(vid.id);
                    m_database->clear_thumbnails(vid.id);
                    m_database->clear_vision_analyses(vid.id);
                    m_database->delete_embeddings_for_video(vid.id);
                    // Delete video
                    m_database->delete_video(vid.id);
                    removed++;
                }
            }
            
            result["success"] = true;
            result["removed_count"] = removed;
            result["message"] = std::to_string(removed) + " failed videos removed";
        } else {
            result["success"] = false;
            result["error"] = "Database not initialized";
        }
        
        return result;
    });
    
    // Navigate to analysis queue page
    m_control_server->register_handler("navigate_to_analysis_queue", [this](const Json::Value& /*args*/) -> Json::Value {
        Json::Value result;
        result["success"] = true;
        result["message"] = "Navigate to analysis queue page";
        result["page"] = "analysis_queue";
        
        // The actual navigation is handled by the main window
        // This command signals intent - the UI callback would handle it
        // For now, we just acknowledge the request
        LUCID_INFO("[ControlServer] navigate_to_analysis_queue called");
        
        return result;
    });
    
    // Clean up stale processing entries (videos stuck in processing state)
    m_control_server->register_handler("cleanup_stale_entries", [this](const Json::Value& /*args*/) -> Json::Value {
        Json::Value result;
        result["success"] = true;
        
        if (m_database) {
            int cleaned = 0;
            
            // Find and reset stale processing entries
            auto videos = m_database->get_all_videos();
            for (const auto& vid : videos) {
                if (vid.status == VideoStatus::ANALYZING ||
                    vid.status == VideoStatus::TRANSCRIBING ||
                    vid.status == VideoStatus::THUMBNAILING ||
                    vid.status == VideoStatus::VISION_SCANNING) {
                    
                    LUCID_WARN("[ControlServer] Cleaning stale entry: %s (status=%d)", 
                               vid.title.c_str(), (int)vid.status);
                    m_database->update_video_status(vid.id, VideoStatus::PENDING);
                    cleaned++;
                }
            }
            
            result["cleaned_count"] = cleaned;
            result["message"] = std::to_string(cleaned) + " stale entries cleaned";
        } else {
            result["success"] = false;
            result["error"] = "Database not initialized";
        }
        
        return result;
    });
    
    // Start downloading a model
    m_control_server->register_handler("start_download", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        
        if (!args.isMember("model_id")) {
            result["success"] = false;
            result["error"] = "model_id is required";
            return result;
        }
        
        std::string model_id = args["model_id"].asString();
        LUCID_INFO("[ControlServer] start_download called for: %s", model_id.c_str());
        
        bool started = ModelManager::instance().start_download(model_id);
        result["success"] = started;
        result["model_id"] = model_id;
        
        if (started) {
            result["message"] = "Download started for: " + model_id;
        } else {
            result["error"] = "Failed to start download (already downloading or model not in catalog)";
        }
        
        return result;
    });
    
    // Cancel ongoing download
    m_control_server->register_handler("cancel_download", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        
        if (!args.isMember("model_id")) {
            result["success"] = false;
            result["error"] = "model_id is required";
            return result;
        }
        
        std::string model_id = args["model_id"].asString();
        LUCID_INFO("[ControlServer] cancel_download called for: %s", model_id.c_str());
        
        ModelManager::instance().cancel_download(model_id);
        
        result["success"] = true;
        result["model_id"] = model_id;
        result["message"] = "Download cancelled: " + model_id;
        
        return result;
    });
    
    // Get download status for a specific model
    m_control_server->register_handler("get_download_status", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        
        if (!args.isMember("model_id")) {
            result["success"] = false;
            result["error"] = "model_id is required";
            return result;
        }
        
        std::string model_id = args["model_id"].asString();
        const ModelDescriptor* model = ModelManager::instance().get_model(model_id);
        
        if (model) {
            result["success"] = true;
            result["model_id"] = model_id;
            result["state"] = model->state == ModelState::DOWNLOADED ? "downloaded" :
                              model->state == ModelState::DOWNLOADING ? "downloading" :
                              model->state == ModelState::ERROR ? "error" : "not_downloaded";
            result["download_progress"] = model->download_progress;
            result["download_speed"] = model->download_speed;
            result["downloaded_bytes"] = (Json::UInt64)model->downloaded_bytes;
            result["size_bytes"] = (Json::UInt64)model->size_bytes;
            
            if (!model->error_message.empty()) {
                result["error"] = model->error_message;
            }
        } else {
            result["success"] = false;
            result["error"] = "Model not found in catalog: " + model_id;
        }
        
        return result;
    });
    
    // Delete a downloaded model
    m_control_server->register_handler("delete_model", [this](const Json::Value& args) -> Json::Value {
        Json::Value result;
        
        if (!args.isMember("model_id")) {
            result["success"] = false;
            result["error"] = "model_id is required";
            return result;
        }
        
        std::string model_id = args["model_id"].asString();
        LUCID_INFO("[ControlServer] delete_model called for: %s", model_id.c_str());
        
        bool deleted = ModelManager::instance().delete_model(model_id);
        result["success"] = deleted;
        result["model_id"] = model_id;
        
        if (deleted) {
            result["message"] = "Model deleted: " + model_id;
        } else {
            result["error"] = "Failed to delete model (not downloaded or file in use)";
        }
        
        return result;
    });
    
    // Get all downloaded models
    m_control_server->register_handler("get_downloaded_models", [this](const Json::Value& /*args*/) -> Json::Value {
        Json::Value result;
        
        auto models = ModelManager::instance().get_downloaded_models();
        result["success"] = true;
        result["count"] = (int)models.size();
        
        for (const auto& model : models) {
            Json::Value m;
            m["id"] = model.id;
            m["name"] = model.name;
            m["type"] = model.type == ModelType::WHISPER ? "whisper" :
                        model.type == ModelType::EMBEDDING ? "embedding" : "llm";
            m["size_bytes"] = (Json::UInt64)model.size_bytes;
            m["local_path"] = model.local_path;
            result["models"].append(m);
        }
        
        return result;
    });
}

} // namespace lucid