// control_server.hpp - HTTP control server for external control
#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>
#include <queue>
#include <json/json.h>

namespace lucid {

class ControlServer {
public:
    using CommandHandler = std::function<Json::Value(const Json::Value& args)>;
    
    ControlServer();
    ~ControlServer();
    
    // Start the server on a Unix socket
    bool start(const std::string& socket_path = "");
    
    // Stop the server
    void stop();
    
    // Check if running
    bool is_running() const { return m_running.load(); }
    
    // Register a command handler
    void register_handler(const std::string& command, CommandHandler handler);
    
    // Send a notification (for internal use)
    void send_notification(const std::string& event, const Json::Value& data);
    
private:
    void run_server_loop();
    void handle_client(int client_fd);
    std::string read_request(int client_fd);
    Json::Value parse_json(const std::string& str);
    std::string make_response(const Json::Value& result, bool error = false);
    
    std::atomic<bool> m_running{false};
    std::thread m_server_thread;
    int m_server_fd = -1;
    std::string m_socket_path;
    
    std::mutex m_handlers_mutex;
    std::map<std::string, CommandHandler> m_handlers;
    
    std::mutex m_notifications_mutex;
    std::queue<std::pair<std::string, Json::Value>> m_notifications;
};

} // namespace lucid