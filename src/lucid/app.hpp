// app.hpp - Lucid Application Class
//
// Minimal GTK4 Application for initial build

#pragma once

#include <gtkmm.h>

#include <memory>
#include <functional>

#include "search/semantic_search.hpp"

namespace lucid {

// Forward declaration for global access
class LucidApp;

// Global pointer to the current app instance - needed for static callback functions
// that can't capture context (like g_timeout_add lambdas used in stress testing)
// This is defined in app.cpp
extern Glib::RefPtr<LucidApp> g_app;

class VideoAnalysisWorker;
class MainWindow;
class ControlServer;
class VideoIndexer;

class LucidApp : public Gtk::Application {
public:
    static Glib::RefPtr<LucidApp> create();
    ~LucidApp() override;

protected:
    void on_activate() override;
    void on_open(const std::vector<Glib::RefPtr<Gio::File>>& files,
                 const Glib::ustring& hint) override;
    void on_shutdown() override;

public:
    // Control server API for external access
    void start_control_server();
    void stop_control_server();
    
    // Set callbacks for analysis worker updates (called from MainWindow)
    // These can be called after the worker is created
    // New pass-aware signature:
    // video_id, pass_state ("pass1"/"pass2"), phase, progress, vision_analyzed, vision_total
    void set_analysis_progress_callback(std::function<void(const std::string&, const std::string&, const std::string&, float, int, int)> cb);
    void set_analysis_complete_callback(std::function<void(const std::string&, bool, const std::string&)> cb);
    void set_queue_update_callback(std::function<void(int, int)> cb);
    void set_vision_update_callback(std::function<void(const std::string&)> cb);  // Called after Pass 2
    void reset_all_videos_to_pending();  // For re-analysis
    void request_cancel_analysis();      // Request cancellation of current job
    
    // Pause/resume analysis worker (for when user is in settings)
    void pause_analysis();
    void resume_analysis();
    bool is_analysis_paused() const;
    
    // Direct access to worker (for callback registration)
    VideoAnalysisWorker* get_analysis_worker() const { return m_analysis_worker.get(); }
    
    // Direct access to search (for API testing)
    SemanticSearch* get_semantic_search() const { return m_semantic_search.get(); }
    VideoIndexer* get_indexer() const { return m_indexer.get(); }
    
    // Direct access to database
    Database* get_database() const { return m_database.get(); }

    // Get the global app instance (for static callbacks that can't capture context)
    static LucidApp* get_global() { return g_app.get(); }

private:
    explicit LucidApp();
    void create_window();
    void open_video(const Glib::ustring& path);
    void create_analysis_worker();  // Create worker without starting
    void setup_analysis_worker();   // Create and start worker (legacy)
    
    // Analysis worker callbacks for UI
    void setup_analysis_callbacks();
    
    // Control server command handlers
    void setup_control_server_handlers();
    
    std::unique_ptr<Database> m_database;
    std::unique_ptr<VideoIndexer> m_indexer;
    std::unique_ptr<VideoAnalysisWorker> m_analysis_worker;
    std::unique_ptr<ControlServer> m_control_server;
    std::unique_ptr<SemanticSearch> m_semantic_search;  // For search API
    
    // Keep reference to window to prevent destruction
    Glib::RefPtr<MainWindow> m_window;
};

} // namespace lucid