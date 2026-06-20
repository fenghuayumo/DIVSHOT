#pragma once

#include "asset_id.h"
#include <filesystem>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <vector>
#include <string>
#include <chrono>

namespace diverse
{
    // File change event type
    enum class FileChangeType
    {
        Modified,
        Created,
        Deleted
    };

    // File change event
    struct FileChangeEvent
    {
        std::filesystem::path path;
        FileChangeType type;
        std::filesystem::file_time_type timestamp;
    };

    // File change callback
    using FileChangeCallback = std::function<void(const FileChangeEvent&)>;

    // Watch entry for a specific file or directory
    struct WatchEntry
    {
        std::filesystem::path path;
        std::filesystem::file_time_type last_check_time;
        bool is_directory;
        FileChangeCallback callback;
    };

    // AssetFileWatcher - Monitors file system for changes
    // Supports hot reload by detecting asset file modifications
    class AssetFileWatcher
    {
    public:
        AssetFileWatcher();
        ~AssetFileWatcher();

        // Watch a specific file for changes
        void watch_file(const std::filesystem::path& path, FileChangeCallback callback);

        // Watch a directory for changes (recursively)
        void watch_directory(const std::filesystem::path& path, bool recursive, FileChangeCallback callback);

        // Stop watching a path
        void unwatch(const std::filesystem::path& path);

        // Check for file changes (call this regularly, e.g., once per frame)
        void update();

        // Enable/disable file watching
        void set_enabled(bool enabled) { is_enabled = enabled; }
        bool get_enabled() const { return is_enabled; }

        // Get watch count
        size_t get_watch_count() const;

        // Clear all watches
        void clear();

    private:
        mutable std::mutex mutex;
        std::unordered_map<std::string, WatchEntry> watches;

        bool is_enabled;
        std::chrono::milliseconds poll_interval;
        std::chrono::steady_clock::time_point last_poll;

        // Check a single watch entry
        void check_watch_entry(WatchEntry& entry);

        // Check if file was modified
        bool was_file_modified(const std::filesystem::path& path, std::filesystem::file_time_type& last_time);

        // Get all files in a directory
        std::vector<std::filesystem::path> get_directory_files(const std::filesystem::path& dir, bool recursive);
    };

    // RAII helper for automatic watch removal
    class FileWatchRegistration
    {
    public:
        FileWatchRegistration(AssetFileWatcher& watcher, const std::filesystem::path& path);
        ~FileWatchRegistration();

        FileWatchRegistration(const FileWatchRegistration&) = delete;
        FileWatchRegistration& operator=(const FileWatchRegistration&) = delete;
        FileWatchRegistration(FileWatchRegistration&& other) noexcept;
        FileWatchRegistration& operator=(FileWatchRegistration&& other) noexcept;

    private:
        AssetFileWatcher* watcher;
        std::filesystem::path path;
        bool is_valid;
    };

} // namespace diverse
