// toast_notification.cpp - Lucid Toast Notifications Implementation
#include "toast_notification.hpp"
#include "../logging.hpp"

#include <gtk/gtk.h>

namespace lucid {

ToastNotification::ToastNotification() = default;
ToastNotification::~ToastNotification() = default;

void ToastNotification::show(GtkWidget* parent, const std::string& message,
                             ToastType type, int duration_ms) {
    create_toast_widget(parent, message, type, duration_ms);
}

void ToastNotification::show_success(GtkWidget* parent, const std::string& message) {
    show(parent, message, ToastType::SUCCESS);
}

void ToastNotification::show_error(GtkWidget* parent, const std::string& message) {
    show(parent, message, ToastType::ERROR, 5000);  // Errors show longer
}

void ToastNotification::show_info(GtkWidget* parent, const std::string& message) {
    show(parent, message, ToastType::INFO);
}

void ToastNotification::create_toast_widget(GtkWidget* parent, const std::string& message,
                                            ToastType type, int duration_ms) {
    if (!parent) return;
    
    // Create toast container
    GtkWidget* toast = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(toast, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(toast, GTK_ALIGN_END);
    gtk_widget_set_margin_bottom(toast, 16);
    // Use margin instead of padding
    gtk_widget_set_margin_start(toast, 12);
    gtk_widget_set_margin_end(toast, 12);
    gtk_widget_set_margin_top(toast, 8);
    gtk_widget_set_margin_bottom(toast, 8);
    
    // Create icon based on type
    const char* icon_name;
    switch (type) {
        case ToastType::SUCCESS:
            icon_name = "emblem-ok-symbolic";
            break;
        case ToastType::WARNING:
            icon_name = "dialog-warning-symbolic";
            break;
        case ToastType::ERROR:
            icon_name = "dialog-error-symbolic";
            break;
        case ToastType::INFO:
        default:
            icon_name = "dialog-information-symbolic";
            break;
    }
    
    GtkWidget* icon = gtk_image_new_from_icon_name(icon_name);
    gtk_box_append(GTK_BOX(toast), icon);
    
    // Create label
    GtkWidget* label = gtk_label_new(message.c_str());
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(toast), label);
    
    // Set fixed width
    gtk_widget_set_size_request(toast, 300, -1);
    
    // Get the parent window to add the toast as a transient child
    GtkWidget* toplevel = gtk_widget_get_ancestor(parent, GTK_TYPE_WINDOW);
    if (toplevel) {
        gtk_widget_set_parent(toast, toplevel);
        
        // Position at bottom center
        int width = gtk_widget_get_width(toplevel);
        gtk_widget_set_margin_start(toast, (width - 300) / 2);
        
        // Show the toast
        gtk_widget_set_visible(toast, TRUE);
        
        // Schedule removal after duration
        g_timeout_add(duration_ms, on_toast_timeout, toast);
        
        LUCID_DEBUG("[Toast] Showing: %s", message.c_str());
    } else {
        // No toplevel window, just add to parent
        gtk_widget_set_parent(toast, parent);
        gtk_widget_set_visible(toast, TRUE);
        g_timeout_add(duration_ms, on_toast_timeout, toast);
    }
}

gboolean ToastNotification::on_toast_timeout(gpointer data) {
    GtkWidget* toast = GTK_WIDGET(data);
    
    // Remove from parent (this also destroys the widget in GTK4)
    gtk_widget_unparent(toast);
    
    LUCID_DEBUG("[Toast] Dismissed");
    
    // Return FALSE to stop the timeout (will not be called again)
    return G_SOURCE_REMOVE;
}

} // namespace lucid
