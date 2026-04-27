// control_server.cpp - HTTP control server implementation

#include "control_server.hpp"
#include "../logging.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <cstring>
#include <sstream>

namespace lucid {

// Default port for the HTTP server
constexpr int DEFAULT_PORT = 8765;

ControlServer::ControlServer() = default;

ControlServer::~ControlServer() {
    stop();
}

bool ControlServer::start(const std::string& socket_path) {
    if (m_running.load()) {
        LUCID_WARN("[ControlServer] Already running");
        return true;
    }
    
    // Create TCP socket for HTTP server
    m_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_server_fd < 0) {
        LUCID_ERROR("[ControlServer] Failed to create socket");
        return false;
    }
    
    // Allow port reuse
    int opt = 1;
    setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind to localhost only for security
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(DEFAULT_PORT);
    
    if (bind(m_server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LUCID_ERROR("[ControlServer] Failed to bind to port %d", DEFAULT_PORT);
        close(m_server_fd);
        m_server_fd = -1;
        return false;
    }
    
    // Listen for connections
    if (listen(m_server_fd, 5) < 0) {
        LUCID_ERROR("[ControlServer] Failed to listen");
        close(m_server_fd);
        m_server_fd = -1;
        return false;
    }
    
    // Set non-blocking
    fcntl(m_server_fd, F_SETFL, O_NONBLOCK);
    
    m_running = true;
    m_server_thread = std::thread(&ControlServer::run_server_loop, this);
    
    LUCID_INFO("[ControlServer] Started on http://127.0.0.1:%d", DEFAULT_PORT);
    LUCID_INFO("[ControlServer] Commands: GET /api/settings, POST /api/settings/vision, POST /api/command");
    
    return true;
}

void ControlServer::stop() {
    if (!m_running.load()) {
        return;
    }
    
    m_running = false;
    
    if (m_server_fd >= 0) {
        close(m_server_fd);
        m_server_fd = -1;
    }
    
    if (m_server_thread.joinable()) {
        m_server_thread.join();
    }
    
    LUCID_INFO("[ControlServer] Stopped");
}

void ControlServer::register_handler(const std::string& command, CommandHandler handler) {
    std::lock_guard<std::mutex> lock(m_handlers_mutex);
    m_handlers[command] = handler;
}

void ControlServer::send_notification(const std::string& event, const Json::Value& data) {
    std::lock_guard<std::mutex> lock(m_notifications_mutex);
    m_notifications.push({event, data});
}

void ControlServer::run_server_loop() {
    while (m_running.load()) {
        struct pollfd pfd = {m_server_fd, POLLIN, 0};
        int ret = poll(&pfd, 1, 100); // 100ms timeout
        
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        
        if (ret == 0) continue; // Timeout
        
        // Accept connection
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(m_server_fd, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                LUCID_ERROR("[ControlServer] Accept failed: %s", strerror(errno));
            }
            continue;
        }
        
        // Handle client in this thread (simple single-threaded server)
        // Note: handle_client() closes the fd internally
        handle_client(client_fd);
    }
}

