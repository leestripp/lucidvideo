// chat_panel.cpp - Chat Panel Widget Implementation
#include "chat_panel.hpp"
#include "../model/database.hpp"
#include "../inference/chat_session.hpp"
#include "../logging.hpp"

#include <glibmm/ustring.h>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <set>

namespace lucid {

ChatPanel::ChatPanel()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 0)
    , m_messages_box(Gtk::Orientation::VERTICAL, 8)
    , m_scroll()
    , m_thinking_indicator(Gtk::Orientation::HORIZONTAL, 8)
{
    setup_ui();
}

ChatPanel::~ChatPanel() = default;

void ChatPanel::setup_ui() {
    // Messages area (scrollable)
    m_scroll.set_hexpand(true);
    m_scroll.set_vexpand(true);
    m_scroll.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    m_scroll.set_size_request(-1, 300);
    
    // Header bar with clear button (placed before scroll in box)
    auto header = Gtk::manage(new Gtk::Box(Gtk::Orientation::HORIZONTAL, 8));
    header->set_halign(Gtk::Align::FILL);
    header->set_margin_start(12);
    header->set_margin_end(12);
    header->set_margin_top(8);
    
    auto title = Gtk::manage(new Gtk::Label("Chat"));
    title->set_hexpand(true);
    title->set_halign(Gtk::Align::START);
    title->add_css_class("title");
    
    m_clear_btn.set_label("Clear");
    m_clear_btn.set_has_frame(false);
    m_clear_btn.signal_clicked().connect(sigc::mem_fun(*this, &ChatPanel::clear_messages));
    
    header->append(*title);
    header->append(m_clear_btn);
    append(*header);
    append(m_scroll);

    // Create viewport with adjustments
    auto hadj = Gtk::Adjustment::create(0.0, 0.0, 100.0, 10.0, 50.0, 50.0);
    auto vadj = Gtk::Adjustment::create(0.0, 0.0, 100.0, 10.0, 50.0, 50.0);
    auto viewport = Gtk::manage(new Gtk::Viewport(hadj, vadj));
    viewport->set_child(m_messages_box);
    m_scroll.set_child(*viewport);

    // Welcome message if empty
    Gtk::Label* welcome = Gtk::manage(new Gtk::Label("Welcome to Chat! Ask me about your video library."));
    welcome->set_halign(Gtk::Align::START);
    welcome->set_margin_start(12);
    welcome->set_margin_end(12);
    welcome->set_margin_top(12);
    welcome->set_margin_bottom(12);
    welcome->set_selectable(true);
    m_messages_box.append(*welcome);

    // Thinking indicator (hidden by default)
    m_thinking_indicator.set_visible(false);
    m_thinking_indicator.set_margin_start(12);
    m_thinking_indicator.set_margin_end(12);
    m_thinking_indicator.set_margin_top(8);
    m_thinking_indicator.set_margin_bottom(8);
    
    Gtk::Spinner* spinner = Gtk::manage(new Gtk::Spinner());
    spinner->start();
    m_thinking_indicator.append(*spinner);
    
    Gtk::Label* thinking_label = Gtk::manage(new Gtk::Label("Thinking..."));
    thinking_label->set_opacity(0.7);
    m_thinking_indicator.append(*thinking_label);
    
    m_messages_box.append(m_thinking_indicator);

    // Input area
    auto input_box = Gtk::manage(new Gtk::Box(Gtk::Orientation::HORIZONTAL, 8));
    input_box->set_margin_start(8);
    input_box->set_margin_end(8);
    input_box->set_margin_bottom(8);
    input_box->set_halign(Gtk::Align::FILL);
    append(*input_box);

    // Text input
    m_input_view.set_hexpand(true);
    m_input_view.set_size_request(-1, 80);
    m_input_view.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    m_input_view.set_editable(true);
    m_input_view.set_cursor_visible(true);

    auto input_sw = Gtk::manage(new Gtk::ScrolledWindow());
    input_sw->set_hexpand(true);
    input_sw->set_child(m_input_view);
    input_box->append(*input_sw);

    // Send button
    m_send_btn.set_label("Send");
    m_send_btn.set_sensitive(false);
    m_send_btn.signal_clicked().connect(sigc::mem_fun(*this, &ChatPanel::on_send_clicked));
    input_box->append(m_send_btn);

    // Track text changes to enable/disable send button
    auto buffer = m_input_view.get_buffer();
    buffer->signal_changed().connect([this, buffer]() {
        auto text = buffer->get_text();
        m_send_btn.set_sensitive(!text.empty() && m_session);
    });
}

