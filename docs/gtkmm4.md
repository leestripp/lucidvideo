# GTK4 and libadwaita (gtkmm4) Documentation

## Overview

GTK4 (GIMP Toolkit) is a cross-platform GUI framework used primarily for GNOME applications. gtkmm4 is the official C++ API for GTK, providing an object-oriented interface with modern C++ features.

**libadwaita** extends GTK4 with GNOME-specific widgets and patterns, including the AdwActionRow, AdwNavigationSplitView, and modern layout components.

**Repository**: [github.com/GNOME/gtkmm](https://github.com/GNOME/gtkmm)
**Documentation**: [gtkmm.org](https://gtkmm.org/) | [docs.gtk.org/gtk4](https://docs.gtk.org/gtk4/)

---

## Installation (Arch Linux)

### System Dependencies

```bash
sudo pacman -S gtk4 libadwaita
```

### Development Packages

```bash
# GTK4 and libadwaita development files
sudo pacman -S gtk4 libadwaita gobject-introspection \
    glibmm gtkmm4 adwaita-mm

# Build tools
sudo pacman -S meson pkgconf cmake ninja
```

### Verify Installation

```bash
# Check pkg-config
pkg-config --modversion gtk4
pkg-config --modversion libadwaita-1
pkg-config --modversion gtkmm-4.0
```

---

## Building Applications with CMake

### Basic CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)
project(Lucid VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find GTK4 and gtkmm4
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtk4)
pkg_check_modules(GTKMM4 REQUIRED gtkmm-4.0)
pkg_check_modules(ADWAITA REQUIRED libadwaita-1)
pkg_check_modules(LIBADWAITA-MM REQUIRED libadwaitamm-1)

# Include directories
include_directories(
    ${GTK4_INCLUDE_DIRS}
    ${GTKMM4_INCLUDE_DIRS}
    ${ADWAITA_INCLUDE_DIRS}
    ${LIBADWAITA-MM_INCLUDE_DIRS}
)

# Create executable
add_executable(lucid
    src/main.cpp
    src/app.cpp
    src/window.cpp
    # ... other sources
)

# Link libraries
target_link_libraries(lucid PRIVATE
    ${GTK4_LIBRARIES}
    ${GTKMM4_LIBRARIES}
    ${ADWAITA_LIBRARIES}
    ${LIBADWAITA-MM_LIBRARIES}
    pthread
)

# Copy resources (UI files, etc.)
target_sources(lucid PRIVATE
    resources/window.ui
    resources/styles.css
)
```

### Alternative with gtkmm4's CMake module

```cmake
find_package(gtkmm4 4.10 REQUIRED)

add_executable(lucid main.cpp)
target_link_libraries(lucid PRIVATE gtkmm::gtkmm4 adwaita::adwaita)
```

---

## Application Structure

### main.cpp - Entry Point

```cpp
#include <gtkmm.h>
#include <adwaita.h>

int main(int argc, char** argv) {
    auto app = Adw::Application::create(
        "org.lucid.Lucid",
        Gio::APPLICATION_NON_UNIQUE
    );
    
    return app->run(argc, argv);
}
```

### Application Class

```cpp
// app.hpp
#pragma once

#include <gtkmm.h>
#include <adwaita.h>

class LucidApp : public Adw::Application {
public:
    LucidApp();
    
protected:
    // Override activate to create window
    void on_activate() override;
    
    // Handle command line
    void on_open(const std::vector<Glib::RefPtr<Gio::File>>& files,
                 const Glib::ustring& hint) override;
    
private:
    void create_window();
    Glib::RefPtr<Gtk::Window> m_window;
};

// app.cpp
#include "app.hpp"
#include "window.hpp"

LucidApp::LucidApp()
    : Adw::Application("org.lucid.Lucid",
                       Gio::APPLICATION_NON_UNIQUE |
                       Gio::APPLICATION_HANDLES_OPEN) {
    
    // Register action for quitting
    add_action("quit", [this]() { quit(); });
    
    // Set keyboard shortcut for quit
    set_accel_for_action("app.quit", "<Ctrl>Q");
}

void LucidApp::on_activate() {
    if (!m_window) {
        create_window();
    }
    m_window->present();
}

void LucidApp::on_open(const std::vector<Glib::RefPtr<Gio::File>>& files,
                       const Glib::ustring& hint) {
    // Handle file open
    for (const auto& file : files) {
        // Process video file
    }
}

void LucidApp::create_window() {
    m_window = Gtk::Builder::create_from_resource(
        "/org/lucid/window.ui"
    )->get_widget<Gtk::Window>("window");
    
    // Or create programmatically
    // m_window = Gtk::Builder::create()->get_widget<Gtk::Window>("main_window");
    
    add_window(*m_window);
}
```

---

## Window and Layout

### AdwApplicationWindow

```cpp
// window.hpp
#pragma once

#include <gtkmm.h>
#include <adwaita.h>

class MainWindow : public Adw::ApplicationWindow {
public:
    MainWindow(BaseObjectType* obj, 
               const Glib::RefPtr<Gtk::Builder>& builder);
    
    static Glib::RefPtr<MainWindow> create();

private:
    void setup_views();
    void setup_actions();
    
    // Child widgets
    Gtk::Widget* m_content;
    Adw::NavigationSplitView* m_split_view;
    Adw::ToolbarView* m_toolbar_view;
};

// window.cpp
#include "window.hpp"

MainWindow::MainWindow(BaseObjectType* obj,
                      const Glib::RefPtr<Gtk::Builder>& builder)
    : Adw::ApplicationWindow(obj) {
    
    // Get widgets from builder
    builder->get_widget("content", m_content);
    builder->get_widget_derived("split_view", m_split_view);
    builder->get_widget("toolbar_view", m_toolbar_view);
    
    setup_views();
    setup_actions();
}

Glib::RefPtr<MainWindow> MainWindow::create() {
    auto builder = Gtk::Builder::create_from_resource(
        "/org/lucid/ui/main_window.ui"
    );
    
    MainWindow* window = nullptr;
    builder->get_widget_derived("main_window", window);
    
    return Glib::wrap(window);
}
```

### UI File (XML)

```xml
<!-- resources/ui/main_window.ui -->
<interface>
    <template class="MainWindow" parent="AdwApplicationWindow">
        <property name="default-width">1200</property>
        <property name="default-height">800</property>
        
        <child>
            <object class="AdwToolbarView">
                <property name="reveal-flips">true</property>
                
                <!-- Header Bar -->
                <child type="top">
                    <object class="AdwHeaderBar">
                        <property name="show-title">true</property>
                        <property name="title-widget">
                            <object class="GtkLabel">
                                <property name="label">Lucid</property>
                            </object>
                        </property>
                    </object>
                </child>
                
                <!-- Content -->
                <child>
                    <object class="AdwNavigationSplitView" id="split_view">
                        <!-- Sidebar -->
                        <property name="sidebar">
                            <object class="AdwNavigationPage">
                                <property name="title">Library</property>
                                <child>
                                    <object class="GtkScrolledWindow">
                                        <property name="hscrollbar-policy">never</property>
                                        <child>
                                            <object class="GtkListView" id="sidebar_list">
                                                <property name="vexpand">true</property>
                                            </object>
                                        </child>
                                    </object>
                                </child>
                            </object>
                        </property>
                        
                        <!-- Content -->
                        <property name="content">
                            <object class="GtkStack" id="content_stack">
                                <child name="home">
                                    <object class="GtkLabel">
                                        <property name="label">Home Content</property>
                                    </object>
                                </child>
                                <child name="search">
                                    <object class="GtkLabel">
                                        <property name="label">Search Content</property>
                                    </object>
                                </child>
                            </object>
                        </property>
                    </object>
                </child>
            </object>
        </child>
    </template>
</interface>
```

---

## libadwaita Widgets

### AdwNavigationSplitView

Three-pane layout with collapsible sidebar:

```cpp
// Create split view
auto split_view = Adw::NavigationSplitView::create();

// Set sidebar (left pane)
auto sidebar = Gtk::Builder::create_from_resource(
    "/org/lucid/ui/sidebar.ui"
)->get_widget<Gtk::Widget>("sidebar");
split_view->set_sidebar(sidebar);

// Set content (right pane)
auto content = Gtk::Builder::create_from_resource(
    "/org/lucid/ui/content.ui"
)->get_widget<Gtk::Widget>("content");
split_view->set_content(content);

// Set min sidebar width
split_view->set_min_sidebar_width(200);
split_view->set_max_sidebar_width(400);

// Programmatic sidebar toggle
split_view->set_show_sidebar(true);  // or false
```

### AdwToolbarView

Container with top/bottom bars:

```cpp
auto toolbar_view = Adw::ToolbarView::create();

// Add content
toolbar_view->set_content(content_widget);

// Add top bar
auto header_bar = Adw::HeaderBar::create();
header_bar->set_title_widget(title_widget);
toolbar_view->add_top_bar(header_bar);

// Add bottom bar (for controls)
auto bottom_bar = Gtk::Box::create(Gtk::Orientation::HORIZONTAL, 6);
toolbar_view->add_bottom_bar(bottom_bar);
```

### AdwPreferencesWindow / PreferencesPage

```cpp
// Create preferences window
auto prefs_window = Adw::PreferencesWindow::create();
prefs_window->set_modal(true);
prefs_window->set_transient_for(*main_window);

// Create page
auto page = Adw::PreferencesPage::create();
page->set_title("General");

// Add group
auto group = Adw::PreferencesGroup::create();
group->set_title("Playback");

// Add action row
auto row = Adw::ActionRow::create();
row->set_title("Auto-play next video");
row->set_subtitle("Automatically play the next video in the queue");

// Add switch
auto toggle = Gtk::Switch::create();
row->set_activatable_widget(*toggle);
group->add(*row);

page->add(*group);
prefs_window->add(*page);
prefs_window->present();
```

### AdwEntryRow

Text input with validation:

```cpp
// Create entry row
auto entry = Adw::EntryRow::create();
entry->set_title("Video Title");
entry->set_placeholder_text("Enter video title...");

// Add prefix icon
entry->set_prefix_icon("video-x-generic");

// Connect to change signal
entry->signal_changed().connect([entry]() {
    Glib::ustring text = entry->get_text();
    // Validate and update
});

// Add to group
group->add(*entry);
```

### AdwClamp

Constrain content width:

```cpp
auto clamp = Adw::Clamp::create();
clamp->set_limit_mode(Adw::Clamp::LIMIT_MODE::MAXIMUM);
clamp->set_maximum_size(600);
clamp->set_tightening_threshold(400);
clamp->set_child(content_widget);
```

---

## ListView, GridView, and ColumnView

### GtkListView - Simple List

```cpp
#include <gtkmm.h>

class VideoListModel : public Glib::Object {
public:
    struct VideoItem {
        Glib::ustring id;
        Glib::ustring title;
        Glib::ustring thumbnail;
        int duration;
    };
    
    Glib::ustring get_id() const { return m_id; }
    Glib::ustring get_title() const { return m_title; }
    // ... getters
    
private:
    Glib::ustring m_id;
    Glib::ustring m_title;
    Glib::ustring m_thumbnail;
    int m_duration;
};

// For ListModel, inherit from Glib::Object and implement Gio::ListModel
class VideoListStore : public Glib::Object, public Gio::ListModel {
    // Implementation of ListModel interface
};

// Create list view
auto list_view = Gtk::ListView::create();
list_view->set_show_separators(true);
list_view->set_single_click_activate(true);

// Create selection model
auto selection_model = Gtk::SingleSelection::create(
    Glib::RefPtr<Gio::ListModel>(video_store)
);
list_view->set_model(selection_model);

// Create factory for items
auto factory = Gtk::SignalListItemFactory::create();
factory->signal_setup().connect([](const Glib::RefPtr<Gtk::ListItem>& item) {
    auto box = Gtk::Box::create(Gtk::Orientation::HORIZONTAL, 12);
    auto image = Gtk::Image::create();
    auto label = Gtk::Label::create();
    box->append(image);
    box->append(label);
    item->set_child(box);
});
factory->signal_bind().connect([](const Glib::RefPtr<Gtk::ListItem>& item) {
    auto video = item->get_item()->get_derived_object<VideoItem>();
    auto box = dynamic_cast<Gtk::Box*>(item->get_child());
    auto image = dynamic_cast<Gtk::Image*>(box->get_first_child());
    auto label = dynamic_cast<Gtk::Label*>(box->get_last_child());
    
    // Load thumbnail
    image->set_from_file(video.thumbnail);
    label->set_label(video.title);
});

list_view->set_factory(factory);

// Connect selection
selection_model->signal_selection_changed().connect(
    [](guint position, guint n_items) {
        // Handle selection
    }
);
```

### GtkGridView - Grid Layout

```cpp
// Create grid view for video thumbnails
auto grid_view = Gtk::GridView::create();

// Create list store for videos
auto store = Gio::ListStore<VideoItem>::create();

Glib::RefPtr<Gtk::NoSelection> selection_model = 
    Gtk::NoSelection::create(Glib::wrap(G_OBJECT(store->gobj())));
grid_view->set_model(selection_model);

// Set factory
auto factory = Gtk::SignalListItemFactory::create();
factory->signal_setup().connect([](const Glib::RefPtr<Gtk::ListItem>& item) {
    auto frame = Gtk::Frame::create();
    auto box = Gtk::Box::create(Gtk::Orientation::VERTICAL, 6);
    auto image = Gtk::Image::create();
    auto label = Gtk::Label::create();
    label->set_wrap();
    label->set_ellipsize(Pango::EllipsizeMode::END);
    box->append(image);
    box->append(label);
    frame->set_child(box);
    item->set_child(frame);
});
factory->signal_bind().connect([](const Glib::RefPtr<Gtk::ListItem>& item) {
    auto video = item->get_item()->get_derived_object<VideoItem>();
    auto frame = dynamic_cast<Gtk::Frame*>(item->get_child());
    auto box = dynamic_cast<Gtk::Box*>(frame->get_child());
    auto image = dynamic_cast<Gtk::Image*>(box->get_first_child());
    auto label = dynamic_cast<Gtk::Label*>(box->get_last_child());
    
    image->set_from_file(video.thumbnail);
    label->set_label(video.title);
});

grid_view->set_factory(factory);

// Set layout properties
grid_view->set_min_columns(2);
grid_view->set_max_columns(6);
grid_view->set_activate_on_single_click(true);
```

### GtkColumnView - Table with Columns

```cpp
// Create column view
auto column_view = Gtk::ColumnView::create();

// Create model (sorted list)
auto store = Gio::ListStore<VideoItem>::create();
auto sort_model = Gtk::SortListModel::create(
    Glib::RefPtr<Gio::ListModel>(store),
    Gtk::ColumnSorter::get_default()
);
auto selection_model = Gtk::SingleSelection::create(sort_model);
column_view->set_model(selection_model);

// Column 1: Thumbnail
{
    auto factory = Gtk::SignalListItemFactory::create();
    factory->signal_bind().connect([](const Glib::RefPtr<Gtk::ListItem>& item) {
        auto video = item->get_item()->get_derived_object<VideoItem>();
        auto image = dynamic_cast<Gtk::Image*>(item->get_child());
        image->set_from_file(video.thumbnail);
    });
    
    auto column = Gtk::ColumnViewColumn::create("Thumbnail", factory);
    column->set_fixed_width(120);
    column_view->append_column(column);
}

// Column 2: Title
{
    auto factory = Gtk::SignalListItemFactory::create();
    factory->signal_bind().connect([](const Glib::RefPtr<Gtk::ListItem>& item) {
        auto video = item->get_item()->get_derived_object<VideoItem>();
        auto label = dynamic_cast<Gtk::Label*>(item->get_child());
        label->set_label(video.title);
    });
    
    auto column = Gtk::ColumnViewColumn::create("Title", factory);
    column->set_expand(true);
    column_view->append_column(column);
}

// Column 3: Duration
{
    auto factory = Gtk::SignalListItemFactory::create();
    factory->signal_bind().connect([](const Glib::RefPtr<Gtk::ListItem>& item) {
        auto video = item->get_item()->get_derived_object<VideoItem>();
        auto label = dynamic_cast<Gtk::Label*>(item->get_child());
        label->set_label(format_duration(video.duration));
    });
    
    auto column = Gtk::ColumnViewColumn::create("Duration", factory);
    column->set_fixed_width(100);
    column_view->append_column(column);
}
```

---

## Dialogs and Notifications

### AdwMessageDialog

```cpp
// Create dialog
auto dialog = Adw::MessageDialog::create(
    *main_window,
    "Confirm Delete",
    "Are you sure you want to delete this video?"
);

// Add responses
dialog->add_response("cancel", "Cancel");
dialog->add_response("delete", "Delete");
dialog->set_response_appearance("delete", Adw::ResponseAppearance::DESTRUCTIVE);

// Set default and escape responses
dialog->set_default_response("cancel");
dialog->set_close_response("cancel");

// Connect response handler
dialog->signal_response().connect([dialog](const Glib::ustring& response) {
    if (response == "delete") {
        // Perform delete
        delete_video();
    }
    dialog->close();
});

dialog->present();
```

### AdwAlertDialog (Alternative)

```cpp
auto dialog = Adw::AlertDialog::create(
    "Save Changes?",
    "Open document contains unsaved changes. Changes which are not saved will be permanently lost."
);

dialog->add_cancel_button("Cancel");
dialog->add_button("Save", "save", Adw::ResponseAppearance::SUGGESTED);
dialog->add_button("Don't Save", "discard", Adw::ResponseAppearance::DESTRUCTIVE);

auto cancellable = Gio::Cancellable::create();
dialog->choose(cancellable, [](Glib::RefPtr<Adw::AlertDialog> dialog,
                               const Glib::RefPtr<Gio::AsyncResult>& result) {
    auto response = dialog->choose_finish(result);
    // Handle response
});
```

### AdwToast - Notifications

```cpp
// Create toast
auto toast = Adw::Toast::create("Video added to library");
toast->set_timeout(5);  // 5 seconds

// Connect button (optional)
toast->set_action_name("app.undo");
toast->set_action_target(glib::create_variant<int32_t>(video_id));

// Show in overlay (requires AdwToastOverlay)
m_toast_overlay->add_toast(toast);
```

### AdwToastOverlay

```xml
<!-- In UI file -->
<object class="AdwToastOverlay" id="toast_overlay">
    <property name="child">
        <!-- Your main content -->
    </property>
</object>
```

---

## CSS Styling

### Inline CSS Provider

```cpp
#include <gtkmm.h>

void apply_custom_style(Gtk::Widget* widget) {
    auto provider = Gtk::CssProvider::create();
    
    Glib::ustring css = R"(
        video-tile {
            background: @theme_bg_color;
            border-radius: 12px;
            padding: 12px;
        }
        
        video-tile:hover {
            background: @theme_hover_color;
        }
        
        video-tile thumbnail {
            border-radius: 8px;
        }
        
        video-tile title {
            font-weight: bold;
            color: @theme_fg_color;
        }
    )";
    
    provider->signal_parsing_error().connect(
        [](const Glib::RefPtr<Gtk::CssSection>& section,
           const Glib::Error& error) {
            g_warning("CSS parsing error: %s", error.what().c_str());
        }
    );
    
    provider->load_from_data(css);
    
    // Apply to widget and children
    widget->add_css_class("video-tile");
    widget->get_style_context()->add_provider(
        provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
}
```

### CSS from Resource

```cpp
void load_css_from_resource(const Glib::RefPtr<Gtk::CssProvider>& provider) {
    auto display = Gdk::Display::get_default();
    Gtk::CssProvider::load_from_resource(provider, 
        "/org/lucid/styles/main.css");
    
    Gtk::StyleContext::add_provider_for_display(
        display, provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
}
```

### Theme Colors (CSS Variables)

```css
/* Use Adwaita colors */
.my-widget {
    background: @theme_bg_color;
    color: @theme_fg_color;
    border-color: @borders;
}

/* Accent colors */
.action-button {
    background: @accent_color;
    color: @accent_fg_color;
}

/* Destructive actions */
.delete-button {
    background: @destructive_color;
    color: @destructive_fg_color;
}

/* Selection */
.selected-item {
    background: @selected_bg_color;
    color: @selected_fg_color;
}
```

---

## Signals and Callbacks

### Connecting Signals (C++17+)

```cpp
// Using signal_connect (member function)
m_button->signal_clicked().connect(
    sigc::mem_fun(*this, &MyClass::on_button_clicked)
);

// Lambda
m_button->signal_clicked().connect([this]() {
    do_something();
});

// With parameters
m_entry->signal_changed().connect([this](const Glib::ustring& text) {
    update_preview(text);
});

// Passing user data
m_button->signal_clicked().connect(
    [this](guint button, GdkEvent* event) {
        handle_click(button, event);
    },
    user_data
);
```

### Signal with Return Value

```cpp
// Confirm-close signal
m_window->signal_close_request().connect(
    [](GdkEvent* event) -> bool {
        // Return true to prevent close
        // Return false to allow close
        if (has_unsaved_changes()) {
            show_confirm_dialog();
            return true;  // Prevent close
        }
        return false;  // Allow close
    }
);
```

### Action Signals

```cpp
// Connect to Gio::Action signals
auto action = add_action("refresh", [this]() {
    refresh_library();
});

// Action with parameter
add_action_with_parameter("open-video", Glib::VariantType("s"),
    [this](const Glib::VariantBase& param) {
        auto id = Glib::VariantBase::cast_static<Glib::Variant<Glib::ustring>>(param);
        open_video(id.get());
    }
);

// Connect menu action
m_button->set_action_name("app.refresh");
```

---

## Glib::RefPtr and Object Management

### Reference Counting

```cpp
// Create new object (returns RefPtr)
auto window = Gtk::Builder::create_from_resource("/org/lucid/window.ui")
    ->get_widget<Gtk::Window>("main_window");

// RefPtr is std::shared_ptr-like
Glib::RefPtr<Gtk::ListStore> store = Gtk::ListStore::create();

// Store is automatically managed
store->clear();  // Still valid, RefPtr keeps alive

// Cast RefPtr types
Glib::RefPtr<Gtk::Widget> widget = window;
Glib::RefPtr<Gtk::Container> container = 
    std::dynamic_pointer_cast<Gtk::Container>(widget);

// Check type
if (std::dynamic_pointer_cast<Gtk::Button>(widget)) {
    // It's a button
}
```

### Wrapping C Objects

```cpp
// Wrap raw GObject* into C++ wrapper
GObject* raw_widget = gtk_button_new();
Gtk::Button* button = Gtk::Builder::create()->add_from_string(ui_xml)
    ->get_widget<Gtk::Button>("my_button");

// Using Glib::wrap()
auto wrapped = Glib::wrap(raw_button, true);  // take_copy = true
```

### Property Binding

```cpp
// Bind widget property to model property
m_view->property_selected_item().bind(
    m_model->property_selected_video()
);

// One-way binding
m_label->property_label().bind(
    Glib::PropertyProxy_ReadOnly<Glib::ustring>(m_model->gobj(), "title")
);

// With transformation
m_label->property_label().bind(
    m_model->property_value(),
    [](double value) -> Glib::ustring {
        return Glib::ustring::sprintf("%.2f", value);
    }
);
```

---

## Async Operations

### Gio::ThreadPool

```cpp
#include <glibmm.h>

class Worker {
public:
    void run_async() {
        auto pool = Gio::ThreadPool::create(4, false);
        
        pool->push([this]() {
            // Do heavy work
            auto result = do_heavy_work();
            
            // Schedule UI update on main thread
            Glib::MainContext::get_default()->signal_idle().connect(
                [this, result]() {
                    update_ui(result);
                    return false;  // Don't repeat
                }
            );
        });
    }
    
private:
    Result do_heavy_work();
    void update_ui(const Result& result);
};
```

### Gio::Task

```cpp
void transcribe_video_async(const Video& video,
                            GCancellable* cancellable,
                            Gio::AsyncReadyCallback callback,
                            gpointer user_data) {
    
    auto task = Gio::Task::create(
        nullptr,  // source_object
        cancellable,
        callback,
        user_data
    );
    
    task->set_priority(G_PRIORITY_DEFAULT);
    
    // Run in thread
    task->run_in_thread([](GObject* source, 
                          GObject* object,
                          GCancellable* cancellable,
                          gpointer task_data) {
        
        auto task = Glib::wrap(static_cast<GTask*>(task_data), true);
        
        try {
            TranscriptionResult result = transcribe(video);
            task->return_value(result);
        } catch (const std::exception& e) {
            task->return_error(std::current_exception());
        }
        
    }, task->gobj());
}

// Handle result
void on_transcribe_ready(GObject* source, 
                         GAsyncResult* result,
                         gpointer user_data) {
    
    auto task = Glib::wrap(static_cast<GTask*>(result), false);
    
    if (task->get_error()) {
        // Handle error
        g_warning("Transcription failed: %s", 
                  task->get_error()->what());
        return;
    }
    
    auto transcription = task->get_propagate_value<TranscriptionResult>();
    // Update UI with result
}
```

---

## GLib Utilities

### Environment Variables

```cpp
// Get environment variable
Glib::ustring value = Glib::getenv("HOME");

// Set environment variable
Glib::setenv("LUCID_HOME", "/path/to/data", true);

// Get application data directory
Glib::RefPtr<Gio::File> data_dir = Gio::File::create_for_path(
    Glib::get_user_data_dir() + "/lucid"
);

// Create directory if not exists
data_dir->make_directory_with_parents();
```

### File Operations

```cpp
// Read file
Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(path);
auto [stream, status] = file->read();
auto data = G_DECODE_STREAM(stream)->read_bytes(4096);

// Write file
auto output = file->replace();
output->write(content);
output->close();

// Watch for changes
auto monitor = file->monitor();
monitor->signal_changed().connect([](Glib::RefPtr<Gio::File> file,
                                      Glib::RefPtr<Gio::File> other,
                                      Gio::FileMonitorEvent event) {
    // Handle file change
});
```

### Timers

```cpp
// One-shot timeout
Glib::signal_timeout().connect_seconds_once([this]() {
    save_data();
    return false;  // Don't repeat
}, 5);  // 5 seconds

// Repeating timeout
Glib::signal_timeout().connect([this]() {
    check_for_updates();
    return true;  // Continue repeating
}, 60000);  // Every 60 seconds

// Idle callback (when main loop is idle)
Glib::signal_idle().connect([this]() {
    process_queue_item();
    return !m_queue.empty();  // Continue until queue empty
});
```

---

## Resource Files (.gresource)

### Create resource XML

```xml
<!-- resources/lucid.gresource.xml -->
<?xml version="1.0" encoding="UTF-8"?>
<gresources>
    <gresource prefix="/org/lucid">
        <file preprocess="to-pixdata">ui/main_window.ui</file>
        <file preprocess="to-pixdata">ui/sidebar.ui</file>
        <file preprocess="to-pixdata">ui/video_tile.ui</file>
        <file>styles/main.css</file>
        <file>styles/dark.css</file>
        <file>icons/lucid.svg</file>
    </gresource>
</gresources>
```

### Build with CMake

```cmake
# Compile .gresource.xml to C source
glib_compile_resources(lucid_gresource
    SOURCE resources/lucid.gresource.xml
    TARGET lucid-resources
)

# Add to executable
target_sources(lucid PRIVATE
    ${lucid_gresource}
)
```

---

## Complete Application Example

### Directory Structure

```
lucid/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── app.cpp
│   ├── app.hpp
│   └── main_window.cpp
├── resources/
│   ├── lucid.gresource.xml
│   └── ui/
│       └── main_window.ui
├── po/
│   └── lucid.pot
└── data/
    └── org.lucid.Lucid.desktop
```

### main.cpp

```cpp
#include "app.hpp"

int main(int argc, char** argv) {
    return LucidApp().run(argc, argv);
}
```

### app.hpp

```cpp
#pragma once

#include <gtkmm.h>
#include <adwaita.h>

class LucidApp : public Adw::Application {
public:
    LucidApp();
    
    void open_video(const Glib::ustring& path);
    
protected:
    void on_activate() override;
    void on_open(const std::vector<Glib::RefPtr<Gio::File>>& files,
                 const Glib::ustring& hint) override;
                 
private:
    Glib::RefPtr<Gtk::ApplicationWindow> m_window;
};
```

### app.cpp

```cpp
#include "app.hpp"
#include "main_window.hpp"

LucidApp::LucidApp()
    : Adw::Application("org.lucid.Lucid",
                       Gio::APPLICATION_NON_UNIQUE |
                       Gio::APPLICATION_HANDLES_OPEN) {
    
    // Register quit action with keyboard shortcut
    add_action("quit", [this]() { quit(); });
    set_accel_for_action("app.quit", "<Ctrl>Q");
}

void LucidApp::on_activate() {
    if (!m_window) {
        m_window = MainWindow::create();
        add_window(*m_window);
    }
    m_window->present();
}

void LucidApp::on_open(const std::vector<Glib::RefPtr<Gio::File>>& files,
                       const Glib::ustring& hint) {
    on_activate();
    
    for (const auto& file : files) {
        open_video(file->get_path());
    }
}

void LucidApp::open_video(const Glib::ustring& path) {
    auto window = std::dynamic_pointer_cast<MainWindow>(m_window);
    if (window) {
        window->load_video(path);
    }
}
```

### main_window.ui (XML template)

```xml
<interface>
    <template class="MainWindow" parent="AdwApplicationWindow">
        <property name="default-width">1200</property>
        <property name="default-height">800</property>
        
        <child>
            <object class="AdwToolbarView">
                <child type="top">
                    <object class="AdwHeaderBar">
                        <property name="show-start-title-buttons">true</property>
                        <property name="show-end-title-buttons">true</property>
                        <property name="title-widget">
                            <object class="AdwViewStack" id="view_stack">
                                <property name="hhomogeneous">false</property>
                            </object>
                        </property>
                    </object>
                </child>
                
                <child>
                    <object class="AdwNavigationSplitView" id="split_view">
                        <property name="sidebar-width-fraction">0.25</property>
                        
                        <property name="sidebar">
                            <object class="AdwNavigationPage">
                                <property name="title">Library</property>
                                <child>
                                    <object class="GtkScrolledWindow">
                                        <child>
                                            <object class="GtkListView" id="video_list">
                                                <property name="show-separator">true</property>
                                            </object>
                                        </child>
                                    </object>
                                </child>
                            </object>
                        </property>
                        
                        <property name="content">
                            <object class="AdwNavigationPage">
                                <property name="title">Video</property>
                                <child>
                                    <object class="GtkBox">
                                        <property name="orientation">vertical</property>
                                        <child>
                                            <object class="GtkLabel" id="video_title">
                                                <property name="label">Select a video</property>
                                            </object>
                                        </child>
                                    </object>
                                </child>
                            </object>
                        </property>
                    </object>
                </child>
            </object>
        </child>
    </template>
</interface>
```

---

## Useful Links

### Official Documentation

| Resource | URL |
|----------|-----|
| gtkmm Homepage | [gtkmm.org](https://gtkmm.org/) |
| gtkmm Documentation | [gnome.pages.gitlab.gnome.org/gtkmm-documentation](https://gnome.pages.gitlab.gnome.org/gtkmm-documentation/) |
| GTK4 Reference | [docs.gtk.org/gtk4](https://docs.gtk.org/gtk4/) |
| libadwaita Reference | [gnome.pages.gitlab.gnome.org/libadwaita](https://gnome.pages.gitlab.gnome.org/libadwaita/) |
| GTK4 Tutorial | [gtk4-tutorial.net](https://www.gtk4-tutorial.net/) |

### GitHub Repositories

| Repository | URL |
|------------|-----|
| gtkmm (C++ binding) | [github.com/GNOME/gtkmm](https://github.com/GNOME/gtkmm) |
| libadwaita | [github.com/GNOME/libadwaita](https://github.com/GNOME/libadwaita) |
| GTK (C library) | [github.com/GNOME/gtk](https://github.com/GNOME/gtk) |
| glibmm | [github.com/GNOME/glibmm](https://github.com/GNOME/glibmm) |
| gtkmm-documentation | [github.com/GNOME/gtkmm-documentation](https://github.com/GNOME/gtkmm-documentation) |

### Community Resources

| Resource | URL |
|----------|-----|
| GNOME Discourse | [discourse.gnome.org](https://discourse.gnome.org/) |
| GTK Forum | [discourse.gnome.org/c/gtk](https://discourse.gnome.org/c/gtk/12) |
| Reddit r/GTK | [reddit.com/r/GTK](https://www.reddit.com/r/GTK/) |
| GTK4 Tutorial (ToshioCP) | [github.com/ToshioCP/Gtk4-tutorial](https://github.com/ToshioCP/Gtk4-tutorial) |
| Python GTK4 Examples | [github.com/yucefsourani/python-gtk4-examples](https://github.com/yucefsourani/python-gtk4-examples) |

### Building and Packaging

| Resource | URL |
|----------|-----|
| GTK Build System | [docs.gtk.org/gtk4/install.html](https://docs.gtk.org/gtk4/install.html) |
| Meson GTK Tutorial | [mesonbuild.com/GuiTutorial.html](https://mesonbuild.com/GuiTutorial.html) |
| AppStream Specification | [freedesktop.org/software/appstream/docs](https://freedesktop.org/software/appstream/docs/) |

---

## Best Practices

### Memory Management

1. **Use RefPtr** - Never store raw `GtkWidget*` pointers
2. **Parent-child relationships** - Let GTK manage widget lifetime
3. **Use std::unique_ptr for custom widgets** - Override `dispose()` for cleanup

### UI Layout

1. **Use libadwaita patterns** - Follow AdwToolbarView, NavigationSplitView
2. **Use CSS for styling** - Avoid manual drawing
3. **Responsive design** - Use Clamp, AdwBreakpointBin

### Performance

1. **Use Gio::ListModel** - For virtualized lists (ListView, GridView, ColumnView)
2. **Reuse factories** - Avoid recreating ListItemFactory
3. **Batch operations** - Use Gio::ThreadPool for heavy work

### Internationalization

1. **Use gettext** - Wrap strings with `_()`
2. **Mark UI strings** - Use N_() for translatable strings in C++
3. **RTL support** - Test layouts with right-to-left text

---

## License

GTK4 and libadwaita are released under the LGPL v2.1+.
gtkmm is released under the LGPL v2.1+.

---

*Document generated for Lucid project*