// chat_panel.hpp - Chat Panel Widget
#pragma once

#include <gtkmm.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>

#include "../inference/llm_engine.hpp"

namespace lucid {

class Database;
class ChatSession;

// ============================================================================
// Chat Panel
// ============================================================================

class ChatPanel : public Gtk::Box {
public:
    ChatPanel();
    ~ChatPanel() override;

    void set_database(Database* db);
    void set_chat_session(ChatSession* session);
    
    void clear_messages();
    void add_user_message(const std::string& message);
    void add_assistant_message(const std::string& message, bool is_error = false);
    void set_input_enabled(bool enabled);
    
    // Show/hide thinking indicator
    void show_thinking();
    void hide_thinking();
    
    // Response callback (called by MainWindow after LLM generates response)
    void on_response_ready(const std::string& response, bool success);
    
    // Callback for when user submits a message (for external handling)
    sigc::signal<void(const std::string&)> signal_message_submitted();
    
    // Callback for when a video title is clicked in chat
    // The string is the video_id to navigate to
    sigc::signal<void(const std::string&)> signal_video_clicked();
    
    // Callback for when user wants to search a message's content
    // The string is the message text to search for
    sigc::signal<void(const std::string&)> signal_search_content();
    
    // Accessor for search signal (for external connections)
    sigc::signal<void(const std::string&)>& signal_search() { return m_signal_search_content; }
    
    // Set callback for video navigation
    void set_on_video_clicked(std::function<void(const std::string&)> callback) {
        m_on_video_clicked = callback;
    }
    
    // Set callback for search content (alternative to signal)
    void set_on_search_content(std::function<void(const std::string&)> callback) {
        m_on_search_content = callback;
    }

private:
    void setup_ui();
    void on_send_clicked();
    void on_input_activate();
    void add_message_to_display(const ChatMessage& msg);
    void scroll_to_bottom();

    Database* m_db = nullptr;
    ChatSession* m_session = nullptr;

    Gtk::Box m_messages_box;
    Gtk::ScrolledWindow m_scroll;
    Gtk::TextView m_input_view;
    Gtk::Button m_send_btn;
    Gtk::Button m_clear_btn;
    Gtk::Box m_thinking_indicator;
    std::vector<ChatMessage> m_messages;
    
    // Callback for video navigation
    std::function<void(const std::string&)> m_on_video_clicked;
    
    // Callback for search content
    std::function<void(const std::string&)> m_on_search_content;
    
    // Helper to parse and display assistant messages with clickable video links
    void add_assistant_message_with_links(const std::string& message, bool is_error);
    
    // Find video ID by title (fuzzy match)
    std::string find_video_id_by_title(const std::string& title);

    sigc::signal<void(const std::string&)> m_signal_message_submitted;
    sigc::signal<void(const std::string&)> m_signal_video_clicked;
    sigc::signal<void(const std::string&)> m_signal_search_content;
};

} // namespace lucid