void ChatPanel::set_database(Database* db) {
    m_db = db;
}

void ChatPanel::set_chat_session(ChatSession* session) {
    m_session = session;
    
    // Enable send button if there's text
    auto buffer = m_input_view.get_buffer();
    auto text = buffer->get_text();
    m_send_btn.set_sensitive(!text.empty() && m_session != nullptr);
}

void ChatPanel::clear_messages() {
    // Remove all children except create new ones
    while (auto child = m_messages_box.get_first_child()) {
        m_messages_box.remove(*child);
    }
    m_messages.clear();
    
    // Add welcome message
    Gtk::Label* welcome = Gtk::manage(new Gtk::Label("Chat cleared. Ask me about your video library!"));
    welcome->set_halign(Gtk::Align::START);
    welcome->set_margin_start(12);
    welcome->set_margin_end(12);
    welcome->set_margin_top(12);
    welcome->set_margin_bottom(12);
    m_messages_box.append(*welcome);
}

void ChatPanel::add_message_to_display(const ChatMessage& msg) {
    auto alignment = Gtk::manage(new Gtk::Box());
    
    if (msg.role == ChatMessage::Role::USER) {
        alignment->set_halign(Gtk::Align::END);
    } else {
        alignment->set_halign(Gtk::Align::START);
    }
    
    auto bubble = Gtk::manage(new Gtk::Box(Gtk::Orientation::VERTICAL, 4));
    bubble->set_margin_start(12);
    bubble->set_margin_end(12);
    bubble->set_margin_top(4);
    bubble->set_margin_bottom(4);
    bubble->set_size_request(200, -1);
    
    // Set bubble color based on role
    if (msg.role == ChatMessage::Role::USER) {
        // Blue for user
        bubble->add_css_class("chat-bubble-user");
    } else {
        // Gray for assistant
        bubble->add_css_class("chat-bubble-assistant");
        if (msg.is_error) {
            bubble->add_css_class("error");
        }
    }
    
    auto label = Gtk::manage(new Gtk::Label(msg.content));
    label->set_halign(Gtk::Align::START);
    label->set_wrap_mode(Pango::WrapMode::WORD_CHAR);
    label->set_wrap(true);
    label->set_selectable(true);
    label->set_xalign(0);
    
    bubble->append(*label);
    alignment->append(*bubble);
    m_messages_box.append(*alignment);
    
    scroll_to_bottom();
}

void ChatPanel::show_thinking() {
    // Insert thinking indicator before last child (input area)
    m_thinking_indicator.set_visible(true);
    scroll_to_bottom();
}

void ChatPanel::hide_thinking() {
    m_thinking_indicator.set_visible(false);
}

void ChatPanel::scroll_to_bottom() {
    // Scroll to end of scrolled window
    auto vadj = m_scroll.get_vadjustment();
    if (vadj) {
        vadj->set_value(vadj->get_upper() - vadj->get_page_size());
    }
}

void ChatPanel::set_input_enabled(bool enabled) {
    m_input_view.set_editable(enabled);
    m_send_btn.set_sensitive(enabled && m_session);
}

void ChatPanel::add_user_message(const std::string& message) {
    ChatMessage msg;
    msg.role = ChatMessage::Role::USER;
    msg.content = message;
    m_messages.push_back(msg);
    add_message_to_display(msg);
}

