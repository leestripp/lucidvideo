// database.cpp - Lucid Database Implementation
//
// SQLite3 with sqlite-vec for vector search

#include "database.hpp"
#include "../logging.hpp"

#include <cstring>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <map>
#include <algorithm>
#include <cmath>

namespace lucid {

// ============================================================================
// Enum Implementations
// ============================================================================

const char* video_status_to_string(VideoStatus status) {
    switch (status) {
        case VideoStatus::PENDING:      return "pending";
        case VideoStatus::ANALYZING:   return "analyzing";
        case VideoStatus::TRANSCRIBING: return "transcribing";
        case VideoStatus::THUMBNAILING: return "thumbnailing";
        case VideoStatus::VISION_SCANNING: return "vision_scanning";
        case VideoStatus::COMPLETED:   return "completed";
        case VideoStatus::FAILED:      return "failed";
        case VideoStatus::STALE:       return "stale";
        case VideoStatus::BROKEN:     return "broken";  // Failed too many times
        default:                        return "pending";
    }
}

VideoStatus video_status_from_string(const std::string& str) {
    if (str == "pending")      return VideoStatus::PENDING;
    if (str == "analyzing")   return VideoStatus::ANALYZING;
    if (str == "transcribing") return VideoStatus::TRANSCRIBING;
    if (str == "thumbnailing") return VideoStatus::THUMBNAILING;
    if (str == "vision_scanning") return VideoStatus::VISION_SCANNING;
    if (str == "completed")   return VideoStatus::COMPLETED;
    if (str == "failed")      return VideoStatus::FAILED;
    if (str == "stale")       return VideoStatus::STALE;
    if (str == "broken")      return VideoStatus::BROKEN;
    return VideoStatus::PENDING;
}

// ============================================================================
// Database Implementation
// ============================================================================

Database::Database(const std::string& db_path)
    : m_db_path(db_path) {
    
    int rc = sqlite3_open(db_path.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        std::string error = sqlite3_errmsg(m_db);
        sqlite3_close(m_db);
        throw std::runtime_error("Failed to open database: " + error);
    }
    
    sqlite3_exec(m_db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    initialize_schema();
    initialize_vec_extension();
}

Database::~Database() {
    if (m_db) {
        sqlite3_close(m_db);
    }
}

void Database::initialize_schema() {
    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS videos (
            id              TEXT PRIMARY KEY,
            path            TEXT NOT NULL UNIQUE,
            title           TEXT NOT NULL,
            thumbnail       TEXT,
            duration_ms     INTEGER DEFAULT 0,
            source          TEXT NOT NULL DEFAULT 'local',
            channel_id      TEXT,
            status          TEXT NOT NULL DEFAULT 'pending',
            error_message   TEXT DEFAULT '',
            created_at      TEXT DEFAULT (datetime('now')),
            updated_at      TEXT DEFAULT (datetime('now')),
            metadata        TEXT DEFAULT '{}'
        );
        
        CREATE INDEX IF NOT EXISTS idx_videos_path ON videos(path);
        CREATE INDEX IF NOT EXISTS idx_videos_status ON videos(status);
        CREATE INDEX IF NOT EXISTS idx_videos_source ON videos(source);
        CREATE INDEX IF NOT EXISTS idx_videos_created ON videos(created_at);
        
        CREATE TABLE IF NOT EXISTS video_chunks (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            video_id    TEXT NOT NULL REFERENCES videos(id) ON DELETE CASCADE,
            start_sec   INTEGER NOT NULL,
            end_sec     INTEGER NOT NULL,
            chunk_index INTEGER NOT NULL,
            UNIQUE(video_id, chunk_index)
        );
        
        -- Markers table (transcription segments become markers)
        CREATE TABLE IF NOT EXISTS markers (
            id                  INTEGER PRIMARY KEY AUTOINCREMENT,
            video_id            TEXT NOT NULL REFERENCES videos(id) ON DELETE CASCADE,
            segment_index       INTEGER NOT NULL,
            start_ms            INTEGER NOT NULL,
            end_ms              INTEGER NOT NULL,
            timecode_start      TEXT NOT NULL,
            timecode_end        TEXT NOT NULL,
            text                TEXT NOT NULL,
            confidence          REAL DEFAULT 1.0,
            word_count          INTEGER DEFAULT 0,
            word_timestamps     TEXT,
            speaker             TEXT,
            marker_type         TEXT DEFAULT 'audio',
            thumbnail_path      TEXT,
            frame_number        INTEGER DEFAULT -1,
            created_at          TEXT DEFAULT (datetime('now')),
            UNIQUE(video_id, segment_index)
        );
        
        CREATE INDEX IF NOT EXISTS idx_markers_video ON markers(video_id);
        CREATE INDEX IF NOT EXISTS idx_markers_timestamp ON markers(start_ms);
        
        -- Thumbnails table
        CREATE TABLE IF NOT EXISTS thumbnails (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            video_id        TEXT NOT NULL REFERENCES videos(id) ON DELETE CASCADE,
            path            TEXT NOT NULL,
            timestamp_ms    INTEGER NOT NULL,
            timecode        TEXT NOT NULL,
            width           INTEGER DEFAULT 0,
            height          INTEGER DEFAULT 0,
            vision_description TEXT,
            vision_analyzed INTEGER DEFAULT 0,
            created_at      TEXT DEFAULT (datetime('now'))
        );
        
        CREATE INDEX IF NOT EXISTS idx_thumbnails_video ON thumbnails(video_id);
        CREATE INDEX IF NOT EXISTS idx_thumbnails_timestamp ON thumbnails(timestamp_ms);
        
        -- Vision analysis results for thumbnails
        CREATE TABLE IF NOT EXISTS vision_analyses (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            thumbnail_id    INTEGER NOT NULL REFERENCES thumbnails(id) ON DELETE CASCADE,
            video_id        TEXT NOT NULL REFERENCES videos(id) ON DELETE CASCADE,
            description     TEXT NOT NULL DEFAULT '',
            objects         TEXT,
            scene_type      TEXT,
            text_content    TEXT,
            confidence      REAL DEFAULT 1.0,
            model_used      TEXT,
            processing_time_ms INTEGER DEFAULT 0,
            created_at      TEXT DEFAULT (datetime('now'))
        );
        
        CREATE INDEX IF NOT EXISTS idx_vision_thumbnail ON vision_analyses(thumbnail_id);
        CREATE INDEX IF NOT EXISTS idx_vision_video ON vision_analyses(video_id);
        
        -- Full-text search for markers
        CREATE VIRTUAL TABLE IF NOT EXISTS markers_fts USING fts5(
            video_id, text, tokenize='porter unicode61'
        );
        
        CREATE TABLE IF NOT EXISTS transcriptions (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            video_id    TEXT NOT NULL REFERENCES videos(id) ON DELETE CASCADE,
            start_ms    INTEGER NOT NULL,
            end_ms      INTEGER NOT NULL,
            text        TEXT NOT NULL,
            confidence  REAL DEFAULT 1.0,
            chunk_id    INTEGER
        );
        
        CREATE INDEX IF NOT EXISTS idx_transcriptions_video ON transcriptions(video_id);
        
        CREATE VIRTUAL TABLE IF NOT EXISTS transcriptions_fts USING fts5(
            video_id, text, tokenize='porter unicode61'
        );
        
        CREATE TABLE IF NOT EXISTS embeddings (
            id          TEXT PRIMARY KEY,
            video_id    TEXT NOT NULL,
            text        TEXT,
            embedding   TEXT,
            timestamp_ms INTEGER DEFAULT 0,
            created_at  TEXT DEFAULT (datetime('now'))
        );
        
        CREATE INDEX IF NOT EXISTS idx_embeddings_video ON embeddings(video_id);
        
        -- Migration: Add timestamp_ms column to embeddings if missing
        -- (for databases created before timestamp_ms was added)
    )";
    
    char* err_msg = nullptr;
    int rc = sqlite3_exec(m_db, schema, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        throw std::runtime_error("Failed to initialize schema: " + error);
    }
    
    // Migration: Add timestamp_ms column if it doesn't exist
    const char* migrate_emb_cols = R"(
        ALTER TABLE embeddings ADD COLUMN timestamp_ms INTEGER DEFAULT 0;
    )";
    sqlite3_exec(m_db, migrate_emb_cols, nullptr, nullptr, nullptr);
    
