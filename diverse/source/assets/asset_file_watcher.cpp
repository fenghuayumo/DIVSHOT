#include "asset_file_watcher.h"
#include "utility/file_utils.h"
#include <algorithm>

namespace diverse
{
    static constexpr std::chrono::milliseconds DEFAULT_POLL_INTERVAL{ 100 };  // 100ms

    AssetFileWatcher::AssetFileWatcher()
        : is_enabled(true)
        , poll_interval(DEFAULT_POLL_INTERVAL)
        , last_poll(std::chrono::steady_clock::now())
    {
    }

    AssetFileWatcher::~AssetFileWatcher()
    {
        clear();
    }

    void AssetFileWatcher::watch_file(const std::filesystem::path& path, FileChangeCallback callback)
    {
        std::lock_guard lock(mutex);

        std::string path_str = path.string();

        WatchEntry entry;
        entry.path = path;
        entry.is_directory = false;
        entry.callback = std::move(callback);

        // Get initial file time
        if (std::filesystem::exists(path))
        {
            try
            {
                entry.last_check_time = std::filesystem::last_write_time(path);
            }
            catch (const std::filesystem::filesystem_error&)
            {
                entry.last_check_time = std::filesystem::file_time_type::min();
            }
        }
        else
        {
            entry.last_check_time = std::filesystem::file_time_type::min();
        }

        watches[path_str] = std::move(entry);
    }

    void AssetFileWatcher::watch_directory(const std::filesystem::path& path, bool recursive, FileChangeCallback callback)
    {
        std::lock_guard lock(mutex);

        std::string path_str = path.string();

        WatchEntry entry;
        entry.path = path;
        entry.is_directory = true;
        entry.callback = std::move(callback);

        watches[path_str] = std::move(entry);

        // Also watch all existing files in the directory
        auto files = get_directory_files(path, recursive);
        for (const auto& file : files)
        {
            watch_file(file, callback);
        }
    }

    void AssetFileWatcher::unwatch(const std::filesystem::path& path)
    {
        std::lock_guard lock(mutex);

        std::string path_str = path.string();
        watches.erase(path_str);
    }

    void AssetFileWatcher::update()
    {
        if (!is_enabled)
        {
            return;
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_poll);

        if (elapsed < poll_interval)
        {
            return;
        }

        last_poll = now;

        std::lock_guard lock(mutex);

        for (auto& [path_str, entry] : watches)
        {
            check_watch_entry(entry);
        }
    }

    void AssetFileWatcher::check_watch_entry(WatchEntry& entry)
    {
        if (!entry.callback)
        {
            return;
        }

        if (entry.is_directory)
        {
            // For directories, check if new files were added
            if (!std::filesystem::exists(entry.path))
            {
                return;  // Directory doesn't exist
            }

            // Scan directory for new/modified files
            auto files = get_directory_files(entry.path, false);
            for (const auto& file : files)
            {
                std::filesystem::file_time_type dummy_time;
                if (was_file_modified(file, dummy_time))
                {
                    FileChangeEvent event;
                    event.path = file;
                    event.type = FileChangeType::Modified;
                    event.timestamp = std::filesystem::last_write_time(file);
                    entry.callback(event);
                }
            }
        }
        else
        {
            // For individual files, check modification time
            std::filesystem::file_time_type new_time;
            if (was_file_modified(entry.path, new_time))
            {
                FileChangeEvent event;
                event.path = entry.path;
                event.type = std::filesystem::exists(entry.path) ?
                    FileChangeType::Modified : FileChangeType::Deleted;
                event.timestamp = new_time;
                entry.callback(event);

                // Update last check time
                entry.last_check_time = new_time;
            }
        }
    }

    bool AssetFileWatcher::was_file_modified(const std::filesystem::path& path, std::filesystem::file_time_type& last_time)
    {
        if (!std::filesystem::exists(path))
        {
            // File was deleted
            last_time = std::filesystem::file_time_type::min();
            return true;
        }

        try
        {
            auto current_time = std::filesystem::last_write_time(path);

            // Check if file was modified (time > last check)
            if (current_time > last_time)
            {
                last_time = current_time;
                return true;
            }
        }
        catch (const std::filesystem::filesystem_error&)
        {
            // Error accessing file
            return false;
        }

        return false;
    }

    std::vector<std::filesystem::path> AssetFileWatcher::get_directory_files(const std::filesystem::path& dir, bool recursive)
    {
        std::vector<std::filesystem::path> files;

        try
        {
            if (recursive)
            {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(dir))
                {
                    if (entry.is_regular_file())
                    {
                        files.push_back(entry.path());
                    }
                }
            }
            else
            {
                for (const auto& entry : std::filesystem::directory_iterator(dir))
                {
                    if (entry.is_regular_file())
                    {
                        files.push_back(entry.path());
                    }
                }
            }
        }
        catch (const std::filesystem::filesystem_error&)
        {
            // Directory access error
        }

        return files;
    }

    size_t AssetFileWatcher::get_watch_count() const
    {
        std::lock_guard lock(mutex);
        return watches.size();
    }

    void AssetFileWatcher::clear()
    {
        std::lock_guard lock(mutex);
        watches.clear();
    }

    // FileWatchRegistration implementation
    FileWatchRegistration::FileWatchRegistration(AssetFileWatcher& watcher, const std::filesystem::path& path)
        : watcher(&watcher)
        , path(path)
        , is_valid(true)
    {
    }

    FileWatchRegistration::~FileWatchRegistration()
    {
        if (is_valid && watcher)
        {
            watcher->unwatch(path);
        }
    }

    FileWatchRegistration::FileWatchRegistration(FileWatchRegistration&& other) noexcept
        : watcher(other.watcher)
        , path(std::move(other.path))
        , is_valid(other.is_valid)
    {
        other.is_valid = false;
    }

    FileWatchRegistration& FileWatchRegistration::operator=(FileWatchRegistration&& other) noexcept
    {
        if (this != &other)
        {
            if (is_valid && watcher)
            {
                watcher->unwatch(path);
            }

            watcher = other.watcher;
            path = std::move(other.path);
            is_valid = other.is_valid;
            other.is_valid = false;
        }
        return *this;
    }

} // namespace diverse