void ChatPanel::add_assistant_message(const std::string& message, bool is_error) {
    ChatMessage msg;
    msg.role = ChatMessage::Role::ASSISTANT;
    msg.content = message;
    msg.is_error = is_error;
    m_messages.push_back(msg);
    
    // Use clickable version if not an error
    if (!is_error) {
        add_assistant_message_with_links(message, false);
    } else {
        add_message_to_display(msg);
    }
}

void ChatPanel::on_send_clicked() {
    auto buffer = m_input_view.get_buffer();
    auto text = buffer->get_text();
    if (text.empty()) return;

    // Add user message
    add_user_message(std::string(text));
    buffer->set_text("");

    // Show thinking indicator
    show_thinking();
    set_input_enabled(false);

    // Emit signal for external handling
    m_signal_message_submitted.emit(std::string(text));
}

void ChatPanel::on_response_ready(const std::string& response, bool success) {
    hide_thinking();
    set_input_enabled(true);
    
    if (success) {
        add_assistant_message(response);
    } else {
        add_assistant_message("Sorry, I couldn't generate a response.", true);
    }
}

sigc::signal<void(const std::string&)> ChatPanel::signal_message_submitted() {
    return m_signal_message_submitted;
}

sigc::signal<void(const std::string&)> ChatPanel::signal_video_clicked() {
    return m_signal_video_clicked;
}

std::string ChatPanel::find_video_id_by_title(const std::string& title) {
    if (!m_db || title.empty()) return "";
    
    // Clean for comparison
    auto clean = [](const std::string& s) -> std::string {
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        // Remove file extension
        size_t dot = result.rfind('.');
        if (dot != std::string::npos && dot > 0) {
            result = result.substr(0, dot);
        }
        return result;
    };
    
    std::string search_clean = clean(title);
    
    // Get all videos and find exact match
    auto videos = m_db->get_all_videos();
    
    for (const auto& v : videos) {
        std::string video_clean = clean(v.title);
        
        // Exact match (case-insensitive, no extension)
        if (video_clean == search_clean) {
            return v.id;
        }
    }
    
    return "";  // No exact match - don't create a link
}