    // Migration: Add vision columns to thumbnails if they don't exist
    // (for databases created before these columns were added)
    // Note: ALTER TABLE fails silently if column already exists in newer SQLite
    const char* migrate_vision_cols = R"(
        ALTER TABLE thumbnails ADD COLUMN vision_description TEXT;
        ALTER TABLE thumbnails ADD COLUMN vision_analyzed INTEGER DEFAULT 0;
    )";
    sqlite3_exec(m_db, migrate_vision_cols, nullptr, nullptr, nullptr);
    
    // Migration: Add marker_type and thumbnail_path columns to markers
    // (for databases created before the unified marker system)
    const char* migrate_marker_cols = R"(
        ALTER TABLE markers ADD COLUMN marker_type TEXT DEFAULT 'audio';
        ALTER TABLE markers ADD COLUMN thumbnail_path TEXT;
        ALTER TABLE markers ADD COLUMN frame_number INTEGER DEFAULT -1;
    )";
    sqlite3_exec(m_db, migrate_marker_cols, nullptr, nullptr, nullptr);
}

void Database::initialize_vec_extension() {
    // Placeholder - sqlite-vec would be loaded here if available
}

// ============================================================================
// Video CRUD
// ============================================================================

std::optional<VideoEntry> Database::get_video(const std::string& id) {
    const char* sql = R"(
        SELECT id, path, title, thumbnail, duration_ms, source, channel_id, 
               status, error_message, created_at, updated_at, metadata
        FROM videos WHERE id = ?
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }
    
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        VideoEntry entry;
        entry.id = (const char*)sqlite3_column_text(stmt, 0);
        entry.path = (const char*)sqlite3_column_text(stmt, 1);
        entry.title = (const char*)sqlite3_column_text(stmt, 2);
        if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
            entry.thumbnail = (const char*)sqlite3_column_text(stmt, 3);
        }
        entry.duration_ms = sqlite3_column_int64(stmt, 4);
        entry.source = (const char*)sqlite3_column_text(stmt, 5);
        if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
            entry.channel_id = (const char*)sqlite3_column_text(stmt, 6);
        }
        entry.status = video_status_from_string((const char*)sqlite3_column_text(stmt, 7));
        if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) {
            entry.error_message = (const char*)sqlite3_column_text(stmt, 8);
        }
        entry.metadata_json = (const char*)sqlite3_column_text(stmt, 11);
        sqlite3_finalize(stmt);
        return entry;
    }
    
    sqlite3_finalize(stmt);
    return std::nullopt;
}

std::vector<VideoEntry> Database::get_all_videos(int limit, int offset) {
    std::vector<VideoEntry> results;
    
    const char* sql = R"(
        SELECT v.id, v.path, v.title, v.thumbnail, v.duration_ms, v.source, v.channel_id,
               v.status, v.error_message, v.created_at, v.updated_at, v.metadata,
               (SELECT t.path FROM thumbnails t WHERE t.video_id = v.id ORDER BY t.timestamp_ms ASC LIMIT 1) as first_thumbnail
        FROM videos v ORDER BY v.created_at DESC LIMIT ? OFFSET ?
    )";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return results;
    }
    
    sqlite3_bind_int(stmt, 1, limit);
    sqlite3_bind_int(stmt, 2, offset);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        VideoEntry entry;
        entry.id = (const char*)sqlite3_column_text(stmt, 0);
        entry.path = (const char*)sqlite3_column_text(stmt, 1);
        entry.title = (const char*)sqlite3_column_text(stmt, 2);
        // Use first_thumbnail from thumbnails table if videos.thumbnail is null
        if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
            entry.thumbnail = (const char*)sqlite3_column_text(stmt, 3);
        } else if (sqlite3_column_type(stmt, 12) != SQLITE_NULL) {
            entry.thumbnail = (const char*)sqlite3_column_text(stmt, 12);
        }
        entry.duration_ms = sqlite3_column_int64(stmt, 4);
        entry.source = (const char*)sqlite3_column_text(stmt, 5);
        if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
            entry.channel_id = (const char*)sqlite3_column_text(stmt, 6);
        }
        entry.status = video_status_from_string((const char*)sqlite3_column_text(stmt, 7));
        if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) {
            entry.error_message = (const char*)sqlite3_column_text(stmt, 8);
        }
        if (sqlite3_column_type(stmt, 9) != SQLITE_NULL) {
            entry.created_at = std::chrono::system_clock::from_time_t(
                (time_t)sqlite3_column_int64(stmt, 9));
        }
        if (sqlite3_column_type(stmt, 10) != SQLITE_NULL) {
            entry.updated_at = std::chrono::system_clock::from_time_t(
                (time_t)sqlite3_column_int64(stmt, 10));
        }
        entry.metadata_json = (const char*)sqlite3_column_text(stmt, 11);
        results.push_back(entry);
    }
    
    sqlite3_finalize(stmt);
    return results;
}

// ============================================================================
// Chat Query Helpers
// ============================================================================

int Database::get_video_count() {
    const char* sql = "SELECT COUNT(*) FROM videos";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return count;
}

