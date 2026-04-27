// video_item.cpp - VideoItem implementation
#include "video_item.hpp"
#include "../model/database.hpp"

namespace lucid {

VideoItem VideoItem::from_entry(const VideoEntry& entry) {
    VideoItem item;
    item.id = entry.id;
    item.path = entry.path;
    item.title = entry.title;
    item.thumbnail_path = entry.thumbnail.value_or("");
    item.duration_sec = static_cast<int>(entry.duration_ms / 1000);
    item.is_indexed = (entry.status == VideoStatus::COMPLETED);
    item.is_transcribing = (entry.status == VideoStatus::TRANSCRIBING);
    return item;
}

} // namespace lucid