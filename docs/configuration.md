# Lucid Configuration and Settings

## Overview

Lucid uses GSettings for user preferences and a custom config file for application-specific settings. This document describes the configuration system, default values, and storage locations.

**Repository**: [github.com/ggml-org/lucid](https://github.com/ggml-org/lucid)

---

## Data Directory Layout

```
~/.local/share/lucid/
├── config.ini              # Application config (rarely changed)
├── settings.ini            # User preferences (GSettings)
├── lucid.db                # SQLite database
├── lucid.db-wal           # SQLite WAL file
├── lucid.db-shm            # SQLite SHM file
├── models/                 # Downloaded GGUF models
│   ├── ggml-small.bin      # Whisper model
│   ├── nomic-embed-text-v1.5.Q4_K_M.gguf  # Embedding model
│   └── llama-3.2-1b-instruct-q4_k_m.gguf  # LLM model
├── thumbnails/             # Video thumbnails cache
│   ├── video-id-1-thumb-0.jpg
│   ├── video-id-1-thumb-1.jpg
│   └── ...
├── cache/                 # Temporary files
│   ├── audio/             # Extracted audio chunks
│   └── temp/              # FFmpeg temp files
└── logs/                  # Application logs
    ├── lucid.log
    └── inference.log
```

### XDG Compliance

```cpp
// Get paths following XDG spec
Glib::ustring get_config_dir() {
    const char* xdg = g_get_user_config_dir();
    return Glib::build_filename(xdg, "lucid");
}

Glib::ustring get_data_dir() {
    const char* xdg = g_get_user_data_dir();
    return Glib::build_filename(xdg, "lucid");
}

Glib::ustring get_cache_dir() {
    const char* xdg = g_get_user_cache_dir();
    return Glib::build_filename(xdg, "lucid");
}
```

---

## GSettings Schema

### Schema Definition

**File**: `data/org.lucid.Lucid.gschema.xml`

```xml
<?xml version="1.0" encoding="UTF-8"?>
<schemalist>
  <schema id="org.lucid.Lucid" path="/org/lucid/Lucid/">
    
    <!-- Video Library Settings -->
    <key name="watch-directories" type="as">
      <summary>Directories to watch for new videos</summary>
      <description>List of directory paths to monitor for new video files</description>
      <default>[]</default>
    </key>
    
    <key name="auto-transcribe" type="b">
      <summary>Automatically transcribe new videos</summary>
      <description>When a new video is added, automatically start transcription</description>
      <default>true</default>
    </key>
    
    <key name="transcribe-language" type="s">
      <summary>Language for transcription</summary>
      <description>Language code for Whisper transcription (empty for auto-detect)</description>
      <default>""</default>
    </key>
    
    <!-- Model Settings -->
    <key name="whisper-model" type="s">
      <summary>Active Whisper model</summary>
      <description>Filename of the Whisper model to use</description>
      <default>ggml-small.bin</default>
    </key>
    
    <key name="embedding-model" type="s">
      <summary>Active embedding model</summary>
      <description>Filename of the embedding model to use</description>
      <default>nomic-embed-text-v1.5.Q4_K_M.gguf</default>
    </key>
    
    <key name="use-gpu" type="b">
      <summary>Use GPU for inference</summary>
      <description>Use Vulkan/CUDA for Whisper and Llama inference</description>
      <default>true</default>
    </key>
    
    <key name="n-threads" type="i">
      <summary>Number of CPU threads</summary>
      <description>Number of threads for CPU inference</description>
      <default>4</default>
      <range min="1" max="64"/>
    </key>
    
    <!-- Playback Settings -->
    <key name="external-player" type="b">
      <summary>Use external player</summary>
      <description>Open videos in external player instead of embedded</description>
      <default>false</default>
    </key>
    
    <key name="external-player-command" type="s">
      <summary>External player command</summary>
      <description>Command template for external player (use {file} and {time})</description>
      <default>mpv --start={time} "{file}"</default>
    </key>
    
    <!-- UI Settings -->
    <key name="theme" type="s">
      <summary>Application theme</summary>
      <description>GTK theme to use (system, light, dark)</description>
      <default>system</default>
    </key>
    
    <key name="grid-columns" type="i">
      <summary>Number of columns in video grid</summary>
      <description>Number of columns in the video library grid view</description>
      <default>4</default>
      <range min="1" max="12"/>
    </key>
    
    <key name="show-previews" type="b">
      <summary>Show video previews on hover</summary>
      <description>Display thumbnail preview when hovering over a video</description>
      <default>true</default>
    </key>
    
    <!-- YouTube Settings -->
    <key name="youtube-subscription-interval" type="i">
      <summary>YouTube check interval in hours</summary>
      <description>How often to check for new videos from subscribed channels</description>
      <default>6</default>
      <range min="1" max="168"/>
    </key>
    
    <!-- Search Settings -->
    <key name="semantic-search-enabled" type="b">
      <summary>Enable semantic search</summary>
      <description>Use embeddings for search in addition to keyword search</description>
      <default>true</default>
    </key>
    
    <key name="max-search-results" type="i">
      <summary>Maximum search results</summary>
      <description>Maximum number of results to return from search</description>
      <default>50</default>
      <range min="10" max="500"/>
    </key>

  </schema>
</schemalist>
```

### C++ GSettings Wrapper

**File**: `src/lucid/config/settings.hpp`

```cpp
#pragma once

#include <gio/gio.h>
#include <glibmm.h>
#include <string>
#include <vector>
#include <optional>

namespace lucid {

class Settings {
public:
    static Settings& instance();
    
    // Initialize GSettings
    void initialize();
    
    // Video Library
    std::vector<std::string> get_watch_directories() const;
    void set_watch_directories(const std::vector<std::string>& dirs);
    void add_watch_directory(const std::string& dir);
    void remove_watch_directory(const std::string& dir);
    
    bool get_auto_transcribe() const;
    void set_auto_transcribe(bool value);
    
    std::string get_transcribe_language() const;
    void set_transcribe_language(const std::string& lang);
    
    // Models
    std::string get_whisper_model() const;
    void set_whisper_model(const std::string& model);
    
    std::string get_embedding_model() const;
    void set_embedding_model(const std::string& model);
    
    bool get_use_gpu() const;
    void set_use_gpu(bool value);
    
    int get_n_threads() const;
    void set_n_threads(int n);
    
    // Playback
    bool get_external_player() const;
    void set_external_player(bool value);
    
    std::string get_external_player_command() const;
    void set_external_player_command(const std::string& cmd);
    
    // UI
    std::string get_theme() const;
    void set_theme(const std::string& theme);
    
    int get_grid_columns() const;
    void set_grid_columns(int columns);
    
    bool get_show_previews() const;
    void set_show_previews(bool value);
    
    // YouTube
    int get_youtube_interval() const;
    void set_youtube_interval(int hours);
    
    // Search
    bool get_semantic_search_enabled() const;
    void set_semantic_search_enabled(bool value);
    
    int get_max_search_results() const;
    void set_max_search_results(int max);
    
    // Connection for UI updates
    sigc::signal<void()> signal_changed;

private:
    Settings() = default;
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;
    
    GSettings* m_settings = nullptr;
    
    void on_settings_changed(const gchar* key);
};

} // namespace lucid
```

**File**: `src/lucid/config/settings.cpp`

```cpp
#include "settings.hpp"
#include <glibmm.h>

namespace lucid {

Settings& Settings::instance() {
    static Settings instance;
    return instance;
}

void Settings::initialize() {
    // Register schema
    // Schema should be installed to /usr/share/glib-2.0/schemas/
    // or bundled with the application
    
    m_settings = g_settings_new("org.lucid.Lucid");
    
    // Connect to change signals
    g_signal_connect_swapped(m_settings, "changed",
                              G_CALLBACK(+[](Settings* s, const gchar* key) {
                                  s->on_settings_changed(key);
                              }), this);
}

void Settings::on_settings_changed(const gchar* key) {
    signal_changed().emit();
}

std::vector<std::string> Settings::get_watch_directories() const {
    auto* list = g_settings_get_strv(m_settings, "watch-directories");
    std::vector<std::string> result;
    
    for (int i = 0; list[i] != nullptr; ++i) {
        result.push_back(list[i]);
    }
    g_strfreev(list);
    
    return result;
}

void Settings::set_watch_directories(const std::vector<std::string>& dirs) {
    std::vector<const char*> argv;
    for (const auto& d : dirs) {
        argv.push_back(d.c_str());
    }
    argv.push_back(nullptr);
    
    g_settings_set_strv(m_settings, "watch-directories", argv.data());
}

bool Settings::get_auto_transcribe() const {
    return g_settings_get_boolean(m_settings, "auto-transcribe");
}

void Settings::set_auto_transcribe(bool value) {
    g_settings_set_boolean(m_settings, "auto-transcribe", value);
}

std::string Settings::get_whisper_model() const {
    return g_settings_get_string(m_settings, "whisper-model");
}

void Settings::set_whisper_model(const std::string& model) {
    g_settings_set_string(m_settings, "whisper-model", model.c_str());
}

// ... similar implementations for all other settings

} // namespace lucid
```

---

## Application Config File

For settings not in GSettings (paths, GPU memory limits, etc.):

**File**: `~/.local/share/lucid/config.ini`

```ini
[paths]
models_dir = ~/.local/share/lucid/models
thumbnail_dir = ~/.local/share/lucid/thumbnails
cache_dir = ~/.local/share/lucid/cache

[gpu]
max_vram_mb = 4096
offload_threshold_mb = 512

[inference]
whisper_n_threads = 4
llama_n_threads = 4
llama_n_ctx = 2048

[transcription]
chunk_duration_sec = 30
overlap_sec = 2

[search]
embedding_dimension = 768
top_k = 20

[logging]
level = info
file = ~/.local/share/lucid/logs/lucid.log
```

### Config File Parser

**File**: `src/lucid/config/config_file.hpp`

```cpp
#pragma once

#include <string>
#include <map>
#include <optional>

namespace lucid {

class ConfigFile {
public:
    ConfigFile(const std::string& path);
    
    // Get value as string
    std::optional<std::string> get(const std::string& section,
                                    const std::string& key) const;
    
    // Get value with default
    std::string get_or(const std::string& section,
                       const std::string& key,
                       const std::string& default_value) const;
    
    // Get value as int
    int get_int(const std::string& section,
                const std::string& key,
                int default_value = 0) const;
    
    // Get value as bool
    bool get_bool(const std::string& section,
                  const std::string& key,
                  bool default_value = false) const;
    
    // Set value (in memory)
    void set(const std::string& section,
             const std::string& key,
             const std::string& value);
    
    // Save to file
    void save();
    
    // Load from file
    void load();

private:
    std::string m_path;
    std::map<std::string, std::map<std::string, std::string>> m_data;
};

} // namespace lucid
```

---

## Default Paths

```cpp
// src/lucid/config/paths.hpp
namespace lucid::paths {

Glib::ustring get_models_dir() {
    return Glib::build_filename(Glib::get_user_data_dir(), "lucid", "models");
}

Glib::ustring get_thumbnails_dir() {
    return Glib::build_filename(Glib::get_user_data_dir(), "lucid", "thumbnails");
}

Glib::ustring get_cache_dir() {
    return Glib::build_filename(Glib::get_user_cache_dir(), "lucid");
}

Glib::ustring get_logs_dir() {
    return Glib::build_filename(Glib::get_user_data_dir(), "lucid", "logs");
}

Glib::ustring get_database_path() {
    return Glib::build_filename(Glib::get_user_data_dir(), "lucid", "lucid.db");
}

Glib::ustring get_config_path() {
    return Glib::build_filename(Glib::get_user_config_dir(), "lucid", "config.ini");
}

// Ensure directories exist
void ensure_directories() {
    std::vector<Glib::ustring> dirs = {
        get_models_dir(),
        get_thumbnails_dir(),
        get_cache_dir(),
        get_logs_dir()
    };
    
    for (const auto& dir : dirs) {
        try {
            Glib::Dir::make_dir(dir);
        } catch (const Glib::Error& e) {
            g_warning("Failed to create directory %s: %s", 
                      dir.c_str(), e.what().c_str());
        }
    }
}

} // namespace lucid::paths
```

---

## First-Run Setup

```cpp
// src/lucid/config/first_run.hpp
void perform_first_run_setup() {
    // Create directories
    paths::ensure_directories();
    
    // Initialize settings with defaults
    Settings::instance().initialize();
    
    // Create default config file
    ConfigFile config(paths::get_config_path());
    if (!std::filesystem::exists(paths::get_config_path())) {
        config.save();  // Creates with defaults
    }
    
    // Initialize database
    Database db(paths::get_database_path());
    db.initialize_schema();
    
    // Open Model Hub page to prompt user to download models
    // (This would be triggered by the GUI)
}
```

---

## Settings Migration

```cpp
// Handle settings version upgrades
void migrate_settings_if_needed() {
    auto version = Settings::instance().get<int>("schema-version", 0);
    
    if (version < 1) {
        // Migration from v0 to v1
        // E.g., rename old settings, set new defaults
        Settings::instance().set("schema-version", 1);
    }
    
    if (version < 2) {
        // Future migrations
        Settings::instance().set("schema-version", 2);
    }
}
```

---

## Reset to Defaults

```cpp
void reset_to_defaults() {
    // Clear GSettings to defaults
    auto* settings = g_settings_new("org.lucid.Lucid");
    g_settings_reset(settings);
    g_object_unref(settings);
    
    // Reset config file
    std::filesystem::remove(paths::get_config_path());
    ConfigFile config(paths::get_config_path());
    config.save();
    
    // Re-initialize
    perform_first_run_setup();
}
```

---

## Schema Installation

### Meson (Alternative to CMake)

```meson
install_data(
    'data/org.lucid.Lucid.gschema.xml',
    install_dir: 'glib-2.0/schemas',
    rename: 'org.lucid.Lucid.gschema.xml'
)

# Compile schema
meson.add_install_script('glib-compile-schemas', meson.current_source_dir())
```

### CMake

```cmake
# Install schema
install(FILES data/org.lucid.Lucid.gschema.xml
        DESTINATION share/glib-2.0/schemas)

# Compile on install
install(CODE "
    execute_process(COMMAND glib-compile-schemas ${CMAKE_INSTALL_PREFIX}/share/glib-2.0/schemas)
")
```

---

## Dependencies

```bash
# Arch Linux
sudo pacman -S glib2 libglibmm
```

```cmake
# CMakeLists.txt
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GLIBMM REQUIRED glibmm-2.68)
```

---

## API Documentation Links

| Resource | URL |
|----------|-----|
| GSettings | [docs.gtk.org/gio/class.Settings.html](https://docs.gtk.org/gio/class.Settings.html) |
| GLib Keyfile | [docs.gtk.org/glib/glib-Key-value-file-parser.html](https://docs.gtk.org/glib/glib-Key-value-file-parser.html) |
| GSettings Schema | [developer.gnome.org/gio/stable/gsettings.html](https://developer.gnome.org/gio/stable/gsettings.html) |

---

*Document generated for Lucid project*