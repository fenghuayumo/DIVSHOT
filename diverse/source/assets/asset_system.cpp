#include "asset_system.h"
#include "asset_pipeline_handlers.h"
#include "model_asset_loader.h"
#include "point_cloud_asset.h"
#include "gaussian_asset.h"
#include "material_importer.h"
#include "mesh_importer.h"
#include "backend/drs_rhi/gpu_device.h"
#include "core/ds_log.h"
#include <chrono>
#include <mutex>

namespace diverse
{
    namespace
    {
        template<typename TAsset>
        AssetType metadata_type_for()
        {
            if constexpr (std::is_same_v<TAsset, TextureAsset>)
                return AssetType::Texture;
            else if constexpr (std::is_same_v<TAsset, MeshAsset>)
                return AssetType::MeshModel;
            else if constexpr (std::is_same_v<TAsset, MaterialAsset>)
                return AssetType::Material;
            else if constexpr (std::is_same_v<TAsset, ModelAsset>)
                return AssetType::MeshModel;
            else if constexpr (std::is_same_v<TAsset, PointCloudAsset>)
                return AssetType::PointCloud;
            else if constexpr (std::is_same_v<TAsset, GaussianAsset>)
                return AssetType::Gaussian;
            else
                return AssetType::None;
        }

        template<typename TAsset>
        AssetCache<TAsset>& cache_for(AssetSystem& sys)
        {
            if constexpr (std::is_same_v<TAsset, TextureAsset>)
                return sys.texture_cache();
            else if constexpr (std::is_same_v<TAsset, MeshAsset>)
                return sys.mesh_cache();
            else if constexpr (std::is_same_v<TAsset, ModelAsset>)
                return sys.model_cache();
            else if constexpr (std::is_same_v<TAsset, PointCloudAsset>)
                return sys.point_cloud_cache();
            else if constexpr (std::is_same_v<TAsset, GaussianAsset>)
                return sys.gaussian_cache();
            else
                return sys.material_cache();
        }
    }

    AssetSystem& AssetSystem::get_instance()
    {
        static AssetSystem instance;
        return instance;
    }

    void AssetSystem::ensure_cpu_caches()
    {
        if (!texture_cache_ptr)
            texture_cache_ptr = std::make_unique<AssetCache<TextureAsset>>();
        if (!mesh_cache_ptr)
            mesh_cache_ptr = std::make_unique<AssetCache<MeshAsset>>();
        if (!material_cache_ptr)
            material_cache_ptr = std::make_unique<AssetCache<MaterialAsset>>();
        if (!model_cache_ptr)
            model_cache_ptr = std::make_unique<AssetCache<ModelAsset>>();
        if (!point_cloud_cache_ptr)
            point_cloud_cache_ptr = std::make_unique<AssetCache<PointCloudAsset>>();
        if (!gaussian_cache_ptr)
            gaussian_cache_ptr = std::make_unique<AssetCache<GaussianAsset>>();
    }

    void AssetSystem::register_cpu_model(const std::shared_ptr<ModelAsset>& model)
    {
        if (!model)
            return;
        ensure_cpu_caches();
        model_cache_ptr->insert(model->id, model);
    }

    void AssetSystem::register_cpu_texture(const std::shared_ptr<TextureAsset>& texture)
    {
        if (!texture)
            return;
        ensure_cpu_caches();
        texture_cache_ptr->insert(texture->id, texture);

        auto& registry = AssetRegistry::get_instance();
        if (!registry.get_metadata(texture->id))
        {
            AssetMetadata metadata;
            metadata.id = texture->id;
            metadata.type = AssetType::Texture;
            metadata.source_path = texture->source_path;
            metadata.version = texture->version;
            metadata.state = AssetState::ReadyCpu;
            registry.register_asset(texture->id, metadata);
        }
    }

    void AssetSystem::register_cpu_mesh(const std::shared_ptr<MeshAsset>& mesh)
    {
        if (!mesh)
            return;
        ensure_cpu_caches();
        mesh_cache_ptr->insert(mesh->id, mesh);
    }

    void AssetSystem::register_cpu_material(const std::shared_ptr<MaterialAsset>& material)
    {
        if (!material)
            return;
        ensure_cpu_caches();
        material_cache_ptr->insert(material->id, material);

        auto& registry = AssetRegistry::get_instance();
        if (!registry.get_metadata(material->id))
        {
            AssetMetadata metadata;
            metadata.id = material->id;
            metadata.type = AssetType::Material;
            metadata.version = material->version;
            metadata.state = AssetState::ReadyCpu;
            registry.register_asset(material->id, metadata);
        }
    }