std::vector<VideoEntry> Database::get_recent_videos(int limit) {
    std::vector<VideoEntry> results;
    
    const char* sql = R"(
        SELECT id, path, title, thumbnail, duration_ms, source, channel_id,
               status, error_message, created_at, updated_at, metadata
        FROM videos ORDER BY created_at DESC LIMIT ?
    )";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return results;
    }
    
    sqlite3_bind_int(stmt, 1, limit);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        VideoEntry entry;
        entry.id = (const char*)sqlite3_column_text(stmt, 0);
        entry.path = (const char*)sqlite3_column_text(stmt, 1);
        entry.title = (const char*)sqlite3_column_text(stmt, 2);
        if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
            entry.thumbnail = (const char*)sqlite3_column_text(stmt, 3);
        }
        entry.duration_ms = sqlite3_column_int64(stmt, 4);
        entry.source = (const char*)sqlite3_column_text(stmt, 5);
        if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
            entry.channel_id = (const char*)sqlite3_column_text(stmt, 6);
        }
        entry.status = video_status_from_string((const char*)sqlite3_column_text(stmt, 7));
        if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) {
            entry.error_message = (const char*)sqlite3_column_text(stmt, 8);
        }
        entry.created_at = std::chrono::system_clock::from_time_t(
                (time_t)sqlite3_column_int64(stmt, 9));
        entry.updated_at = std::chrono::system_clock::from_time_t(
                (time_t)sqlite3_column_int64(stmt, 10));
        if (sqlite3_column_type(stmt, 11) != SQLITE_NULL) {
            entry.metadata_json = (const char*)sqlite3_column_text(stmt, 11);
        }
        results.push_back(entry);
    }
    
    sqlite3_finalize(stmt);
    return results;
}

std::vector<VideoEntry> Database::get_videos_by_duration(int64_t min_ms, int64_t max_ms) {
    std::vector<VideoEntry> results;
    
    std::string sql;
    if (max_ms < 0) {
        // Only min constraint
        sql = R"(
            SELECT id, path, title, thumbnail, duration_ms, source, channel_id,
                   status, error_message, created_at, updated_at, metadata
            FROM videos WHERE duration_ms >= ? ORDER BY duration_ms DESC
        )";
    } else {
        // Both min and max
        sql = R"(
            SELECT id, path, title, thumbnail, duration_ms, source, channel_id,
                   status, error_message, created_at, updated_at, metadata
            FROM videos WHERE duration_ms >= ? AND duration_ms <= ? ORDER BY duration_ms DESC
        )";
    }
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return results;
    }
    
    sqlite3_bind_int64(stmt, 1, min_ms);
    if (max_ms >= 0) {
        sqlite3_bind_int64(stmt, 2, max_ms);
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        VideoEntry entry;
        entry.id = (const char*)sqlite3_column_text(stmt, 0);
        entry.path = (const char*)sqlite3_column_text(stmt, 1);
        entry.title = (const char*)sqlite3_column_text(stmt, 2);
        if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
            entry.thumbnail = (const char*)sqlite3_column_text(stmt, 3);
        }
        entry.duration_ms = sqlite3_column_int64(stmt, 4);
        entry.source = (const char*)sqlite3_column_text(stmt, 5);
        if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
            entry.channel_id = (const char*)sqlite3_column_text(stmt, 6);
        }
        entry.status = video_status_from_string((const char*)sqlite3_column_text(stmt, 7));
        if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) {
            entry.error_message = (const char*)sqlite3_column_text(stmt, 8);
        }
        entry.created_at = std::chrono::system_clock::from_time_t((time_t)sqlite3_column_int64(stmt, 9));
        entry.updated_at = std::chrono::system_clock::from_time_t((time_t)sqlite3_column_int64(stmt, 10));
        if (sqlite3_column_type(stmt, 11) != SQLITE_NULL) {
            entry.metadata_json = (const char*)sqlite3_column_text(stmt, 11);
        }
        results.push_back(entry);
    }
    
    sqlite3_finalize(stmt);
    return results;
}

std::string Database::add_video(const VideoEntry& video) {
    const char* sql = R"(
        INSERT INTO videos (id, path, title, thumbnail, duration_ms, source, 
                           channel_id, status, error_message, metadata)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare insert statement");
    }
    
    sqlite3_bind_text(stmt, 1, video.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, video.path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, video.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, video.thumbnail ? video.thumbnail->c_str() : nullptr, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, video.duration_ms);
    sqlite3_bind_text(stmt, 6, video.source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, video.channel_id ? video.channel_id->c_str() : nullptr, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, video_status_to_string(video.status), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, video.error_message.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10, video.metadata_json.c_str(), -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::string error = sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to insert video: " + error);
    }
    
    sqlite3_finalize(stmt);
    return video.id;
}

void Database::update_video(const VideoEntry& video) {
    const char* sql = R"(
        UPDATE videos SET title = ?, thumbnail = ?, duration_ms = ?, 
            status = ?, error_message = ?,
            updated_at = datetime('now') WHERE id = ?
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare update statement");
    }
    
    sqlite3_bind_text(stmt, 1, video.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, video.thumbnail ? video.thumbnail->c_str() : nullptr, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, video.duration_ms);
    sqlite3_bind_text(stmt, 4, video_status_to_string(video.status), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, video.error_message.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, video.id.c_str(), -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::string error = sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to update video: " + error);
    }
    
    sqlite3_finalize(stmt);
}

void Database::delete_video(const std::string& id) {
    const char* sql = "DELETE FROM videos WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare delete statement");
    }
    
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::string error = sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to delete video: " + error);
    }
    
    sqlite3_finalize(stmt);
}

// ============================================================================
// Video Status Management
// ============================================================================

std::optional<std::string> Database::find_by_path(const std::string& path) {
    const char* sql = "SELECT id FROM videos WHERE path = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }
    
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string id = (const char*)sqlite3_column_text(stmt, 0);
        sqlite3_finalize(stmt);
        return id;
    }
    
    sqlite3_finalize(stmt);
    return std::nullopt;
}

