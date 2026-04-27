// app_settings_tests.cpp - Unit tests for AppSettings class

#include <gtest/gtest.h>
#include <glibmm.h>
#include "lucid/settings/app_settings.hpp"
#include <fstream>
#include <filesystem>
#include <cstdio>

class AppSettingsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Store original config path
        m_original_config_path = Glib::build_filename(Glib::get_user_config_dir(), "lucid", "settings.json");
        
        // Backup existing config if any
        if (std::filesystem::exists(m_original_config_path)) {
            std::filesystem::copy_file(m_original_config_path, m_original_config_path + ".backup");
        }
    }
    
    void TearDown() override {
        // Restore backup
        if (std::filesystem::exists(m_original_config_path + ".backup")) {
            std::filesystem::rename(m_original_config_path + ".backup", m_original_config_path);
        } else {
            // Remove test config
            std::filesystem::remove(m_original_config_path);
        }
    }
    
    std::string m_original_config_path;
};

// Test singleton access
TEST_F(AppSettingsTest, SingletonAccess) {
    lucid::AppSettings& instance1 = lucid::AppSettings::instance();
    lucid::AppSettings& instance2 = lucid::AppSettings::instance();
    
    // Should be same instance
    EXPECT_EQ(&instance1, &instance2);
}

// Test load defaults when no config exists
TEST_F(AppSettingsTest, LoadDefaultsWhenNoConfig) {
    // Ensure no config file
    if (std::filesystem::exists(m_original_config_path)) {
        std::filesystem::remove(m_original_config_path);
    }
    
    // Create fresh instance (will re-initialize on next call)
    // Note: Singleton is initialized once, so we just verify defaults
    lucid::AppSettings& settings = lucid::AppSettings::instance();
    
    EXPECT_TRUE(settings.is_loaded());
    EXPECT_EQ(settings.get_whisper_model(), "whisper-tiny");
    EXPECT_EQ(settings.get_embedding_model(), "nomic-embed-text");
}

// Test getter default values
TEST_F(AppSettingsTest, DefaultValues) {
    lucid::AppSettings& settings = lucid::AppSettings::instance();
    
    EXPECT_EQ(settings.get_whisper_model(), "whisper-tiny");
    EXPECT_EQ(settings.get_embedding_model(), "nomic-embed-text");
    EXPECT_EQ(settings.get_llm_model(), "qwen2-vl-2b");
    EXPECT_TRUE(settings.get_use_gpu());
    EXPECT_EQ(settings.get_max_parallel_jobs(), 4);
    EXPECT_EQ(settings.get_embedding_context_size(), 2048);
    EXPECT_EQ(settings.get_whisper_context_size(), 16384);
    EXPECT_EQ(settings.get_thumbnail_size(), lucid::ThumbnailSize::MEDIUM);
    EXPECT_TRUE(settings.is_vision_scan_enabled());  // Always enabled
    EXPECT_EQ(settings.get_vision_scan_interval(), 2);  // Scan every 2nd thumbnail
    EXPECT_EQ(settings.get_vision_max_per_video(), 25);
}

// Test setters
TEST_F(AppSettingsTest, SetWhisperModel) {
    lucid::AppSettings& settings = lucid::AppSettings::instance();
    
    settings.set_whisper_model("whisper-small");
    EXPECT_EQ(settings.get_whisper_model(), "whisper-small");
}

TEST_F(AppSettingsTest, SetEmbeddingModel) {
    lucid::AppSettings& settings = lucid::AppSettings::instance();
    
    settings.set_embedding_model("other-embedding");
    EXPECT_EQ(settings.get_embedding_model(), "other-embedding");
}

TEST_F(AppSettingsTest, SetUseGpu) {
    lucid::AppSettings& settings = lucid::AppSettings::instance();
    
    settings.set_use_gpu(false);
    EXPECT_FALSE(settings.get_use_gpu());
    
    settings.set_use_gpu(true);
    EXPECT_TRUE(settings.get_use_gpu());
}