    void AssetSystem::register_cpu_point_cloud(const std::shared_ptr<PointCloudAsset>& point_cloud)
    {
        if (!point_cloud)
            return;
        ensure_cpu_caches();
        point_cloud_cache_ptr->insert(point_cloud->id, point_cloud);

        auto& registry = AssetRegistry::get_instance();
        if (!registry.get_metadata(point_cloud->id))
        {
            AssetMetadata metadata;
            metadata.id = point_cloud->id;
            metadata.type = AssetType::PointCloud;
            metadata.source_path = point_cloud->source_path;
            metadata.version = point_cloud->version;
            metadata.state = AssetState::ReadyCpu;
            registry.register_asset(point_cloud->id, metadata);
        }
    }

    void AssetSystem::register_cpu_gaussian(const std::shared_ptr<GaussianAsset>& gaussian)
    {
        if (!gaussian)
            return;
        ensure_cpu_caches();
        gaussian_cache_ptr->insert(gaussian->id, gaussian);

        auto& registry = AssetRegistry::get_instance();
        if (!registry.get_metadata(gaussian->id))
        {
            AssetMetadata metadata;
            metadata.id = gaussian->id;
            metadata.type = AssetType::Gaussian;
            metadata.source_path = gaussian->source_path;
            metadata.version = gaussian->version;
            metadata.state = AssetState::ReadyCpu;
            registry.register_asset(gaussian->id, metadata);
        }
    }

    void AssetSystem::register_asset_loaders()
    {
        texture_loader_ptr = std::make_unique<TextureLoader>();
        mesh_loader_ptr = std::make_unique<MeshLoader>();

        texture_loader_ptr->set_default_asset(get_default_white_texture());

        texture_loader_ptr->set_load_func([](const AssetId& id, const AssetMetadata& metadata) -> std::shared_ptr<TextureAsset> {
            auto texture = import_texture_from_path(metadata.source_path);
            if (!texture)
                return nullptr;
            texture->id = id;
            texture->source_path = metadata.source_path;
            return texture;
        });

        texture_loader_ptr->set_on_loaded([this](const AssetId& id, std::shared_ptr<TextureAsset> asset) {
            register_cpu_texture(asset);
            registry().set_state(id, AssetState::ReadyCpu);
        });

        mesh_loader_ptr->set_load_func([](const AssetId& id, const AssetMetadata& metadata) -> std::shared_ptr<MeshAsset> {
            (void)id;
            (void)metadata;
            return nullptr;
        });
    }

    bool AssetSystem::is_async_loading(const AssetId& id) const
    {
        std::lock_guard lock(async_load_mutex);
        return pending_async_loads.find(id) != pending_async_loads.end();
    }

    void AssetSystem::start_async_load(const AssetId& id)
    {
        auto metadata = AssetRegistry::get_instance().get_metadata(id);
        if (!metadata)
            return;

        {
            std::lock_guard lock(async_load_mutex);
            if (pending_async_loads.find(id) != pending_async_loads.end())
                return;
        }

        registry().set_state(id, AssetState::LoadingCpu);

        switch (metadata->type)
        {
            case AssetType::Texture:
            {
                if (!texture_loader_ptr)
                    return;
                auto future = texture_loader_ptr->load(id, *metadata);
                std::lock_guard lock(async_load_mutex);
                pending_async_loads[id] = PendingAsyncLoad{ std::move(future) };
                break;
            }
            default:
                registry().set_state(id, AssetState::Failed);
                break;
        }
    }