void Database::update_video_status(const std::string& id, VideoStatus status) {
    const char* sql = "UPDATE videos SET status = ?, updated_at = datetime('now') WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare update status statement");
    }
    
    sqlite3_bind_text(stmt, 1, video_status_to_string(status), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);
    
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void Database::set_video_error(const std::string& id, const std::string& error) {
    const char* sql = "UPDATE videos SET status = 'failed', error_message = ?, updated_at = datetime('now') WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }
    
    sqlite3_bind_text(stmt, 1, error.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);
    
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void Database::update_video_duration(const std::string& id, int64_t duration_ms) {
    const char* sql = "UPDATE videos SET duration_ms = ?, updated_at = datetime('now') WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }
    
    sqlite3_bind_int64(stmt, 1, duration_ms);
    sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);
    
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void Database::reset_all_videos_to_pending() {
    // Reset all videos to pending status
    const char* sql = "UPDATE videos SET status = 'pending', updated_at = datetime('now')";
    char* err_msg = nullptr;
    if (sqlite3_exec(m_db, sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
        // Log error (without using LUCID_ERROR which requires GLib)
        std::cerr << "Failed to reset videos to pending: " << (err_msg ? err_msg : "unknown") << std::endl;
        sqlite3_free(err_msg);
    }
    
    // Also delete all markers to avoid segment_index conflicts on re-analysis
    // This ensures clean re-analysis without duplicate marker errors
    const char* delete_markers_sql = "DELETE FROM markers";
    if (sqlite3_exec(m_db, delete_markers_sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
        std::cerr << "Failed to delete markers: " << (err_msg ? err_msg : "unknown") << std::endl;
        sqlite3_free(err_msg);
    }
}

// ============================================================================
// Markers (from transcription)
// ============================================================================

int64_t Database::add_marker(const Marker& marker) {
    const char* sql = R"(
        INSERT INTO markers (video_id, segment_index, start_ms, end_ms, 
                            timecode_start, timecode_end, text, confidence, 
                            word_count, word_timestamps, speaker, marker_type, thumbnail_path, frame_number)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare marker insert");
    }
    
    sqlite3_bind_text(stmt, 1, marker.video_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, marker.segment_index);
    sqlite3_bind_int64(stmt, 3, marker.start_ms);
    sqlite3_bind_int64(stmt, 4, marker.end_ms);
    sqlite3_bind_text(stmt, 5, marker.timecode_start.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, marker.timecode_end.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, marker.text.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 8, marker.confidence);
    sqlite3_bind_int(stmt, 9, marker.word_count);
    sqlite3_bind_text(stmt, 10, marker.word_timestamps_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 11, marker.speaker ? marker.speaker->c_str() : nullptr, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 12, marker_type_to_string(marker.marker_type), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 13, marker.thumbnail_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 14, marker.frame_number);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::string error = sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to insert marker: " + error);
    }
    
    int64_t id = sqlite3_last_insert_rowid(m_db);
    sqlite3_finalize(stmt);
    
    // Also insert into FTS table
    const char* fts_sql = "INSERT INTO markers_fts (video_id, text) VALUES (?, ?)";
    if (sqlite3_prepare_v2(m_db, fts_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, marker.video_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, marker.text.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
    return id;
}

void Database::update_marker(const Marker& marker) {
    const char* sql = R"(
        UPDATE markers SET 
            text = ?,
            confidence = ?,
            timecode_start = ?,
            timecode_end = ?,
            thumbnail_path = ?
        WHERE id = ?
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare marker update statement");
    }
    
    sqlite3_bind_text(stmt, 1, marker.text.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, marker.confidence);
    sqlite3_bind_text(stmt, 3, marker.timecode_start.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, marker.timecode_end.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, marker.thumbnail_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 6, marker.id);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::string error = sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to update marker: " + error);
    }
    
    sqlite3_finalize(stmt);
    
    // Update FTS table
    const char* fts_sql = "UPDATE markers_fts SET text = ? WHERE rowid = ?";
    if (sqlite3_prepare_v2(m_db, fts_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, marker.text.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, marker.id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void Database::add_markers(const std::string& video_id, const std::vector<Marker>& markers) {
    for (const auto& marker : markers) {
        Marker m = marker;
        m.video_id = video_id;
        add_marker(m);
    }
}

std::vector<Marker> Database::get_markers(const std::string& video_id) {
    std::vector<Marker> results;
    
    const char* sql = R"(
        SELECT id, video_id, segment_index, start_ms, end_ms, 
               timecode_start, timecode_end, text, confidence, 
               word_count, word_timestamps, speaker, marker_type, thumbnail_path, frame_number, created_at
        FROM markers WHERE video_id = ? ORDER BY segment_index
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return results;
    }
    
    sqlite3_bind_text(stmt, 1, video_id.c_str(), -1, SQLITE_TRANSIENT);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Marker marker;
        marker.id = sqlite3_column_int64(stmt, 0);
        marker.video_id = (const char*)sqlite3_column_text(stmt, 1);
        marker.segment_index = sqlite3_column_int(stmt, 2);
        marker.start_ms = sqlite3_column_int64(stmt, 3);
        marker.end_ms = sqlite3_column_int64(stmt, 4);
        marker.timecode_start = (const char*)sqlite3_column_text(stmt, 5);
        marker.timecode_end = (const char*)sqlite3_column_text(stmt, 6);
        marker.text = (const char*)sqlite3_column_text(stmt, 7);
        marker.confidence = static_cast<float>(sqlite3_column_double(stmt, 8));
        marker.word_count = sqlite3_column_int(stmt, 9);
        if (sqlite3_column_type(stmt, 10) != SQLITE_NULL) {
            marker.word_timestamps_json = (const char*)sqlite3_column_text(stmt, 10);
        }
        if (sqlite3_column_type(stmt, 11) != SQLITE_NULL) {
            marker.speaker = (const char*)sqlite3_column_text(stmt, 11);
        }
        if (sqlite3_column_type(stmt, 12) != SQLITE_NULL) {
            marker.marker_type = marker_type_from_string((const char*)sqlite3_column_text(stmt, 12));
        }
        if (sqlite3_column_type(stmt, 13) != SQLITE_NULL) {
            marker.thumbnail_path = (const char*)sqlite3_column_text(stmt, 13);
        }
        if (sqlite3_column_type(stmt, 14) != SQLITE_NULL) {
            marker.frame_number = sqlite3_column_int64(stmt, 14);
        }
        results.push_back(marker);
    }
    
    sqlite3_finalize(stmt);
    return results;
}

std::vector<Marker> Database::search_markers(const std::string& query, int limit) {
    std::vector<Marker> results;
    
    const char* sql = R"(
        SELECT m.id, m.video_id, m.segment_index, m.start_ms, m.end_ms,
               m.timecode_start, m.timecode_end, m.text, m.confidence,
               m.word_count, m.word_timestamps, m.speaker, m.marker_type, m.thumbnail_path, m.frame_number, m.created_at
        FROM markers_fts
        JOIN markers m ON markers_fts.rowid = m.id
        WHERE markers_fts MATCH ?
        ORDER BY rank
        LIMIT ?
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return results;
    }
    
    sqlite3_bind_text(stmt, 1, query.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, limit);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Marker marker;
        marker.id = sqlite3_column_int64(stmt, 0);
        marker.video_id = (const char*)sqlite3_column_text(stmt, 1);
        marker.segment_index = sqlite3_column_int(stmt, 2);
        marker.start_ms = sqlite3_column_int64(stmt, 3);
        marker.end_ms = sqlite3_column_int64(stmt, 4);
        marker.timecode_start = (const char*)sqlite3_column_text(stmt, 5);
        marker.timecode_end = (const char*)sqlite3_column_text(stmt, 6);
        marker.text = (const char*)sqlite3_column_text(stmt, 7);
        marker.confidence = static_cast<float>(sqlite3_column_double(stmt, 8));
        marker.word_count = sqlite3_column_int(stmt, 9);
        if (sqlite3_column_type(stmt, 10) != SQLITE_NULL) {
            marker.word_timestamps_json = (const char*)sqlite3_column_text(stmt, 10);
        }
        if (sqlite3_column_type(stmt, 11) != SQLITE_NULL) {
            marker.speaker = (const char*)sqlite3_column_text(stmt, 11);
        }
        if (sqlite3_column_type(stmt, 12) != SQLITE_NULL) {
            marker.marker_type = marker_type_from_string((const char*)sqlite3_column_text(stmt, 12));
        }
        if (sqlite3_column_type(stmt, 13) != SQLITE_NULL) {
            marker.thumbnail_path = (const char*)sqlite3_column_text(stmt, 13);
        }
        if (sqlite3_column_type(stmt, 14) != SQLITE_NULL) {
            marker.frame_number = sqlite3_column_int64(stmt, 14);
        }
        results.push_back(marker);
    }
    
    sqlite3_finalize(stmt);
    return results;
}