TEST_F(AppSettingsTest, SetMaxParallelJobs) {
    lucid::AppSettings& settings = lucid::AppSettings::instance();
    
    settings.set_max_parallel_jobs(8);
    EXPECT_EQ(settings.get_max_parallel_jobs(), 8);
    
    // Test clamping
    settings.set_max_parallel_jobs(100);
    EXPECT_EQ(settings.get_max_parallel_jobs(), 16);  // Clamped to max
    
    settings.set_max_parallel_jobs(0);
    EXPECT_EQ(settings.get_max_parallel_jobs(), 1);  // Clamped to min
}

// Note: set_enable_vision_scan() removed - vision scan is now always enabled

TEST_F(AppSettingsTest, SetVisionScanInterval) {
    lucid::AppSettings& settings = lucid::AppSettings::instance();
    
    settings.set_vision_scan_interval(5);
    EXPECT_EQ(settings.get_vision_scan_interval(), 5);
    
    // Test clamping
    settings.set_vision_scan_interval(100);
    EXPECT_EQ(settings.get_vision_scan_interval(), 20);  // Clamped to max
}

TEST_F(AppSettingsTest, SetVisionMaxPerVideo) {
    lucid::AppSettings& settings = lucid::AppSettings::instance();
    
    settings.set_vision_max_per_video(50);
    EXPECT_EQ(settings.get_vision_max_per_video(), 50);
    
    // Test clamping
    settings.set_vision_max_per_video(0);
    EXPECT_EQ(settings.get_vision_max_per_video(), 5);  // Clamped to min
}

TEST_F(AppSettingsTest, SetThumbnailSize) {
    lucid::AppSettings& settings = lucid::AppSettings::instance();
    
    settings.set_thumbnail_size(lucid::ThumbnailSize::SMALL);
    EXPECT_EQ(settings.get_thumbnail_size(), lucid::ThumbnailSize::SMALL);
    
    settings.set_thumbnail_size(lucid::ThumbnailSize::LARGE);
    EXPECT_EQ(settings.get_thumbnail_size(), lucid::ThumbnailSize::LARGE);
    
    settings.set_thumbnail_size_int(2);  // LARGE
    EXPECT_EQ(settings.get_thumbnail_size_int(), 2);
}

// Test watched folders
TEST_F(AppSettingsTest, WatchedFolders) {
    lucid::AppSettings& settings = lucid::AppSettings::instance();
    
    // Clear folders first
    settings.set_watched_folders({});
    EXPECT_TRUE(settings.get_watched_folders().empty());
    
    // Add folder
    settings.add_watched_folder("/home/user/videos");
    EXPECT_EQ(settings.get_watched_folders().size(), 1);
    EXPECT_EQ(settings.get_watched_folders()[0], "/home/user/videos");
    
    // Add another
    settings.add_watched_folder("/home/user/downloads");
    EXPECT_EQ(settings.get_watched_folders().size(), 2);
    
    // Remove folder
    settings.remove_watched_folder("/home/user/videos");
    EXPECT_EQ(settings.get_watched_folders().size(), 1);
    EXPECT_EQ(settings.get_watched_folders()[0], "/home/user/downloads");
    
    // Duplicate add should be ignored
    settings.add_watched_folder("/home/user/downloads");
    EXPECT_EQ(settings.get_watched_folders().size(), 1);
}

// Test signal callback
TEST_F(AppSettingsTest, SignalCallback) {
    lucid::AppSettings& settings = lucid::AppSettings::instance();
    
    bool callback_called = false;
    settings.on_changed(lucid::SignalType::GPU_CHANGED, [&callback_called]() {
        callback_called = true;
    });
    
    // Trigger GPU change
    settings.set_use_gpu(false);
    
    EXPECT_TRUE(callback_called);
}

// Test context sizes
TEST_F(AppSettingsTest, SetContextSizes) {
    lucid::AppSettings& settings = lucid::AppSettings::instance();
    
    settings.set_embedding_context_size(4096);
    EXPECT_EQ(settings.get_embedding_context_size(), 4096);
    
    settings.set_whisper_context_size(32768);
    EXPECT_EQ(settings.get_whisper_context_size(), 32768);
}
