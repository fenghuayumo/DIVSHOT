#pragma once

#include "asset_metadata.h"
#include "asset_id.h"
#include "asset_handle.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <filesystem>
#include <mutex>
#include <functional>
#include <memory>

namespace diverse
{
    // Forward declaration
    class AssetFileWatcher;

    // Callback type for asset change notifications
    using AssetChangeCallback = std::function<void(const AssetId&)>;

    // AssetRegistry - Central database for all asset metadata
    // Responsibilities:
    // - Asset ID generation and management
    // - Path to ID mapping
    // - Dependency tracking
    // - Version tracking for hot reload
    // - Type query
    class AssetRegistry
    {
    public:
        static AssetRegistry& get_instance();

        // Register a new asset or update existing asset metadata
        void register_asset(const AssetId& id, const AssetMetadata& metadata);

        // Unregister an asset
        void unregister_asset(const AssetId& id);

        // Get asset metadata
        std::shared_ptr<AssetMetadata> get_metadata(const AssetId& id) const;
        std::shared_ptr<AssetMetadata> get_metadata_by_path(const std::filesystem::path& path) const;

        // Path mapping
        AssetId find_by_path(const std::filesystem::path& path) const;
        std::filesystem::path get_path(const AssetId& id) const;

        // Type query
        AssetType get_type(const AssetId& id) const;

        // Dependency tracking
        void add_dependency(const AssetId& asset, const AssetId& depends_on);
        void remove_dependency(const AssetId& asset, const AssetId& depends_on);
        std::vector<AssetId> get_dependencies(const AssetId& id) const;
        std::vector<AssetId> get_dependents(const AssetId& id) const;

        // Check for circular dependencies
        bool has_circular_dependency(const AssetId& asset, const AssetId& depends_on) const;

        // Version tracking for hot reload
        uint32_t get_version(const AssetId& id) const;
        void increment_version(const AssetId& id);

        // Lifecycle state
        AssetState get_state(const AssetId& id) const;
        void set_state(const AssetId& id, AssetState state);

        // Stable handle with generation synced to version
        template<typename TAsset>
        AssetHandle<TAsset> get_handle(const AssetId& id) const;

        AssetHandle<TextureAsset> get_texture_handle(const AssetId& id) const { return get_handle<TextureAsset>(id); }
        AssetHandle<MeshAsset> get_mesh_handle(const AssetId& id) const { return get_handle<MeshAsset>(id); }
        AssetHandle<MaterialAsset> get_material_handle(const AssetId& id) const { return get_handle<MaterialAsset>(id); }

        // Asset change notifications
        void register_change_callback(const AssetId& id, AssetChangeCallback callback);
        void notify_asset_changed(const AssetId& id);

        // File watching integration
        void set_file_watcher(std::shared_ptr<AssetFileWatcher> watcher);
        std::vector<AssetId> collect_file_changes();

        // Query all assets of a type
        std::vector<AssetId> find_by_type(AssetType type) const;

        // Statistics
        size_t get_asset_count() const;
        size_t get_asset_count(AssetType type) const;

        // Validation
        bool validate_asset(const AssetId& id) const;

        // Clear all assets (for testing/development)
        void clear();

    private:
        AssetRegistry() = default;
        ~AssetRegistry() = default;
        AssetRegistry(const AssetRegistry&) = delete;
        AssetRegistry& operator=(const AssetRegistry&) = delete;

        mutable std::mutex mutex;

        // Asset ID -> Metadata mapping
        std::unordered_map<AssetId, std::shared_ptr<AssetMetadata>> assets;

        // Path -> Asset ID mapping (for fast lookup)
        std::unordered_map<std::string, AssetId> path_to_id;

        // Change callbacks per asset
        std::unordered_map<AssetId, std::vector<AssetChangeCallback>> change_callbacks;

        // File watcher (optional, for hot reload)
        std::shared_ptr<AssetFileWatcher> file_watcher;

        // Internal helper to update dependent metadata
        std::shared_ptr<AssetMetadata> get_metadata_unlocked(const AssetId& id) const;
        bool has_circular_dependency_unlocked(const AssetId& asset, const AssetId& depends_on) const;
        void update_dependent_metadata(const AssetId& id);
        void increment_version_unlocked(
            const AssetId& id,
            std::vector<AssetId>& changed,
            std::unordered_set<AssetId>& visited);
    };

    template<typename TAsset>
    AssetHandle<TAsset> AssetRegistry::get_handle(const AssetId& id) const
    {
        if (!id.is_valid())
            return {};
        return AssetHandle<TAsset>(id, get_version(id));
    }

    // RAII helper for change callback registration
    class AssetChangeCallbackRegistration
    {
    public:
        AssetChangeCallbackRegistration(AssetId id, AssetChangeCallback callback);
        ~AssetChangeCallbackRegistration();

    private:
        AssetId asset_id;
    };
}
