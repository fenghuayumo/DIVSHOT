#pragma once

/**
 * @file asset_system.h
 * @brief Unified asset management system header
 *
 * This header provides access to the four-layer asset management architecture:
 * 1. AssetDB / AssetRegistry - Metadata, dependencies, versioning, hot reload
 * 2. AssetCache / Loader - Async IO, CPU caching, state machine, failure fallback
 * 3. GpuResourceSystem - Upload queue, budget control, bindless, deferred release
 * 4. RenderGraph - Frame-local resources (separate system)
 */

// Layer 0: Core types
#include "asset_id.h"
#include "asset_handle.h"
#include "asset_metadata.h"
#include "cpu_assets.h"

// Layer 2: GPU resource types
#include "gpu_assets.h"

// Layer 3: AssetRegistry (AssetDB)
#include "asset_registry.h"

// Layer 4: AssetCache and Loader
#include "asset_cache.h"
#include "asset_loader.h"

// Layer 5: Material system
#include "material_asset.h"

// Layer 6: GPU resource management
#include "rendering/gpu_resource_system.h"

// Layer 7: File watching for hot reload
#include "asset_file_watcher.h"
#include "asset_pipeline.h"
#include "texture_importer.h"
#include "model_asset.h"
#include "primitive_type.h"
#include <memory>

namespace diverse
{
    // AssetSystem - Facade for the entire asset management system
    class AssetSystem
    {
    public:
        static AssetSystem& get_instance();

        // Initialize the asset system
        void initialize(rhi::GpuDevice* device);

        // Shutdown the asset system
        void shutdown();

        // Per-frame update
        void update(uint64_t frame_index, float delta_time);

        // Access subsystems
        AssetRegistry& registry() { return AssetRegistry::get_instance(); }
        GpuResourceSystem& gpu_system() { return *gpu_resource_system; }
        AssetFileWatcher& file_watcher() { return *file_watcher_ptr; }

        // Asset loading helpers
        template<typename TAsset>
        std::shared_ptr<TAsset> load_asset(const std::filesystem::path& path);

        template<typename TAsset>
        std::shared_ptr<TAsset> get_asset(const AssetId& id);

        template<typename TAsset>
        std::shared_ptr<TAsset> resolve(const AssetHandle<TAsset>& handle);

        std::shared_ptr<TextureAsset> get_default_white_texture() const { return default_white; }
        std::shared_ptr<TextureAsset> get_default_normal_texture() const { return default_normal; }

        AssetCache<TextureAsset>& texture_cache() { return *texture_cache_ptr; }
        AssetCache<MeshAsset>& mesh_cache() { return *mesh_cache_ptr; }
        AssetCache<MaterialAsset>& material_cache() { return *material_cache_ptr; }
        AssetCache<ModelAsset>& model_cache() { return *model_cache_ptr; }
        AssetPipeline& pipeline() { return AssetPipeline::get_instance(); }

        // Model loading (Registry + Pipeline entry points)
        std::shared_ptr<ModelAsset> load_model(
            const std::filesystem::path& path,
            bool preserve_origin = false);
        std::shared_ptr<ModelAsset> load_primitive(PrimitiveType type);
        std::shared_ptr<ModelAsset> get_model(const AssetId& id);

        // GPU resource helpers
        void queue_gpu_upload(const AssetId& id, UploadPriority priority = UploadPriority::Normal);

        // Hot reload helpers
        void enable_hot_reload(bool enable);
        bool is_hot_reload_enabled() const;
        void process_hot_reloads(uint64_t frame_index);

        // CPU cache registration (used by loaders)
        void register_cpu_texture(const std::shared_ptr<TextureAsset>& texture);
        void register_cpu_mesh(const std::shared_ptr<MeshAsset>& mesh);
        void register_cpu_material(const std::shared_ptr<MaterialAsset>& material);
        void register_cpu_model(const std::shared_ptr<ModelAsset>& model);

    private:
        AssetSystem() = default;
        ~AssetSystem() = default;

        std::shared_ptr<GpuResourceSystem> gpu_resource_system;
        std::shared_ptr<AssetFileWatcher> file_watcher_ptr;

        std::unique_ptr<AssetCache<TextureAsset>> texture_cache_ptr;
        std::unique_ptr<AssetCache<MeshAsset>> mesh_cache_ptr;
        std::unique_ptr<AssetCache<MaterialAsset>> material_cache_ptr;
        std::unique_ptr<AssetCache<ModelAsset>> model_cache_ptr;

        std::shared_ptr<TextureAsset> default_white;
        std::shared_ptr<TextureAsset> default_normal;

        void ensure_cpu_caches();
        void register_default_textures();
        void invalidate_for_hot_reload(const AssetId& id, uint64_t frame_index);
        void begin_hot_reload(const AssetId& id, uint64_t frame_index);

        bool hot_reload_enabled;
        bool is_initialized;
    };

    // Convenience functions
    inline AssetSystem& AssetSys() { return AssetSystem::get_instance(); }

} // namespace diverse