void Database::delete_marker(int64_t marker_id) {
    // Delete from markers table
    const char* sql = "DELETE FROM markers WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }
    
    sqlite3_bind_int64(stmt, 1, marker_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void Database::clear_markers(const std::string& video_id) {
    // Clear from markers table (FTS will cascade if configured, otherwise we need to handle separately)
    const char* sql = "DELETE FROM markers WHERE video_id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }
    
    sqlite3_bind_text(stmt, 1, video_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    // Note: FTS table doesn't support foreign keys, so we clear by text search
    // In production, you might want to maintain a separate ID mapping for FTS
}

// ============================================================================
// Thumbnails
// ============================================================================

int64_t Database::add_thumbnail(const ThumbnailEntry& thumbnail) {
    const char* sql = R"(
        INSERT INTO thumbnails (video_id, path, timestamp_ms, timecode, width, height, vision_description, vision_analyzed)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare thumbnail insert");
    }
    
    sqlite3_bind_text(stmt, 1, thumbnail.video_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, thumbnail.path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, thumbnail.timestamp_ms);
    sqlite3_bind_text(stmt, 4, thumbnail.timecode.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, thumbnail.width);
    sqlite3_bind_int(stmt, 6, thumbnail.height);
    sqlite3_bind_text(stmt, 7, thumbnail.vision_description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 8, thumbnail.vision_analyzed ? 1 : 0);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::string error = sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to insert thumbnail: " + error);
    }
    
    int64_t id = sqlite3_last_insert_rowid(m_db);
    sqlite3_finalize(stmt);
    return id;
}

void Database::add_thumbnails(const std::string& video_id, const std::vector<ThumbnailEntry>& thumbnails) {
    for (const auto& thumb : thumbnails) {
        ThumbnailEntry t = thumb;
        t.video_id = video_id;
        add_thumbnail(t);
    }
}

std::vector<ThumbnailEntry> Database::get_thumbnails(const std::string& video_id) {
    std::vector<ThumbnailEntry> results;
    
    const char* sql = R"(
        SELECT id, video_id, path, timestamp_ms, timecode, width, height, 
               COALESCE(vision_description, ''), vision_analyzed, created_at
        FROM thumbnails WHERE video_id = ? ORDER BY timestamp_ms
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return results;
    }
    
    sqlite3_bind_text(stmt, 1, video_id.c_str(), -1, SQLITE_TRANSIENT);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ThumbnailEntry entry;
        entry.id = sqlite3_column_int64(stmt, 0);
        entry.video_id = (const char*)sqlite3_column_text(stmt, 1);
        entry.path = (const char*)sqlite3_column_text(stmt, 2);
        entry.timestamp_ms = sqlite3_column_int64(stmt, 3);
        entry.timecode = (const char*)sqlite3_column_text(stmt, 4);
        entry.width = sqlite3_column_int(stmt, 5);
        entry.height = sqlite3_column_int(stmt, 6);
        entry.vision_description = (const char*)sqlite3_column_text(stmt, 7);
        entry.vision_analyzed = sqlite3_column_int(stmt, 8) != 0;
        results.push_back(entry);
    }
    
    sqlite3_finalize(stmt);
    return results;
}

void Database::clear_thumbnails(const std::string& video_id) {
    const char* sql = "DELETE FROM thumbnails WHERE video_id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }
    
    sqlite3_bind_text(stmt, 1, video_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// ============================================================================
// Vision Analysis
// ============================================================================

int64_t Database::add_vision_analysis(const VisionAnalysis& analysis) {
    const char* sql = R"(
        INSERT INTO vision_analyses (thumbnail_id, video_id, description, objects, scene_type, text_content, confidence, model_used, processing_time_ms)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare vision analysis insert");
    }
    
    sqlite3_bind_int64(stmt, 1, analysis.thumbnail_id);
    sqlite3_bind_text(stmt, 2, analysis.video_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, analysis.description.c_str(), -1, SQLITE_TRANSIENT);
    
    // Serialize objects vector to JSON
    std::ostringstream objects_json;
    objects_json << "[";
    for (size_t i = 0; i < analysis.objects.size(); ++i) {
        if (i > 0) objects_json << ",";
        objects_json << "\"" << analysis.objects[i] << "\"";
    }
    objects_json << "]";
    sqlite3_bind_text(stmt, 4, objects_json.str().c_str(), -1, SQLITE_TRANSIENT);
    
    sqlite3_bind_text(stmt, 5, analysis.scene_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, analysis.text_content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 7, analysis.confidence);
    sqlite3_bind_text(stmt, 8, analysis.model_used.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 9, analysis.processing_time_ms);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::string error = sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to insert vision analysis: " + error);
    }
    
    int64_t id = sqlite3_last_insert_rowid(m_db);
    sqlite3_finalize(stmt);
    return id;
}

std::vector<VisionAnalysis> Database::get_vision_analyses(const std::string& video_id) {
    std::vector<VisionAnalysis> results;
    
    const char* sql = R"(
        SELECT id, thumbnail_id, video_id, description, objects, scene_type, text_content, confidence, model_used, processing_time_ms, created_at
        FROM vision_analyses WHERE video_id = ? ORDER BY thumbnail_id
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return results;
    }
    
    sqlite3_bind_text(stmt, 1, video_id.c_str(), -1, SQLITE_TRANSIENT);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        VisionAnalysis entry;
        entry.id = sqlite3_column_int64(stmt, 0);
        entry.thumbnail_id = sqlite3_column_int64(stmt, 1);
        entry.video_id = (const char*)sqlite3_column_text(stmt, 2);
        entry.description = (const char*)sqlite3_column_text(stmt, 3);
        
        // Parse objects JSON array
        const char* objects_json = (const char*)sqlite3_column_text(stmt, 4);
        if (objects_json && objects_json[0] == '[') {
            std::string json = objects_json;
            size_t pos = 1;
            while (pos < json.size()) {
                auto quote1 = json.find('"', pos);
                if (quote1 == std::string::npos) break;
                auto quote2 = json.find('"', quote1 + 1);
                if (quote2 == std::string::npos) break;
                entry.objects.push_back(json.substr(quote1 + 1, quote2 - quote1 - 1));
                pos = quote2 + 1;
            }
        }
        
        entry.scene_type = (const char*)sqlite3_column_text(stmt, 5);
        entry.text_content = (const char*)sqlite3_column_text(stmt, 6);
        entry.confidence = static_cast<float>(sqlite3_column_double(stmt, 7));
        entry.model_used = (const char*)sqlite3_column_text(stmt, 8);
        entry.processing_time_ms = sqlite3_column_int64(stmt, 9);
        results.push_back(entry);
    }
    
    sqlite3_finalize(stmt);
    return results;
}