void ChatPanel::add_assistant_message_with_links(const std::string& message, bool is_error) {
    // Create alignment container
    auto alignment = Gtk::manage(new Gtk::Box());
    alignment->set_halign(Gtk::Align::START);
    
    auto bubble = Gtk::manage(new Gtk::Box(Gtk::Orientation::VERTICAL, 4));
    bubble->set_margin_start(12);
    bubble->set_margin_end(12);
    bubble->set_margin_top(4);
    bubble->set_margin_bottom(4);
    bubble->set_size_request(200, -1);
    bubble->add_css_class("chat-bubble-assistant");
    if (is_error) {
        bubble->add_css_class("error");
    }
    
    // Parse message for video titles - look for quoted text and bullet points
    std::string text = message;
    
    // Simple approach: find video titles in quotes
    // Pattern: 'Title Here' or "Title Here"
    std::vector<std::pair<std::string, std::string>> replacements;  // title -> video_id
    
    // Find quoted titles (both single and double quotes)
    size_t pos = 0;
    while (pos < text.length()) {
        // Look for opening quote
        char quote_char = 0;
        if (text[pos] == '\'' || text[pos] == '"') {
            quote_char = text[pos];
        } else {
            pos++;
            continue;
        }
        
        // Find closing quote
        size_t end = pos + 1;
        while (end < text.length() && text[end] != quote_char) {
            end++;
        }
        
        if (end > pos + 1 && end < text.length()) {
            std::string quoted_title = text.substr(pos + 1, end - pos - 1);
            
            // Skip short strings and things that look like URLs or times
            if (quoted_title.length() >= 5 && 
                quoted_title.find("http") == std::string::npos &&
                quoted_title.find("://") == std::string::npos &&
                quoted_title.find("10:") == std::string::npos &&
                quoted_title.find("20:") == std::string::npos &&
                quoted_title.find("30:") == std::string::npos) {
                
                std::string video_id = find_video_id_by_title(quoted_title);
                if (!video_id.empty()) {
                    replacements.push_back(std::make_pair(quoted_title, video_id));
                }
            }
        }
        pos = end + 1;
    }
    
    // Also look for bullet points like "- Title" or "1. Title"
    size_t bullet_pos = 0;
    while (bullet_pos < text.length()) {
        // Look for bullet patterns at start of line or after newline
        bool at_line_start = (bullet_pos == 0) || (text[bullet_pos-1] == '\n');
        
        if (at_line_start) {
            // Check for "- " or "* " or numbered pattern
            if (text.substr(bullet_pos, 2) == "- " || text.substr(bullet_pos, 2) == "* ") {
                size_t line_end = text.find('\n', bullet_pos);
                if (line_end == std::string::npos) line_end = text.length();
                
                std::string line = text.substr(bullet_pos + 2, line_end - bullet_pos - 2);
                
                // Extract title (up to first colon or parenthesis)
                std::string potential_title = line;
                size_t cutoff = std::string::npos;
                
                size_t colon = line.find(':');
                size_t paren = line.find('(');
                
                if (colon != std::string::npos && (paren == std::string::npos || colon < paren)) {
                    cutoff = colon;
                } else if (paren != std::string::npos) {
                    cutoff = paren;
                }
                
                if (cutoff != std::string::npos) {
                    potential_title = line.substr(0, cutoff);
                }
                
                // Trim
                while (!potential_title.empty() && std::isspace(potential_title.back())) {
                    potential_title.pop_back();
                }
                
                if (potential_title.length() >= 5) {
                    std::string video_id = find_video_id_by_title(potential_title);
                    if (!video_id.empty()) {
                        replacements.push_back(std::make_pair(potential_title, video_id));
                    }
                }
            }
            // Check for numbered pattern "1. Title"
            else if (bullet_pos + 2 < text.length() && 
                     std::isdigit(text[bullet_pos]) &&
                     text[bullet_pos + 1] == '.') {
                size_t line_end = text.find('\n', bullet_pos);
                if (line_end == std::string::npos) line_end = text.length();
                
                std::string line = text.substr(bullet_pos + 2, line_end - bullet_pos - 2);
                
                // Extract title
                std::string potential_title = line;
                size_t cutoff = std::string::npos;
                
                size_t colon = line.find(':');
                size_t paren = line.find('(');
                
                if (colon != std::string::npos && (paren == std::string::npos || colon < paren)) {
                    cutoff = colon;
                } else if (paren != std::string::npos) {
                    cutoff = paren;
                }
                
                if (cutoff != std::string::npos) {
                    potential_title = line.substr(0, cutoff);
                }
                
                while (!potential_title.empty() && std::isspace(potential_title.back())) {
                    potential_title.pop_back();
                }
                
                if (potential_title.length() >= 5) {
                    std::string video_id = find_video_id_by_title(potential_title);
                    if (!video_id.empty()) {
                        replacements.push_back(std::make_pair(potential_title, video_id));
                    }
                }
            }
        }
        
        // Move to next line
        size_t next_newline = text.find('\n', bullet_pos + 1);
        if (next_newline == std::string::npos) break;
        bullet_pos = next_newline + 1;
    }
    
    // Build the content with clickable links
    if (replacements.empty()) {
        // No matches - just add plain text
        auto label = Gtk::manage(new Gtk::Label());
        label->set_text(message);
        label->set_halign(Gtk::Align::START);
        label->set_wrap_mode(Pango::WrapMode::WORD_CHAR);
        label->set_wrap(true);
        label->set_selectable(true);
        label->set_xalign(0);
        bubble->append(*label);
    } else {
        // Build text with links
        auto label = Gtk::manage(new Gtk::Label());
        label->set_halign(Gtk::Align::START);
        label->set_wrap_mode(Pango::WrapMode::WORD_CHAR);
        label->set_wrap(true);
        label->set_selectable(true);
        label->set_xalign(0);
        
        // Enable link activation to intercept video: links
        // Note: In GTK4, we set the property to enable link activation
        g_object_set(G_OBJECT(label->gobj()), "activate-link", TRUE, NULL);
        
        label->signal_activate_link().connect(
            sigc::track_obj(
                [this](const std::string& uri) -> bool {
                    LUCID_DEBUG("[ChatPanel] Link activated: %s", uri.c_str());
                    // Parse video:VIDEO_ID format
                    if (uri.substr(0, 6) == "video:") {
                        std::string video_id = uri.substr(6);
                        if (m_on_video_clicked) {
                            m_on_video_clicked(video_id);
                            return true;  // Link handled
                        }
                        m_signal_video_clicked.emit(video_id);
                        return true;  // Link handled
                    }
                    return false;  // Let GTK handle other links
                },
                *this
            ),
            false  // before handlers (not after)
        );
        
        // Escape function for Pango markup
        auto escape = [](const std::string& s) -> std::string {
            std::string result;
            for (char c : s) {
                switch (c) {
                    case '&': result += "&amp;"; break;
                    case '<': result += "&lt;"; break;
                    case '>': result += "&gt;"; break;
                    case '"': result += "&quot;"; break;
                    case '\'': result += "&apos;"; break;
                    default: result += c; break;
                }
            }
            return result;
        };
        
        // Sort replacements by position in text
        std::vector<std::pair<size_t, std::pair<std::string, std::string>>> sorted;
        for (const auto& rp : replacements) {
            size_t found = text.find(rp.first);
            if (found != std::string::npos) {
                sorted.push_back(std::make_pair(found, rp));
            }
        }
        
        std::sort(sorted.begin(), sorted.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        
        // Build markup
        std::string markup;
        size_t last_end = 0;
        
        for (const auto& entry : sorted) {
            size_t pos = entry.first;
            const std::string& title = entry.second.first;
            const std::string& video_id = entry.second.second;
            
            markup += escape(text.substr(last_end, pos - last_end));
            markup += "<a href=\"video:" + video_id + "\">" + escape(title) + "</a>";
            last_end = pos + title.length();
        }
        
        if (last_end < text.length()) {
            markup += escape(text.substr(last_end));
        }
        
        label->set_markup(markup);
        
        bubble->append(*label);
    }
    
    // Add search button to assistant message bubble (bottom-right corner)
    auto button_row = Gtk::manage(new Gtk::Box(Gtk::Orientation::HORIZONTAL, 0));
    button_row->set_halign(Gtk::Align::END);
    button_row->set_margin_top(4);
    button_row->set_margin_bottom(2);
    
    auto search_btn = Gtk::manage(new Gtk::Button());
    search_btn->set_tooltip_text("Search this response");
    search_btn->set_has_frame(false);
    search_btn->set_sensitive(true);
    
    // Create a small search icon using Unicode magnifying glass
    auto icon_label = Gtk::manage(new Gtk::Label("🔍"));
    search_btn->set_child(*icon_label);
    
    // Copy message text for the callback
    std::string msg_copy = message;
    search_btn->signal_clicked().connect(
        sigc::track_obj(
            [this, msg_copy]() {
                LUCID_DEBUG("[ChatPanel] Search button clicked for message");
                // Emit the search signal/callback with the message content
                if (m_on_search_content) {
                    m_on_search_content(msg_copy);
                }
                m_signal_search_content.emit(msg_copy);
            },
            *this
        )
    );
    
    button_row->append(*search_btn);
    bubble->append(*button_row);
    
    alignment->append(*bubble);
    m_messages_box.append(*alignment);
    
    scroll_to_bottom();
}

} // namespace lucid
