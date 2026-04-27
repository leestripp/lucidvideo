// toast_notification.hpp - Lucid Toast Notifications
// Simple toast notification system for user feedback
#pragma once

#include <gtkmm.h>
#include <string>
#include <functional>

namespace lucid {

enum class ToastType {
    INFO,       // Blue-ish, informational
    SUCCESS,    // Green, success messages
    WARNING,    // Orange, warnings
    ERROR       // Red, errors
};

// ToastNotification - Shows temporary toast messages
class ToastNotification {
public:
    ToastNotification();
    ~ToastNotification();
    
    // Show a toast notification
    // parent: The parent window to attach to
    // message: The message to display
    // type: Type of notification (INFO, SUCCESS, WARNING, ERROR)
    // duration: How long to show in milliseconds (default 3000)
    static void show(GtkWidget* parent, const std::string& message, 
                   ToastType type = ToastType::INFO, 
                   int duration_ms = 3000);
    
    // Show success toast
    static void show_success(GtkWidget* parent, const std::string& message);
    
    // Show error toast  
    static void show_error(GtkWidget* parent, const std::string& message);
    
    // Show info toast
    static void show_info(GtkWidget* parent, const std::string& message);

private:
    static void create_toast_widget(GtkWidget* parent, const std::string& message, 
                                   ToastType type, int duration_ms);
    static gboolean on_toast_timeout(gpointer data);
};

} // namespace lucid