    void AssetSystem::process_async_loads()
    {
        std::vector<AssetId> completed;
        std::vector<AssetId> failed;

        {
            std::lock_guard lock(async_load_mutex);
            for (auto& [id, pending] : pending_async_loads)
            {
                if (pending.future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
                    continue;

                auto result = pending.future.get();
                if (result.success)
                    completed.push_back(id);
                else
                {
                    failed.push_back(id);
                    DS_LOG_WARN("Async asset load failed: {}", result.error_message);
                }
            }

            for (const auto& id : completed)
                pending_async_loads.erase(id);
            for (const auto& id : failed)
                pending_async_loads.erase(id);
        }

        for (const auto& id : failed)
            registry().set_state(id, AssetState::Failed);

        for (const auto& id : completed)
            pipeline().submit(id, PipelineStage::CpuOptimize);
    }

    void AssetSystem::register_default_textures()
    {
        ensure_cpu_caches();
        default_white = create_white_texture_asset();
        default_normal = create_normal_texture_asset();

        auto& registry = AssetRegistry::get_instance();
        auto register_one = [&](const std::shared_ptr<TextureAsset>& tex) {
            if (!tex)
                return;
            AssetMetadata metadata;
            metadata.id = tex->id;
            metadata.type = AssetType::Texture;
            metadata.version = 0;
            metadata.state = AssetState::ReadyCpu;
            registry.register_asset(tex->id, metadata);
            texture_cache_ptr->insert(tex->id, tex);
        };
        register_one(default_white);
        register_one(default_normal);
    }

    void AssetSystem::initialize(rhi::GpuDevice* device)
    {
        if (is_initialized)
        {
            DS_LOG_WARN("AssetSystem already initialized");
            return;
        }

        texture_cache_ptr = std::make_unique<AssetCache<TextureAsset>>();
        mesh_cache_ptr = std::make_unique<AssetCache<MeshAsset>>();
        material_cache_ptr = std::make_unique<AssetCache<MaterialAsset>>();
        model_cache_ptr = std::make_unique<AssetCache<ModelAsset>>();
        point_cloud_cache_ptr = std::make_unique<AssetCache<PointCloudAsset>>();
        gaussian_cache_ptr = std::make_unique<AssetCache<GaussianAsset>>();

        gpu_resource_system = std::make_shared<GpuResourceSystem>(device);
        file_watcher_ptr = std::make_shared<AssetFileWatcher>();
        AssetRegistry::get_instance().set_file_watcher(file_watcher_ptr);

        register_default_textures();
        register_asset_loaders();
        register_asset_pipeline_handlers();

        hot_reload_enabled = true;
        is_initialized = true;

        DS_LOG_INFO("AssetSystem initialized successfully");
    }

    void AssetSystem::shutdown()
    {
        if (!is_initialized)
            return;

        if (gpu_resource_system)
        {
            gpu_resource_system->release();
            gpu_resource_system.reset();
        }

        if (file_watcher_ptr)
        {
            file_watcher_ptr->clear();
            file_watcher_ptr.reset();
        }

        if (texture_cache_ptr) texture_cache_ptr->clear();
        if (mesh_cache_ptr) mesh_cache_ptr->clear();
        if (material_cache_ptr) material_cache_ptr->clear();
        if (model_cache_ptr) model_cache_ptr->clear();
        if (point_cloud_cache_ptr) point_cloud_cache_ptr->clear();
        if (gaussian_cache_ptr) gaussian_cache_ptr->clear();
        {
            std::lock_guard lock(async_load_mutex);
            pending_async_loads.clear();
        }
        texture_loader_ptr.reset();
        mesh_loader_ptr.reset();
        texture_cache_ptr.reset();
        mesh_cache_ptr.reset();
        material_cache_ptr.reset();
        model_cache_ptr.reset();
        point_cloud_cache_ptr.reset();
        gaussian_cache_ptr.reset();
        default_white.reset();
        default_normal.reset();

        AssetRegistry::get_instance().clear();

        hot_reload_enabled = false;
        is_initialized = false;

        DS_LOG_INFO("AssetSystem shutdown complete");
    }

    void AssetSystem::invalidate_for_hot_reload(const AssetId& id, uint64_t frame_index)
    {
        auto& reg = registry();
        const AssetType type = reg.get_type(id);

        auto retire_gpu = [&](const AssetId& asset_id) {
            if (gpu_resource_system)
                gpu_resource_system->retire_asset_gpu(asset_id, frame_index);
        };

        auto remove_cpu = [&](const AssetId& asset_id, AssetType asset_type) {
            switch (asset_type)
            {
                case AssetType::Texture:
                    texture_cache().remove(asset_id);
                    break;
                case AssetType::Material:
                    material_cache().remove(asset_id);
                    break;
                case AssetType::MeshModel:
                    if (mesh_cache().contains(asset_id))
                        mesh_cache().remove(asset_id);
                    else if (model_cache().contains(asset_id))
                        model_cache().remove(asset_id);
                    break;
                default:
                    break;
            }
        };

        if (type == AssetType::MeshModel && model_cache().contains(id))
        {
            const auto dependencies = reg.get_dependencies(id);
            for (const auto& dep_id : dependencies)
            {
                remove_cpu(dep_id, reg.get_type(dep_id));
                retire_gpu(dep_id);
            }

            model_cache().remove(id);
            retire_gpu(id);
            return;
        }

        remove_cpu(id, type);
        retire_gpu(id);
    }

    void AssetSystem::begin_hot_reload(const AssetId& id, uint64_t frame_index)
    {
        DS_UNUSED(frame_index);
        invalidate_for_hot_reload(id, frame_index);
        pipeline().submit(id, PipelineStage::IO);
    }

    void AssetSystem::process_hot_reloads(uint64_t frame_index)
    {
        auto changed = registry().collect_file_changes();
        for (const auto& id : changed)
            begin_hot_reload(id, frame_index);
    }

    void AssetSystem::update(uint64_t frame_index, float delta_time)
    {
        DS_UNUSED(delta_time);
        if (!is_initialized)
            return;

        if (hot_reload_enabled && file_watcher_ptr)
            file_watcher_ptr->update();

        if (hot_reload_enabled)
            process_hot_reloads(frame_index);

        process_async_loads();
        pipeline().tick(frame_index);

        if (gpu_resource_system)
        {
            gpu_resource_system->process_upload_queue(frame_index);
            gpu_resource_system->update(frame_index);
        }
    }

    std::shared_ptr<ModelAsset> AssetSystem::load_model(
        const std::filesystem::path& path,
        bool preserve_origin)
    {
        auto model = load_model_asset(path, preserve_origin);
        if (model && is_initialized)
            pipeline().submit(model->id, PipelineStage::Upload);
        return model;
    }

    std::shared_ptr<ModelAsset> AssetSystem::load_primitive(PrimitiveType type)
    {
        return load_primitive_model(type);
    }

    std::shared_ptr<ModelAsset> AssetSystem::get_model(const AssetId& id)
    {
        return get_asset<ModelAsset>(id);
    }

    template<typename TAsset>
    std::shared_ptr<TAsset> AssetSystem::load_asset(const std::filesystem::path& path)
    {
        auto& registry = AssetRegistry::get_instance();
        auto& cache = cache_for<TAsset>(*this);

        AssetId existing_id = registry.find_by_path(path);
        if (existing_id.is_valid())
            return get_asset<TAsset>(existing_id);

        AssetId new_id = GenerateAssetId();
        AssetMetadata metadata;
        metadata.id = new_id;
        metadata.source_path = path;
        metadata.type = metadata_type_for<TAsset>();
        metadata.version = 0;
        metadata.state = AssetState::LoadingCpu;
        registry.register_asset(new_id, metadata);

        std::shared_ptr<TAsset> asset;
        if constexpr (std::is_same_v<TAsset, TextureAsset>)
        {
            asset = import_texture_from_path(path);
            if (asset)
                asset->id = new_id;
        }

        if (!asset)
        {
            registry.set_state(new_id, AssetState::Failed);
            return nullptr;
        }

        cache.insert(new_id, asset);
        registry.set_state(new_id, AssetState::ReadyCpu);
        pipeline().submit(new_id, PipelineStage::CpuOptimize);
        return asset;
    }

    template<typename TAsset>
    std::shared_ptr<TAsset> AssetSystem::get_asset(const AssetId& id)
    {
        if (!id.is_valid())
            return nullptr;
        return cache_for<TAsset>(*this).access(id);
    }

    template<typename TAsset>
    std::shared_ptr<TAsset> AssetSystem::resolve(const AssetHandle<TAsset>& handle)
    {
        if (!handle.is_valid())
            return nullptr;

        auto metadata = AssetRegistry::get_instance().get_metadata(handle.get_id());
        if (metadata && handle.get_generation() != metadata->version)
            return nullptr;

        return get_asset<TAsset>(handle.get_id());
    }

    void AssetSystem::queue_gpu_upload(const AssetId& id, UploadPriority priority)
    {
        if (!gpu_resource_system)
            return;
        gpu_resource_system->queue_upload(id, AssetRegistry::get_instance().get_type(id), priority);
    }

    void AssetSystem::enable_hot_reload(bool enable)
    {
        hot_reload_enabled = enable;
        if (file_watcher_ptr)
            file_watcher_ptr->set_enabled(enable);
    }

    bool AssetSystem::is_hot_reload_enabled() const
    {
        return hot_reload_enabled;
    }

    template std::shared_ptr<TextureAsset> AssetSystem::load_asset<TextureAsset>(const std::filesystem::path&);
    template std::shared_ptr<MeshAsset> AssetSystem::load_asset<MeshAsset>(const std::filesystem::path&);
    template std::shared_ptr<MaterialAsset> AssetSystem::load_asset<MaterialAsset>(const std::filesystem::path&);

    template std::shared_ptr<TextureAsset> AssetSystem::get_asset<TextureAsset>(const AssetId&);
    template std::shared_ptr<MeshAsset> AssetSystem::get_asset<MeshAsset>(const AssetId&);
    template std::shared_ptr<MaterialAsset> AssetSystem::get_asset<MaterialAsset>(const AssetId&);
    template std::shared_ptr<ModelAsset> AssetSystem::get_asset<ModelAsset>(const AssetId&);
    template std::shared_ptr<PointCloudAsset> AssetSystem::get_asset<PointCloudAsset>(const AssetId&);
    template std::shared_ptr<GaussianAsset> AssetSystem::get_asset<GaussianAsset>(const AssetId&);

    template std::shared_ptr<TextureAsset> AssetSystem::resolve<TextureAsset>(const AssetHandle<TextureAsset>&);
    template std::shared_ptr<MeshAsset> AssetSystem::resolve<MeshAsset>(const AssetHandle<MeshAsset>&);
    template std::shared_ptr<MaterialAsset> AssetSystem::resolve<MaterialAsset>(const AssetHandle<MaterialAsset>&);

} // namespace diverse
