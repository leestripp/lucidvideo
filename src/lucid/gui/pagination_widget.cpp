// pagination_widget.cpp - Pagination navigation widget implementation
#include "pagination_widget.hpp"
#include "../logging.hpp"

#include <gtk/gtk.h>
#include <glib.h>
#include <cstdio>

namespace lucid {

PaginationWidget::PaginationWidget() = default;

PaginationWidget::~PaginationWidget() = default;

void PaginationWidget::initialize(GtkWidget* parent) {
    // Main horizontal container
    m_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(m_container, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(m_container, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(m_container, 12);
    gtk_widget_set_margin_end(m_container, 12);
    gtk_widget_set_margin_top(m_container, 8);
    gtk_widget_set_margin_bottom(m_container, 8);
    
    // First page button
    m_btn_first = gtk_button_new_from_icon_name("go-first-symbolic");
    gtk_button_set_has_frame(GTK_BUTTON(m_btn_first), FALSE);
    gtk_widget_set_tooltip_text(m_btn_first, "First page");
    gtk_box_append(GTK_BOX(m_container), m_btn_first);
    g_signal_connect(m_btn_first, "clicked", G_CALLBACK(on_first_clicked), this);
    
    // Previous page button
    m_btn_prev = gtk_button_new_from_icon_name("go-previous-symbolic");
    gtk_button_set_has_frame(GTK_BUTTON(m_btn_prev), FALSE);
    gtk_widget_set_tooltip_text(m_btn_prev, "Previous page");
    gtk_box_append(GTK_BOX(m_container), m_btn_prev);
    g_signal_connect(m_btn_prev, "clicked", G_CALLBACK(on_prev_clicked), this);
    
    // Page label (e.g., "Page 1 of 100")
    m_page_label = gtk_label_new("No items");
    gtk_box_append(GTK_BOX(m_container), m_page_label);
    
    // Next page button
    m_btn_next = gtk_button_new_from_icon_name("go-next-symbolic");
    gtk_button_set_has_frame(GTK_BUTTON(m_btn_next), FALSE);
    gtk_widget_set_tooltip_text(m_btn_next, "Next page");
    gtk_box_append(GTK_BOX(m_container), m_btn_next);
    g_signal_connect(m_btn_next, "clicked", G_CALLBACK(on_next_clicked), this);
    
    // Last page button
    m_btn_last = gtk_button_new_from_icon_name("go-last-symbolic");
    gtk_button_set_has_frame(GTK_BUTTON(m_btn_last), FALSE);
    gtk_widget_set_tooltip_text(m_btn_last, "Last page");
    gtk_box_append(GTK_BOX(m_container), m_btn_last);
    g_signal_connect(m_btn_last, "clicked", G_CALLBACK(on_last_clicked), this);
    
    // Separator
    GtkWidget* separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_margin_start(separator, 12);
    gtk_widget_set_margin_end(separator, 12);
    gtk_box_append(GTK_BOX(m_container), separator);
    
    // Initial state
    update_controls();
}

void PaginationWidget::set_total_items(int total) {
    m_total_items = total;
    calculate_total_pages();
    update_controls();
}

void PaginationWidget::set_page_size(int size) {
    // Find the closest matching size
    int closest = PAGE_SIZES[0];
    int min_diff = abs(size - closest);
    for (int i = 1; i < NUM_PAGE_SIZES; i++) {
        int diff = abs(size - PAGE_SIZES[i]);
        if (diff < min_diff) {
            min_diff = diff;
            closest = PAGE_SIZES[i];
        }
    }
    
    if (m_page_size != closest) {
        m_page_size = closest;
        
        calculate_total_pages();
        
        // Clamp current page to valid range
        if (m_current_page >= m_total_pages) {
            m_current_page = (m_total_pages > 0) ? m_total_pages - 1 : 0;
        }
        
        update_controls();
    }
}

void PaginationWidget::set_current_page(int page) {
    if (page < 0) page = 0;
    if (m_total_pages > 0 && page >= m_total_pages) {
        page = m_total_pages - 1;
    }
    
    if (m_current_page != page) {
        m_current_page = page;
        update_controls();
    }
}

void PaginationWidget::set_on_page_changed(PageChangedCallback callback) {
    m_page_changed_callback = callback;
}

void PaginationWidget::set_on_page_size_changed(PageSizeChangedCallback callback) {
    m_page_size_changed_callback = callback;
}

void PaginationWidget::show() {
    if (m_container && !m_visible) {
        gtk_widget_set_visible(m_container, TRUE);
        m_visible = true;
    }
}

void PaginationWidget::hide() {
    if (m_container && m_visible) {
        gtk_widget_set_visible(m_container, FALSE);
        m_visible = false;
    }
}

void PaginationWidget::update_controls() {
    // Update button sensitivity
    bool has_items = m_total_items > 0;
    bool can_go_prev = has_items && m_current_page > 0;
    bool can_go_next = has_items && m_current_page < m_total_pages - 1;
    
    gtk_widget_set_sensitive(m_btn_first, can_go_prev);
    gtk_widget_set_sensitive(m_btn_prev, can_go_prev);
    gtk_widget_set_sensitive(m_btn_next, can_go_next);
    gtk_widget_set_sensitive(m_btn_last, can_go_next);
    
    update_page_label();
}

void PaginationWidget::update_page_label() {
    if (m_total_items == 0) {
        gtk_label_set_text(GTK_LABEL(m_page_label), "No items");
    } else {
        char label[64];
        snprintf(label, sizeof(label), "Page %d of %d", m_current_page + 1, m_total_pages);
        gtk_label_set_text(GTK_LABEL(m_page_label), label);
    }
}

void PaginationWidget::calculate_total_pages() {
    if (m_total_items <= 0 || m_page_size <= 0) {
        m_total_pages = 0;
    } else {
        m_total_pages = (m_total_items + m_page_size - 1) / m_page_size;
        if (m_total_pages == 0) m_total_pages = 1;
    }
}

// Signal handlers
void PaginationWidget::on_first_clicked(GtkButton* button, gpointer data) {
    auto* self = static_cast<PaginationWidget*>(data);
    int old_page = self->m_current_page;
    self->m_current_page = 0;
    self->update_controls();
    
    if (old_page != 0 && self->m_page_changed_callback) {
        self->m_page_changed_callback(0, self->m_page_size);
    }
}

void PaginationWidget::on_prev_clicked(GtkButton* button, gpointer data) {
    auto* self = static_cast<PaginationWidget*>(data);
    if (self->m_current_page > 0) {
        int old_page = self->m_current_page;
        self->m_current_page--;
        self->update_controls();
        
        if (old_page != self->m_current_page && self->m_page_changed_callback) {
            self->m_page_changed_callback(self->m_current_page, self->m_page_size);
        }
    }
}

void PaginationWidget::on_next_clicked(GtkButton* button, gpointer data) {
    auto* self = static_cast<PaginationWidget*>(data);
    if (self->m_current_page < self->m_total_pages - 1) {
        int old_page = self->m_current_page;
        self->m_current_page++;
        self->update_controls();
        
        if (old_page != self->m_current_page && self->m_page_changed_callback) {
            self->m_page_changed_callback(self->m_current_page, self->m_page_size);
        }
    }
}

void PaginationWidget::on_last_clicked(GtkButton* button, gpointer data) {
    auto* self = static_cast<PaginationWidget*>(data);
    int old_page = self->m_current_page;
    self->m_current_page = self->m_total_pages > 0 ? self->m_total_pages - 1 : 0;
    self->update_controls();
    
    if (old_page != self->m_current_page && self->m_page_changed_callback) {
        self->m_page_changed_callback(self->m_current_page, self->m_page_size);
    }
}

} // namespace lucid