void ControlServer::handle_client(int client_fd) {
    std::string request = read_request(client_fd);
    if (request.empty()) {
        return;
    }
    
    // Parse HTTP request
    std::istringstream iss(request);
    std::string method, path, version;
    iss >> method >> path >> version;
    
    // Read body if present
    std::string body;
    size_t body_start = request.find("\r\n\r\n");
    if (body_start != std::string::npos) {
        body = request.substr(body_start + 4);
    }
    
    LUCID_DEBUG("[ControlServer] %s %s", method.c_str(), path.c_str());
    
    // Log request body for debugging
    if (!body.empty() && body.size() < 1000) {
        LUCID_DEBUG("[ControlServer] Body: %s", body.substr(0, 200).c_str());
    }
    
    Json::Value response;
    
    // Route requests
    try {
    if (path == "/api/settings" && method == "GET") {
        // Get all settings
        std::lock_guard<std::mutex> lock(m_handlers_mutex);
        if (m_handlers.count("get_settings")) {
            response = m_handlers["get_settings"](Json::Value());
        } else {
            response["error"] = "Handler not registered";
        }
    }
    else if (path == "/api/settings/vision" && method == "POST") {
        // Set vision scan settings
        Json::Value args = parse_json(body);
        std::lock_guard<std::mutex> lock(m_handlers_mutex);
        if (m_handlers.count("set_vision_scan")) {
            response = m_handlers["set_vision_scan"](args);
        } else {
            response["error"] = "Handler not registered";
        }
    }
    else if (path == "/api/command" && method == "POST") {
        // Generic command
        Json::Value args = parse_json(body);
        std::string cmd = args["command"].asString();
        Json::Value cmd_args = args["args"];
        
        std::lock_guard<std::mutex> lock(m_handlers_mutex);
        if (m_handlers.count(cmd)) {
            response = m_handlers[cmd](cmd_args);
        } else {
            response["error"] = "Unknown command: " + cmd;
        }
    }
    else if (path == "/api/notifications" && method == "GET") {
        // Get pending notifications (SSE-style)
        std::lock_guard<std::mutex> lock(m_notifications_mutex);
        Json::Value notifications;
        while (!m_notifications.empty()) {
            auto [event, data] = m_notifications.front();
            m_notifications.pop();
            Json::Value notif;
            notif["event"] = event;
            notif["data"] = data;
            notifications.append(notif);
        }
        response["notifications"] = notifications;
    }
    else if (path == "/health" && method == "GET") {
        response["status"] = "ok";
        response["server"] = "lucid-control";
    }
    else {
        response["error"] = "Unknown endpoint";
        response["path"] = path;
    }
    } catch (const std::exception& e) {
        LUCID_ERROR("[ControlServer] Exception handling request: %s", e.what());
        response.clear();
        response["error"] = "Internal server error";
        response["exception"] = e.what();
    } catch (...) {
        LUCID_ERROR("[ControlServer] Unknown exception handling request");
        response.clear();
        response["error"] = "Internal server error";
    }
    
    // Log the response we're about to send
    LUCID_DEBUG("[ControlServer] Sending response: %s", response.toStyledString().substr(0, 200).c_str());
    
    std::string http_response = make_response(response);
    ssize_t sent = 0;
    while (sent < (ssize_t)http_response.length()) {
        ssize_t n = send(client_fd, http_response.c_str() + sent, http_response.length() - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            LUCID_WARN("[ControlServer] Send failed: %s", strerror(errno));
            break;
        }
        sent += n;
    }
    
    // Shutdown write side before close to ensure data is sent
    shutdown(client_fd, SHUT_WR);
    close(client_fd);
}

std::string ControlServer::read_request(int client_fd) {
    std::string request;
    char buf[8192];
    
    // Set a timeout on the client socket (5 seconds)
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    // Read all available data (loop until we have enough or connection closes)
    while (true) {
        ssize_t n = recv(client_fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            // Connection closed, timeout, or error
            if (request.empty()) return "";
            break;
        }
        buf[n] = '\0';
        request += buf;
        
        // For POST requests, check if we have the full body
        if (request.find("POST") != std::string::npos) {
            // Look for Content-Length header
            size_t header_end = request.find("\r\n\r\n");
            if (header_end != std::string::npos) {
                size_t body_start = header_end + 4;
                size_t body_length = 0;
                
                // Find Content-Length
                size_t pos = request.find("Content-Length:");
                if (pos != std::string::npos && pos < header_end) {
                    std::string len_str = request.substr(pos + 15);
                    size_t end_pos = len_str.find("\r\n");
                    if (end_pos != std::string::npos) {
                        len_str = len_str.substr(0, end_pos);
                        // Trim whitespace
                        while (!len_str.empty() && (len_str[0] == ' ' || len_str[0] == '\t')) {
                            len_str = len_str.substr(1);
                        }
                        body_length = std::stoul(len_str);
                    }
                }
                
                // Check if we have the full body
                if (request.size() >= body_start + body_length) {
                    break;  // We have everything
                }
                
                // Need more data, but buffer is full - shouldn't happen for our requests
                if (request.size() >= sizeof(buf) - 1) {
                    break;
                }
            }
        } else {
            // For GET requests, we just need the headers
            if (request.find("\r\n\r\n") != std::string::npos) {
                break;
            }
        }
        
        // Safety limit
        if (request.size() > 65536) {
            LUCID_WARN("[ControlServer] Request too large, truncating");
            break;
        }
    }
    
    return request;
}

Json::Value ControlServer::parse_json(const std::string& str) {
    Json::Value result;
    if (str.empty()) return result;
    
    Json::CharReaderBuilder builder;
    std::istringstream iss(str);
    std::string errs;
    
    if (!Json::parseFromStream(builder, iss, &result, &errs)) {
        LUCID_WARN("[ControlServer] JSON parse error: %s", errs.c_str());
    }
    
    return result;
}

std::string ControlServer::make_response(const Json::Value& result, bool error) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string body = Json::writeString(builder, result);
    
    std::ostringstream oss;
    oss << "HTTP/1.1 " << (error ? "400" : "200") << " OK\r\n";
    oss << "Content-Type: application/json\r\n";
    oss << "Content-Length: " << body.length() << "\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "\r\n";
    oss << body;
    
    return oss.str();
}

} // namespace lucid