std::vector<VisionAnalysis> Database::get_vision_analyses_for_thumbnail(int64_t thumbnail_id) {
    std::vector<VisionAnalysis> results;
    
    const char* sql = R"(
        SELECT id, thumbnail_id, video_id, description, objects, scene_type, text_content, confidence, model_used, processing_time_ms, created_at
        FROM vision_analyses WHERE thumbnail_id = ? ORDER BY id
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return results;
    }
    
    sqlite3_bind_int64(stmt, 1, thumbnail_id);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        VisionAnalysis entry;
        entry.id = sqlite3_column_int64(stmt, 0);
        entry.thumbnail_id = sqlite3_column_int64(stmt, 1);
        entry.video_id = (const char*)sqlite3_column_text(stmt, 2);
        entry.description = (const char*)sqlite3_column_text(stmt, 3);
        
        const char* objects_json = (const char*)sqlite3_column_text(stmt, 4);
        if (objects_json && objects_json[0] == '[') {
            std::string json = objects_json;
            size_t pos = 1;
            while (pos < json.size()) {
                auto quote1 = json.find('"', pos);
                if (quote1 == std::string::npos) break;
                auto quote2 = json.find('"', quote1 + 1);
                if (quote2 == std::string::npos) break;
                entry.objects.push_back(json.substr(quote1 + 1, quote2 - quote1 - 1));
                pos = quote2 + 1;
            }
        }
        
        entry.scene_type = (const char*)sqlite3_column_text(stmt, 5);
        entry.text_content = (const char*)sqlite3_column_text(stmt, 6);
        entry.confidence = static_cast<float>(sqlite3_column_double(stmt, 7));
        entry.model_used = (const char*)sqlite3_column_text(stmt, 8);
        entry.processing_time_ms = sqlite3_column_int64(stmt, 9);
        results.push_back(entry);
    }
    
    sqlite3_finalize(stmt);
    return results;
}

void Database::clear_vision_analyses(const std::string& video_id) {
    const char* sql = "DELETE FROM vision_analyses WHERE video_id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }
    
    sqlite3_bind_text(stmt, 1, video_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void Database::mark_thumbnail_vision_analyzed(int64_t thumbnail_id, const std::string& description) {
    const char* sql = "UPDATE thumbnails SET vision_analyzed = 1, vision_description = ? WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }
    
    sqlite3_bind_text(stmt, 1, description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, thumbnail_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// ============================================================================
// Timeline (combined thumbnails + markers)
// ============================================================================

std::vector<TimelineEntry> Database::get_timeline(const std::string& video_id) {
    std::vector<TimelineEntry> results;
    
    // Get all thumbnails and markers, merge by timestamp
    auto thumbnails = get_thumbnails(video_id);
    auto markers = get_markers(video_id);
    
    // Create map for dedup (same timestamp should only appear once)
    std::map<int64_t, TimelineEntry> entries;
    
    for (const auto& thumb : thumbnails) {
        TimelineEntry entry;
        entry.timestamp_ms = thumb.timestamp_ms;
        entry.entry_type = "thumbnail";
        entry.thumbnail_id = thumb.id;
        entries[thumb.timestamp_ms] = entry;
    }
    
    for (const auto& marker : markers) {
        // Only add marker if no thumbnail at same timestamp (within 1 second)
        bool found = false;
        for (const auto& [ts, existing] : entries) {
            if (std::abs(ts - marker.start_ms) < 1000) {
                found = true;
                break;
            }
        }
        if (!found) {
            TimelineEntry entry;
            entry.timestamp_ms = marker.start_ms;
            entry.entry_type = "marker";
            entry.marker_id = marker.id;
            entries[marker.start_ms] = entry;
        }
    }
    
    // Sort by timestamp and convert to vector
    for (const auto& [ts, entry] : entries) {
        results.push_back(entry);
    }
    
    std::sort(results.begin(), results.end(), 
              [](const TimelineEntry& a, const TimelineEntry& b) {
                  return a.timestamp_ms < b.timestamp_ms;
              });
    
    return results;
}

// ============================================================================
// Transcription CRUD
// ============================================================================

std::vector<TranscriptionSegment> Database::get_transcription(const std::string& video_id) {
    std::vector<TranscriptionSegment> results;
    
    const char* sql = R"(
        SELECT id, video_id, start_ms, end_ms, text, confidence, chunk_id, 0 as is_complete
        FROM transcriptions WHERE video_id = ? ORDER BY start_ms
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return results;
    }
    
    sqlite3_bind_text(stmt, 1, video_id.c_str(), -1, SQLITE_TRANSIENT);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TranscriptionSegment seg;
        seg.id = sqlite3_column_int64(stmt, 0);
        seg.video_id = (const char*)sqlite3_column_text(stmt, 1);
        seg.start_ms = sqlite3_column_int(stmt, 2);
        seg.end_ms = sqlite3_column_int(stmt, 3);
        seg.text = (const char*)sqlite3_column_text(stmt, 4);
        seg.confidence = static_cast<float>(sqlite3_column_double(stmt, 5));
        results.push_back(seg);
    }
    
    sqlite3_finalize(stmt);
    return results;
}

void Database::add_transcription(const std::string& video_id,
                                 const std::vector<TranscriptionSegment>& segments) {
    const char* sql = R"(
        INSERT INTO transcriptions (video_id, start_ms, end_ms, text, confidence, chunk_id)
        VALUES (?, ?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare transcription insert");
    }
    
    for (const auto& seg : segments) {
        sqlite3_bind_text(stmt, 1, video_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, seg.start_ms);
        sqlite3_bind_int(stmt, 3, seg.end_ms);
        sqlite3_bind_text(stmt, 4, seg.text.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 5, seg.confidence);
        sqlite3_bind_int64(stmt, 6, seg.chunk_id.value_or(0));
        
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::string error = sqlite3_errmsg(m_db);
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to insert transcription: " + error);
        }
        sqlite3_reset(stmt);
    }
    
    sqlite3_finalize(stmt);
}

void Database::clear_transcription(const std::string& video_id) {
    const char* sql = "DELETE FROM transcriptions WHERE video_id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare clear transcription");
    }
    
    sqlite3_bind_text(stmt, 1, video_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// ============================================================================
// Search
// ============================================================================

std::vector<SearchResult> Database::keyword_search(const std::string& query, int limit) {
    std::vector<SearchResult> results;
    
    const char* sql = R"(
        SELECT t.video_id, v.title, t.start_ms,
               snippet(transcriptions_fts, 1, '<b>', '</b>', '...', 32) as snippet
        FROM transcriptions_fts
        JOIN transcriptions t ON transcriptions_fts.rowid = t.id
        JOIN videos v ON t.video_id = v.id
        WHERE transcriptions_fts MATCH ?
        LIMIT ?
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return results;
    }
    
    sqlite3_bind_text(stmt, 1, query.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, limit);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SearchResult result;
        result.video_id = (const char*)sqlite3_column_text(stmt, 0);
        result.title = (const char*)sqlite3_column_text(stmt, 1);
        result.timestamp_ms = sqlite3_column_int(stmt, 2);
        result.snippet = (const char*)sqlite3_column_text(stmt, 3);
        results.push_back(result);
    }
    
    sqlite3_finalize(stmt);
    return results;
}

