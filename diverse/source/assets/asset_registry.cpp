#include "asset_registry.h"
#include "engine/file_system.h"
#include "core/ds_log.h"
#include "utility/file_utils.h"
#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace diverse
{
    namespace
    {
        bool is_non_reloadable_path(const std::filesystem::path& path)
        {
            const auto path_str = path.string();
            return path_str.starts_with("primitive://");
        }

        std::filesystem::path resolve_watch_path(const std::filesystem::path& logical_path)
        {
            if (logical_path.empty() || is_non_reloadable_path(logical_path))
                return {};

            std::string physical_path;
            if (FileSystem::get().resolve_physical_path(logical_path.string(), physical_path))
                return std::filesystem::path(physical_path);

            return logical_path;
        }

        bool query_file_mtime(
            const std::filesystem::path& logical_path,
            std::filesystem::file_time_type& out_mtime)
        {
            const auto watch_path = resolve_watch_path(logical_path);
            if (watch_path.empty())
                return false;

            try
            {
                if (!std::filesystem::exists(watch_path))
                    return false;

                out_mtime = std::filesystem::last_write_time(watch_path);
                return true;
            }
            catch (const std::filesystem::filesystem_error&)
            {
                return false;
            }
        }
    }

    AssetRegistry& AssetRegistry::get_instance()
    {
        static AssetRegistry instance;
        return instance;
    }

    void AssetRegistry::register_asset(const AssetId& id, const AssetMetadata& metadata)
    {
        std::lock_guard lock(mutex);

        // Check if asset already exists
        auto it = assets.find(id);
        if (it != assets.end())
        {
            const auto preserved_mtime = it->second->last_modified;
            *it->second = metadata;
            if (metadata.last_modified == std::filesystem::file_time_type{} &&
                preserved_mtime != std::filesystem::file_time_type{})
            {
                it->second->last_modified = preserved_mtime;
            }
        }
        else
        {
            assets[id] = std::make_shared<AssetMetadata>(metadata);
        }

        // Update path mapping
        if (!metadata.source_path.empty())
        {
            std::string path_str = metadata.source_path.string();
            path_to_id[path_str] = id;
        }

        auto& stored = assets[id];
        if (stored && stored->last_modified == std::filesystem::file_time_type{})
        {
            std::filesystem::file_time_type initial_mtime;
            if (query_file_mtime(stored->source_path, initial_mtime))
                stored->last_modified = initial_mtime;
        }
    }

    void AssetRegistry::unregister_asset(const AssetId& id)
    {
        std::lock_guard lock(mutex);

        auto it = assets.find(id);
        if (it != assets.end())
        {
            // Remove from path mapping
            if (!it->second->source_path.empty())
            {
                std::string path_str = it->second->source_path.string();
                path_to_id.erase(path_str);
            }

            // Remove change callbacks
            change_callbacks.erase(id);

            // Remove asset
            assets.erase(it);
        }
    }

    std::shared_ptr<AssetMetadata> AssetRegistry::get_metadata(const AssetId& id) const
    {
        std::lock_guard lock(mutex);

        auto it = assets.find(id);
        if (it != assets.end())
        {
            return it->second;
        }
        return nullptr;
    }

    std::shared_ptr<AssetMetadata> AssetRegistry::get_metadata_by_path(const std::filesystem::path& path) const
    {
        std::lock_guard lock(mutex);

        std::string path_str = path.string();
        auto it = path_to_id.find(path_str);
        if (it != path_to_id.end())
        {
            return get_metadata(it->second);
        }
        return nullptr;
    }

    AssetId AssetRegistry::find_by_path(const std::filesystem::path& path) const
    {
        std::lock_guard lock(mutex);

        std::string path_str = path.string();
        auto it = path_to_id.find(path_str);
        if (it != path_to_id.end())
        {
            return it->second;
        }
        return InvalidAssetId();
    }

    std::filesystem::path AssetRegistry::get_path(const AssetId& id) const
    {
        auto metadata = get_metadata(id);
        if (metadata)
        {
            return metadata->source_path;
        }
        return std::filesystem::path();
    }

    AssetType AssetRegistry::get_type(const AssetId& id) const
    {
        auto metadata = get_metadata(id);
        if (metadata)
        {
            return metadata->type;
        }
        return AssetType::None;
    }

    void AssetRegistry::add_dependency(const AssetId& asset, const AssetId& depends_on)
    {
        std::lock_guard lock(mutex);

        auto asset_meta = get_metadata(asset);
        auto dep_meta = get_metadata(depends_on);

        if (!asset_meta || !dep_meta)
        {
            return;  // Invalid assets
        }

        // Check for circular dependency
        if (has_circular_dependency(asset, depends_on))
        {
            DS_LOG_WARN("Circular dependency detected between asset {} and {}", asset.id, depends_on.id);
            return;
        }

        // Add forward dependency
        auto& deps = asset_meta->dependencies;
        if (std::find(deps.begin(), deps.end(), depends_on) == deps.end())
        {
            deps.push_back(depends_on);
        }

        // Add reverse dependency
        auto& dependents = dep_meta->dependents;
        if (std::find(dependents.begin(), dependents.end(), asset) == dependents.end())
        {
            dependents.push_back(asset);
        }
    }

    void AssetRegistry::remove_dependency(const AssetId& asset, const AssetId& depends_on)
    {
        std::lock_guard lock(mutex);

        auto asset_meta = get_metadata(asset);
        auto dep_meta = get_metadata(depends_on);

        if (!asset_meta || !dep_meta)
        {
            return;
        }

        // Remove forward dependency
        auto& deps = asset_meta->dependencies;
        deps.erase(std::remove(deps.begin(), deps.end(), depends_on), deps.end());

        // Remove reverse dependency
        auto& dependents = dep_meta->dependents;
        dependents.erase(std::remove(dependents.begin(), dependents.end(), asset), dependents.end());
    }

    std::vector<AssetId> AssetRegistry::get_dependencies(const AssetId& id) const
    {
        auto metadata = get_metadata(id);
        if (metadata)
        {
            return metadata->dependencies;
        }
        return {};
    }

    std::vector<AssetId> AssetRegistry::get_dependents(const AssetId& id) const
    {
        auto metadata = get_metadata(id);
        if (metadata)
        {
            return metadata->dependents;
        }
        return {};
    }

    bool AssetRegistry::has_circular_dependency(const AssetId& asset, const AssetId& depends_on) const
    {
        // Check if 'depends_on' transitively depends on 'asset'
        std::function<bool(const AssetId&, std::unordered_set<AssetId>&)> check_transitive;
        check_transitive = [&](const AssetId& current, std::unordered_set<AssetId>& visited) -> bool
        {
            if (current == asset)
            {
                return true;  // Found circular dependency
            }

            if (visited.count(current))
            {
                return false;  // Already checked this path
            }
            visited.insert(current);

            auto metadata = get_metadata(current);
            if (!metadata)
            {
                return false;
            }

            for (const auto& dep : metadata->dependencies)
            {
                if (check_transitive(dep, visited))
                {
                    return true;
                }
            }

            return false;
        };

        std::unordered_set<AssetId> visited;
        return check_transitive(depends_on, visited);
    }

    uint32_t AssetRegistry::get_version(const AssetId& id) const
    {
        auto metadata = get_metadata(id);
        if (metadata)
        {
            return metadata->version;
        }
        return 0;
    }

    void AssetRegistry::increment_version(const AssetId& id)
    {
        std::lock_guard lock(mutex);
        increment_version_unlocked(id);
    }

    void AssetRegistry::increment_version_unlocked(const AssetId& id)
    {
        auto it = assets.find(id);
        if (it == assets.end())
            return;

        it->second->version++;

        notify_asset_changed(id);

        std::vector<AssetId> dependents = it->second->dependents;
        for (const auto& dependent : dependents)
            increment_version_unlocked(dependent);
    }

    AssetState AssetRegistry::get_state(const AssetId& id) const
    {
        auto metadata = get_metadata(id);
        return metadata ? metadata->state : AssetState::Unloaded;
    }

    void AssetRegistry::set_state(const AssetId& id, AssetState state)
    {
        std::lock_guard lock(mutex);
        auto it = assets.find(id);
        if (it != assets.end())
            it->second->state = state;
    }

    void AssetRegistry::register_change_callback(const AssetId& id, AssetChangeCallback callback)
    {
        std::lock_guard lock(mutex);
        change_callbacks[id].push_back(callback);
    }

    void AssetRegistry::notify_asset_changed(const AssetId& id)
    {
        auto it = change_callbacks.find(id);
        if (it != change_callbacks.end())
        {
            for (const auto& callback : it->second)
            {
                callback(id);
            }
        }
    }

    void AssetRegistry::set_file_watcher(std::shared_ptr<AssetFileWatcher> watcher)
    {
        std::lock_guard lock(mutex);
        file_watcher = watcher;
    }

    std::vector<AssetId> AssetRegistry::collect_file_changes()
    {
        struct WatchEntry
        {
            AssetId id;
            std::filesystem::path source_path;
            std::filesystem::file_time_type last_modified;
        };

        std::vector<WatchEntry> watch_list;
        {
            std::lock_guard lock(mutex);
            if (!file_watcher)
                return {};

            watch_list.reserve(assets.size());
            for (const auto& [id, metadata] : assets)
            {
                if (!metadata || metadata->source_path.empty() || is_non_reloadable_path(metadata->source_path))
                    continue;

                watch_list.push_back({ id, metadata->source_path, metadata->last_modified });
            }
        }

        struct PendingChange
        {
            AssetId id;
            std::filesystem::file_time_type mtime;
        };

        std::vector<PendingChange> pending;
        pending.reserve(watch_list.size());

        for (const auto& entry : watch_list)
        {
            std::filesystem::file_time_type current_time;
            if (!query_file_mtime(entry.source_path, current_time))
                continue;

            if (current_time <= entry.last_modified)
                continue;

            pending.push_back({ entry.id, current_time });
        }

        std::vector<AssetId> changed;
        changed.reserve(pending.size());

        {
            std::lock_guard lock(mutex);
            for (const auto& change : pending)
            {
                auto it = assets.find(change.id);
                if (it == assets.end())
                    continue;

                it->second->last_modified = change.mtime;
                it->second->state = AssetState::LoadingCpu;
                increment_version_unlocked(change.id);
                changed.push_back(change.id);

                DS_LOG_INFO("Hot reload: detected file change for {}", it->second->source_path.string());
            }
        }

        return changed;
    }

    std::vector<AssetId> AssetRegistry::find_by_type(AssetType type) const
    {
        std::lock_guard lock(mutex);

        std::vector<AssetId> result;
        for (const auto& [id, metadata] : assets)
        {
            if (metadata->type == type)
            {
                result.push_back(id);
            }
        }
        return result;
    }

    size_t AssetRegistry::get_asset_count() const
    {
        std::lock_guard lock(mutex);
        return assets.size();
    }

    size_t AssetRegistry::get_asset_count(AssetType type) const
    {
        std::lock_guard lock(mutex);

        size_t count = 0;
        for (const auto& [id, metadata] : assets)
        {
            if (metadata->type == type)
            {
                count++;
            }
        }
        return count;
    }

    bool AssetRegistry::validate_asset(const AssetId& id) const
    {
        auto metadata = get_metadata(id);
        if (!metadata)
        {
            return false;
        }

        // Check if source file exists
        if (!metadata->source_path.empty())
        {
            return std::filesystem::exists(metadata->source_path);
        }

        return true;
    }

    void AssetRegistry::clear()
    {
        std::lock_guard lock(mutex);
        assets.clear();
        path_to_id.clear();
        change_callbacks.clear();
    }

    void AssetRegistry::update_dependent_metadata(const AssetId& id)
    {
        // This is called when an asset is updated to refresh dependent metadata
        auto metadata = get_metadata(id);
        if (!metadata)
        {
            return;
        }

        for (const auto& dependent_id : metadata->dependents)
        {
            auto dep_metadata = get_metadata(dependent_id);
            if (dep_metadata)
            {
                // Trigger any necessary updates for dependent assets
                notify_asset_changed(dependent_id);
            }
        }
    }

    // AssetChangeCallbackRegistration implementation
    AssetChangeCallbackRegistration::AssetChangeCallbackRegistration(AssetId id, AssetChangeCallback callback)
        : asset_id(id)
    {
        AssetRegistry::get_instance().register_change_callback(id, callback);
    }

    AssetChangeCallbackRegistration::~AssetChangeCallbackRegistration()
    {
        // Callbacks are removed when the asset is unregistered
        // Individual callback removal is not implemented for simplicity
    }
}