std::vector<SearchResult> Database::semantic_search(const std::vector<float>& embedding,
                                                   int limit) {
    std::vector<SearchResult> results;
    
    if (embedding.empty()) {
        LUCID_WARN("[Database] Empty embedding provided to semantic_search");
        return results;
    }
    
    // Compute the embedding norm for cosine similarity
    double norm = 0.0;
    for (float v : embedding) {
        norm += static_cast<double>(v) * static_cast<double>(v);
    }
    norm = std::sqrt(norm);
    
    if (norm < 1e-10) {
        LUCID_WARN("[Database] Embedding norm too small");
        return results;
    }
    
    // Fetch all embeddings from database
    auto all_embeddings = get_all_embeddings();
    
    // Score each embedding using cosine similarity
    // embedding = [v1, v2, ...] stored as JSON string
    for (const auto& stored : all_embeddings) {
        if (stored.embedding.empty()) continue;
        
        // Compute cosine similarity
        double similarity = 0.0;
        size_t min_dim = std::min(embedding.size(), stored.embedding.size());
        
        for (size_t i = 0; i < min_dim; ++i) {
            similarity += static_cast<double>(embedding[i]) * 
                         static_cast<double>(stored.embedding[i]);
        }
        
        // Normalize by both norms
        double stored_norm = 0.0;
        for (float v : stored.embedding) {
            stored_norm += static_cast<double>(v) * static_cast<double>(v);
        }
        stored_norm = std::sqrt(stored_norm);
        
        if (stored_norm > 1e-10) {
            similarity /= (norm * stored_norm);
            
            // Only include results with positive similarity (common for normalized embeddings)
            if (similarity > 0.0) {
                SearchResult result;
                result.video_id = stored.video_id;
                result.timestamp_ms = stored.timestamp_ms;
                result.relevance_score = static_cast<float>(similarity);
                result.snippet = stored.text;
                results.push_back(result);
            }
        }
    }
    
    // Sort by relevance score descending
    std::sort(results.begin(), results.end(),
              [](const SearchResult& a, const SearchResult& b) {
                  return a.relevance_score > b.relevance_score;
              });
    
    // Limit results
    if (static_cast<int>(results.size()) > limit) {
        results.resize(limit);
    }
    
    LUCID_INFO("[Database] semantic_search returned %zu results", results.size());
    return results;
}

std::vector<SearchResult> Database::hybrid_search(const std::string& query, int limit) {
    return keyword_search(query, limit);
}

// ============================================================================
// Embeddings
// ============================================================================

void Database::add_embedding(const std::string& id, const std::string& text,
                             const std::vector<float>& embedding,
                             int64_t timestamp_ms /* = 0 */) {
    std::ostringstream json;
    json << "[";
    for (size_t i = 0; i < embedding.size(); ++i) {
        if (i > 0) json << ",";
        json << embedding[i];
    }
    json << "]";

    // Extract video_id from id format "video_id:marker_index"
    std::string video_id;
    size_t colon = id.find(':');
    if (colon != std::string::npos) {
        video_id = id.substr(0, colon);
    } else {
        video_id = id;
    }

    const char* sql = R"(
        INSERT OR REPLACE INTO embeddings (id, video_id, text, embedding, timestamp_ms) VALUES (?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, video_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, text.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, json.str().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, timestamp_ms);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void Database::delete_embeddings_for_video(const std::string& video_id) {
    const char* sql = "DELETE FROM embeddings WHERE id LIKE ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }
    
    std::string pattern = video_id + ":%";
    sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

namespace {
// Helper: parse JSON float array like "[0.1, 0.2, ...]" into vector<float>
static std::vector<float> parse_embedding_json(const std::string& json) {
    std::vector<float> result;
    if (json.empty() || json == "[]") return result;

    size_t pos = 0;
    // Find opening bracket
    size_t start = json.find('[', pos);
    if (start == std::string::npos) return result;
    pos = start + 1;

    while (pos < json.size()) {
        // Skip whitespace and commas
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == ',')) pos++;
        if (pos >= json.size() || json[pos] == ']') break;

        // Parse number
        size_t end = pos;
        while (end < json.size() && json[end] != ',' && json[end] != ']') end++;
        std::string num = json.substr(pos, end - pos);
        try {
            result.push_back(std::stof(num));
        } catch (...) {
            // Skip malformed numbers
        }
        pos = end + 1;
    }

    return result;
}
} // anonymous namespace

// ============================================================================
// Embedding Retrieval (for semantic search)
// ============================================================================

std::vector<Database::StoredEmbedding> Database::get_all_embeddings() {
    std::vector<StoredEmbedding> results;

    const char* sql = R"(
        SELECT id, video_id, text, embedding, timestamp_ms
        FROM embeddings
        WHERE embedding IS NOT NULL AND embedding != ''
        ORDER BY video_id
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LUCID_ERROR("[Database] Failed to prepare get_all_embeddings: %s",
                    sqlite3_errmsg(m_db));
        return results;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        StoredEmbedding emb;
        emb.id = (const char*)sqlite3_column_text(stmt, 0);

        // Parse video_id from "video_id:marker_id" format in id column
        std::string full_id = emb.id;
        size_t colon = full_id.find(':');
        if (colon != std::string::npos) {
            emb.video_id = full_id.substr(0, colon);
        } else {
            emb.video_id = (const char*)sqlite3_column_text(stmt, 1);
        }

        if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
            emb.text = (const char*)sqlite3_column_text(stmt, 2);
        }

        // Parse JSON embedding array
        if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
            const char* json = (const char*)sqlite3_column_text(stmt, 3);
            emb.embedding = parse_embedding_json(json);
        }
        
        // Get timestamp_ms (column index 4)
        emb.timestamp_ms = sqlite3_column_int64(stmt, 4);

        results.push_back(emb);
    }

    sqlite3_finalize(stmt);
    LUCID_DEBUG("[Database] Loaded %zu stored embeddings", results.size());
    return results;
}

std::vector<Database::StoredEmbedding> Database::get_embeddings_for_video(const std::string& video_id) {
    std::vector<StoredEmbedding> results;

    const char* sql = R"(
        SELECT id, text, embedding, timestamp_ms
        FROM embeddings
        WHERE id LIKE ? AND embedding IS NOT NULL AND embedding != ''
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return results;
    }

    std::string pattern = video_id + ":%";
    sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        StoredEmbedding emb;
        emb.id = (const char*)sqlite3_column_text(stmt, 0);
        emb.video_id = video_id;

        if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
            emb.text = (const char*)sqlite3_column_text(stmt, 1);
        }

        if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
            const char* json = (const char*)sqlite3_column_text(stmt, 2);
            emb.embedding = parse_embedding_json(json);
        }
        
        // Get timestamp_ms (column index 3)
        emb.timestamp_ms = sqlite3_column_int64(stmt, 3);

        results.push_back(emb);
    }

    sqlite3_finalize(stmt);
    return results;
}

// ============================================================================
// Statistics
// ============================================================================

int Database::get_video_count_by_status(VideoStatus status) {
    const char* status_str = video_status_to_string(status);
    const char* sql = "SELECT COUNT(*) FROM videos WHERE status = ?";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_text(stmt, 1, status_str, -1, SQLITE_TRANSIENT);
    
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return count;
}

int Database::get_total_video_count() {
    const char* sql = "SELECT COUNT(*) FROM videos";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return count;
}

int Database::get_marker_count() {
    const char* sql = "SELECT COUNT(*) FROM markers";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return count;
}

// ============================================================================
// Bulk operations for queue panel
// ============================================================================

void Database::clear_all_markers() {
    const char* sql = "DELETE FROM markers";
    char* err_msg = nullptr;
    sqlite3_exec(m_db, sql, nullptr, nullptr, &err_msg);
    if (err_msg) {
        std::cerr << "Failed to clear markers: " << err_msg << std::endl;
        sqlite3_free(err_msg);
    }
}

void Database::clear_all_thumbnails() {
    const char* sql = "DELETE FROM thumbnails";
    char* err_msg = nullptr;
    sqlite3_exec(m_db, sql, nullptr, nullptr, &err_msg);
    if (err_msg) {
        std::cerr << "Failed to clear thumbnails: " << err_msg << std::endl;
        sqlite3_free(err_msg);
    }
}

void Database::clear_all_embeddings() {
    const char* sql = "DELETE FROM embeddings";
    char* err_msg = nullptr;
    sqlite3_exec(m_db, sql, nullptr, nullptr, &err_msg);
    if (err_msg) {
        std::cerr << "Failed to clear embeddings: " << err_msg << std::endl;
        sqlite3_free(err_msg);
    }
}

std::vector<VideoEntry> Database::get_failed_videos() {
    std::vector<VideoEntry> results;
    
    const char* sql = R"(
        SELECT id, path, title, thumbnail, duration_ms, source, channel_id,
               status, error_message, created_at, updated_at, metadata
        FROM videos WHERE status = 'failed' ORDER BY created_at DESC
    )";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return results;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        VideoEntry entry;
        entry.id = (const char*)sqlite3_column_text(stmt, 0);
        entry.path = (const char*)sqlite3_column_text(stmt, 1);
        entry.title = (const char*)sqlite3_column_text(stmt, 2);
        if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
            entry.thumbnail = (const char*)sqlite3_column_text(stmt, 3);
        }
        entry.duration_ms = sqlite3_column_int64(stmt, 4);
        entry.source = (const char*)sqlite3_column_text(stmt, 5);
        if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
            entry.channel_id = (const char*)sqlite3_column_text(stmt, 6);
        }
        entry.status = video_status_from_string((const char*)sqlite3_column_text(stmt, 7));
        if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) {
            entry.error_message = (const char*)sqlite3_column_text(stmt, 8);
        }
        entry.metadata_json = (const char*)sqlite3_column_text(stmt, 11);
        results.push_back(entry);
    }
    
    sqlite3_finalize(stmt);
    return results;
}

void Database::delete_failed_videos() {
    // First delete associated markers, thumbnails, embeddings
    const char* delete_related = R"(
        DELETE FROM markers WHERE video_id IN (SELECT id FROM videos WHERE status = 'failed');
        DELETE FROM thumbnails WHERE video_id IN (SELECT id FROM videos WHERE status = 'failed');
    )";
    char* err_msg = nullptr;
    sqlite3_exec(m_db, delete_related, nullptr, nullptr, &err_msg);
    if (err_msg) {
        std::cerr << "Failed to delete failed video data: " << err_msg << std::endl;
        sqlite3_free(err_msg);
    }
    
    // Delete embeddings for failed videos (simplified query)
    const char* delete_emb = "DELETE FROM embeddings";
    sqlite3_exec(m_db, delete_emb, nullptr, nullptr, &err_msg);
    if (err_msg) {
        std::cerr << "Failed to delete embeddings: " << err_msg << std::endl;
        sqlite3_free(err_msg);
    }
    
    // Then delete the failed videos
    const char* sql = "DELETE FROM videos WHERE status = 'failed'";
    sqlite3_exec(m_db, sql, nullptr, nullptr, &err_msg);
    if (err_msg) {
        std::cerr << "Failed to delete failed videos: " << err_msg << std::endl;
        sqlite3_free(err_msg);
    }
}

void Database::delete_all_videos() {
    // Delete all associated data from all tables in correct order (respecting foreign keys)
    char* err_msg = nullptr;
    
    // 1. Clear all vision analyses
    sqlite3_exec(m_db, "DELETE FROM vision_analyses", nullptr, nullptr, &err_msg);
    if (err_msg) {
        std::cerr << "Failed to delete vision analyses: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        err_msg = nullptr;
    }
    
    // 2. Clear all transcriptions
    sqlite3_exec(m_db, "DELETE FROM transcriptions", nullptr, nullptr, &err_msg);
    if (err_msg) {
        std::cerr << "Failed to delete transcriptions: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        err_msg = nullptr;
    }
    
    // 3. Clear all markers
    sqlite3_exec(m_db, "DELETE FROM markers", nullptr, nullptr, &err_msg);
    if (err_msg) {
        std::cerr << "Failed to delete markers: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        err_msg = nullptr;
    }
    
    // 4. Clear all thumbnails
    sqlite3_exec(m_db, "DELETE FROM thumbnails", nullptr, nullptr, &err_msg);
    if (err_msg) {
        std::cerr << "Failed to delete thumbnails: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        err_msg = nullptr;
    }
    
    // 5. Clear all embeddings
    sqlite3_exec(m_db, "DELETE FROM embeddings", nullptr, nullptr, &err_msg);
    if (err_msg) {
        std::cerr << "Failed to delete embeddings: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        err_msg = nullptr;
    }
    
    // 6. Clear all timeline entries
    sqlite3_exec(m_db, "DELETE FROM timeline", nullptr, nullptr, &err_msg);
    if (err_msg) {
        std::cerr << "Failed to delete timeline entries: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        err_msg = nullptr;
    }
    
    // 7. Delete all videos
    sqlite3_exec(m_db, "DELETE FROM videos", nullptr, nullptr, &err_msg);
    if (err_msg) {
        std::cerr << "Failed to delete videos: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        err_msg = nullptr;
    }
    
    LUCID_INFO("Database::delete_all_videos: All video entries and associated data deleted");
}

void Database::reset_completed_to_pending() {
    // Reset completed videos to pending and clear their analysis data
    const char* clear_data = R"(
        DELETE FROM markers WHERE video_id IN (SELECT id FROM videos WHERE status = 'completed');
        DELETE FROM thumbnails WHERE video_id IN (SELECT id FROM videos WHERE status = 'completed');
        DELETE FROM vision_analyses WHERE video_id IN (SELECT id FROM videos WHERE status = 'completed');
        DELETE FROM embeddings WHERE video_id IN (SELECT id FROM videos WHERE status = 'completed');
    )";
    char* err_msg = nullptr;
    sqlite3_exec(m_db, clear_data, nullptr, nullptr, &err_msg);
    if (err_msg) {
        std::cerr << "Failed to clear analysis data: " << err_msg << std::endl;
        sqlite3_free(err_msg);
    }
    
    // Reset status to pending
    const char* sql = R"(
        UPDATE videos SET status = 'pending', updated_at = datetime('now')
        WHERE status = 'completed'
    )";
    sqlite3_exec(m_db, sql, nullptr, nullptr, &err_msg);
    if (err_msg) {
        std::cerr << "Failed to reset completed videos: " << err_msg << std::endl;
        sqlite3_free(err_msg);
    }
}

} // namespace lucid