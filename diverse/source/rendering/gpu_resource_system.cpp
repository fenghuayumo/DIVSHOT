#include "gpu_resource_system.h"
#include "assets/asset_registry.h"
#include "assets/asset_system.h"
#include "assets/material_asset.h"
#include "assets/material_properties.h"
#include "assets/point_cloud_asset.h"
#include "assets/gaussian_asset.h"
#include "assets/gaussian_model.h"
#include "core/base_type.h"
#include "mesh_gpu_utils.h"
#include "texture_gpu_utils.h"
#include "utility/file_utils.h"
#include <algorithm>
#include <cmath>

namespace diverse
{
    namespace
    {
        static constexpr uint32_t MAX_BINDLESS_TEXTURES = 16384;
        static constexpr size_t STAGING_BUFFER_SIZE = 16 * 1024 * 1024;
        static constexpr uint64_t MAX_FRAMES_IN_FLIGHT = 3;

        // Default texture IDs (must match BindlessTable fixed slots)
        static constexpr uint32_t WHITE_TEX_ID = BindlessTable::DEFAULT_WHITE;
        static constexpr uint32_t NORMAL_TEX_ID = BindlessTable::DEFAULT_NORMAL;

        size_t align_up(size_t value, size_t alignment)
        {
            return (value + alignment - 1) & ~(alignment - 1);
        }

        void subtract_usage(size_t& usage, size_t bytes)
        {
            usage = bytes > usage ? 0 : usage - bytes;
        }

        template<typename AssetType>
        void upsert_lru(
            std::vector<GpuLruEntry<AssetType>>& lru,
            const AssetId& id,
            uint64_t frame,
            size_t memory_size,
            ResidentPriority priority)
        {
            auto it = std::find_if(lru.begin(), lru.end(),
                [&id](const GpuLruEntry<AssetType>& entry) { return entry.id == id; });
            if (it != lru.end())
            {
                it->last_used_frame = frame;
                it->memory_size = memory_size;
                it->priority = priority;
                return;
            }

            lru.push_back({ id, frame, memory_size, priority });
        }
    }

    // StagingBuffer implementation
    StagingBuffer::StagingBuffer(rhi::GpuDevice* dev, size_t size)
        : device(dev)
        , size(size)
        , used_bytes(0)
        , mapped_ptr(nullptr)
    {
        rhi::GpuBufferDesc desc = rhi::GpuBufferDesc::new_cpu_to_gpu(
            size,
            rhi::BufferUsageFlags::TRANSFER_SRC
        );

        buffer = device->create_buffer(desc, "staging_buffer", nullptr);
    }

    StagingBuffer::~StagingBuffer()
    {
        if (mapped_ptr && buffer)
        {
            unmap();
        }
    }

    void* StagingBuffer::map()
    {
        if (!mapped_ptr && buffer)
        {
            mapped_ptr = buffer->map(device);
        }
        return mapped_ptr;
    }

    void StagingBuffer::unmap()
    {
        if (mapped_ptr && buffer)
        {
            buffer->unmap(device);
            mapped_ptr = nullptr;
        }
    }

    void StagingBuffer::reset()
    {
        used_bytes = 0;
    }

    size_t StagingBuffer::allocate(size_t bytes, size_t alignment)
    {
        size_t aligned_offset = align_up(used_bytes, alignment);
        if (aligned_offset + bytes > size)
        {
            return SIZE_MAX;  // Not enough space
        }
        used_bytes = aligned_offset + bytes;
        return aligned_offset;
    }

    void StagingBuffer::copy_data(size_t offset, const void* data, size_t bytes)
    {
        if (mapped_ptr && offset + bytes <= size)
        {
            memcpy(static_cast<u8*>(mapped_ptr) + offset, data, bytes);
        }
    }

    // GpuResourceSystem implementation
    GpuResourceSystem::GpuResourceSystem(rhi::GpuDevice* device)
        : device(device)
        , current_frame(0)
        , texture_memory_usage(0)
        , buffer_memory_usage(0)
    {
        bindless = std::make_unique<BindlessTable>(device, MAX_BINDLESS_TEXTURES);
    }

    GpuResourceSystem::~GpuResourceSystem()
    {
        release();
    }

    void GpuResourceSystem::set_budget_config(const BudgetConfig& config)
    {
        budget = config;
    }

    const BudgetConfig& GpuResourceSystem::get_budget_config() const
    {
        return budget;
    }

    void GpuResourceSystem::queue_upload(const AssetId& id, AssetType type, UploadPriority priority)
    {
        if (!id.is_valid() || type == AssetType::None)
            return;

        std::lock_guard lock(upload_queue_mutex);

        UploadRequest request;
        request.asset_id = id;
        request.asset_type = type;
        request.priority = priority;
        request.request_frame = current_frame;

        upload_queue.push(request);
    }

    void GpuResourceSystem::process_upload_queue(uint64_t frame)
    {
        current_frame = frame;

        std::vector<UploadRequest> requests;
        {
            std::lock_guard lock(upload_queue_mutex);
            while (!upload_queue.empty())
            {
                requests.push_back(upload_queue.top());
                upload_queue.pop();
            }
        }

        for (const auto& request : requests)
        {
            switch (request.asset_type)
            {
                case AssetType::Texture:
                    process_texture_upload(request.asset_id);
                    break;
                case AssetType::MeshModel:
                    process_mesh_upload(request.asset_id);
                    break;
                case AssetType::Material:
                    process_material_upload(request.asset_id);
                    break;
                case AssetType::PointCloud:
                    process_point_cloud_upload(request.asset_id);
                    break;
                case AssetType::Gaussian:
                    process_gaussian_upload(request.asset_id);
                    break;
                default:
                    break;
            }
        }
    }

    void GpuResourceSystem::process_texture_upload(const AssetId& id)
    {
        auto& registry = AssetRegistry::get_instance();
        auto metadata = registry.get_metadata(id);
        if (!metadata || metadata->type != AssetType::Texture)
            return;

        const uint32_t version = registry.get_version(id);
        {
            std::unique_lock lock(gpu_resources_mutex);
            auto it = texture_gpu_cache.find(id);
            if (it != texture_gpu_cache.end() && it->second.resident_version == version)
                return;
        }

        auto texture_asset = AssetSystem::get_instance().get_asset<TextureAsset>(id);
        if (!texture_asset)
            return;

        auto gpu_texture = upload_texture_asset(*texture_asset, device);
        if (!gpu_texture)
            return;

        const size_t gpu_memory_size = texture_asset->calculate_memory_size();

        {
            std::unique_lock lock(gpu_resources_mutex);

            TextureGpu gpu;
            gpu.texture = gpu_texture;
            gpu.gpu_memory_size = gpu_memory_size;
            gpu.resident_version = version;

            auto bindless_it = texture_bindless.find(id);
            if (bindless_it == texture_bindless.end())
            {
                auto handle = bindless->allocate_texture(gpu_texture.get());
                gpu.srv = BindlessImageHandle(handle.index);
                texture_bindless[id] = handle;
            }
            else
            {
                gpu.srv = BindlessImageHandle(bindless_it->second.index);
                bindless->update_texture(bindless_it->second, gpu_texture.get());
            }

            texture_gpu_cache[id] = gpu;

            GpuResourceEntry& entry = gpu_resources[id];
            if (entry.is_resident)
                subtract_usage(texture_memory_usage, entry.gpu_memory_size);
            entry.resource = gpu_texture;
            entry.gpu_version = gpu.resident_version;
            entry.last_used_frame = current_frame;
            entry.is_resident = true;
            entry.gpu_memory_size = gpu_memory_size;
            entry.priority = ResidentPriority::Normal;
            texture_memory_usage += gpu_memory_size;

            upsert_lru(texture_lru, id, current_frame, gpu_memory_size, ResidentPriority::Normal);
        }

        registry.set_state(id, AssetState::ResidentGpu);

        // Notify CPU cache that GPU upload is complete
        AssetSystem::get_instance().texture_cache().mark_gpu_uploaded(id);
    }

    void GpuResourceSystem::process_mesh_upload(const AssetId& id)
    {
        const uint32_t version = AssetRegistry::get_instance().get_version(id);
        {
            std::unique_lock lock(gpu_resources_mutex);
            auto it = mesh_gpu_cache.find(id);
            if (it != mesh_gpu_cache.end() && it->second.resident_version == version)
                return;
        }

        auto mesh_asset = AssetSystem::get_instance().get_asset<MeshAsset>(id);
        if (!mesh_asset)
            return;

        MeshUploadResult upload;
        if (!upload_mesh_asset(*mesh_asset, device, upload))
            return;

        {
            std::unique_lock lock(gpu_resources_mutex);

            MeshGpu gpu;
            gpu.vertex_buffer = upload.vertex_buffer;
            gpu.index_buffer = upload.index_buffer;
            gpu.vertex_pos_nor_offset = upload.vertex_pos_nor_offset;
            gpu.vertex_uv_offset = upload.vertex_uv_offset;
            gpu.vertex_tangent_offset = upload.vertex_tangent_offset;
            gpu.vertex_color_offset = upload.vertex_color_offset;
            gpu.vertex_count = static_cast<uint32_t>(mesh_asset->get_vertex_count());
            gpu.index_count = static_cast<uint32_t>(mesh_asset->get_index_count());
            gpu.resident_version = version;
            gpu.vertex_buffer_size = upload.vertex_buffer_size;
            gpu.index_buffer_size = upload.index_buffer_size;
            mesh_gpu_cache[id] = gpu;

            GpuResourceEntry& entry = gpu_resources[id];
            if (entry.is_resident)
                subtract_usage(buffer_memory_usage, entry.gpu_memory_size);
            entry.resource = upload.vertex_buffer;
            entry.gpu_version = gpu.resident_version;
            entry.last_used_frame = current_frame;
            entry.is_resident = true;
            entry.gpu_memory_size = gpu.vertex_buffer_size + gpu.index_buffer_size;
            entry.priority = ResidentPriority::Normal;
            buffer_memory_usage += entry.gpu_memory_size;

            upsert_lru(mesh_lru, id, current_frame, entry.gpu_memory_size, ResidentPriority::Normal);
        }

        AssetRegistry::get_instance().set_state(id, AssetState::ResidentGpu);

        // Notify CPU cache that GPU upload is complete
        AssetSystem::get_instance().mesh_cache().mark_gpu_uploaded(id);
    }

    void GpuResourceSystem::process_material_upload(const AssetId& id)
    {
        auto& registry = AssetRegistry::get_instance();
        auto metadata = registry.get_metadata(id);
        if (!metadata || metadata->type != AssetType::Material)
            return;

        auto material_asset = AssetSystem::get_instance().get_asset<MaterialAsset>(id);
        if (!material_asset)
            return;

        const uint32_t version = registry.get_version(id);
        {
            std::shared_lock lock(gpu_resources_mutex);
            auto it = material_gpu_cache.find(id);
            if (it != material_gpu_cache.end() && it->second.resident_version == version)
                return;
        }

        auto resolve_texture = [this](const AssetHandle<TextureAsset>& handle, uint32_t default_id) -> uint32_t {
            if (!handle.is_valid())
                return default_id;
            auto tex_gpu = request_texture(handle.get_id(), UploadPriority::Critical);
            return tex_gpu.srv.is_valid() ? tex_gpu.srv.index : default_id;
        };

        MaterialGpu gpu;
        gpu.resident_version = version;
        gpu.set_bindless_index(MaterialGpu::TEXTURE_SLOT_ALBEDO, resolve_texture(material_asset->albedo, WHITE_TEX_ID));
        gpu.set_bindless_index(MaterialGpu::TEXTURE_SLOT_NORMAL, resolve_texture(material_asset->normal, NORMAL_TEX_ID));
        gpu.set_bindless_index(MaterialGpu::TEXTURE_SLOT_METALLIC, resolve_texture(material_asset->metallic, WHITE_TEX_ID));
        gpu.set_bindless_index(MaterialGpu::TEXTURE_SLOT_ROUGHNESS, resolve_texture(material_asset->roughness, WHITE_TEX_ID));
        gpu.set_bindless_index(MaterialGpu::TEXTURE_SLOT_AO, resolve_texture(material_asset->ao, WHITE_TEX_ID));
        gpu.set_bindless_index(MaterialGpu::TEXTURE_SLOT_EMISSIVE, resolve_texture(material_asset->emissive, WHITE_TEX_ID));
        gpu.set_bindless_index(MaterialGpu::TEXTURE_SLOT_TRANSMISSION, WHITE_TEX_ID);
        gpu.set_bindless_index(MaterialGpu::TEXTURE_SLOT_NORMAL_DETAIL, NORMAL_TEX_ID);

        MaterialProperties props = material_asset->properties;
        update_material_properties_bindless_indices(props, gpu);

        uint32_t slot = 0;
        {
            std::unique_lock lock(gpu_resources_mutex);
            auto it = material_gpu_cache.find(id);
            if (it == material_gpu_cache.end())
            {
                if (material_count >= material_capacity)
                    return;
                slot = material_count++;
            }
            else
            {
                slot = it->second.material_buffer_index;
            }

            gpu.material_buffer_index = slot;
            material_gpu_cache[id] = gpu;
        }

        if (material_buffer && slot < material_capacity)
        {
            auto* material_data = reinterpret_cast<MaterialProperties*>(material_buffer->map(device));
            if (material_data)
            {
                material_data[slot] = props;
                material_buffer->unmap(device);
            }
        }

        AssetSystem::get_instance().material_cache().mark_gpu_uploaded(id);
        registry.set_state(id, AssetState::ResidentGpu);
    }

    void GpuResourceSystem::process_point_cloud_upload(const AssetId& id)
    {
        auto& registry = AssetRegistry::get_instance();
        auto metadata = registry.get_metadata(id);
        if (!metadata || metadata->type != AssetType::PointCloud)
            return;

        const uint32_t version = registry.get_version(id);
        {
            std::unique_lock lock(gpu_resources_mutex);
            auto it = point_cloud_gpu_cache.find(id);
            if (it != point_cloud_gpu_cache.end() && it->second.resident_version == version)
                return;
        }

        auto point_asset = AssetSystem::get_instance().get_asset<PointCloudAsset>(id);
        if (!point_asset || point_asset->vertices.empty())
            return;

        auto vertex_desc = rhi::GpuBufferDesc::new_gpu_only(
            point_asset->vertices.size() * sizeof(PointCloudVertex),
            rhi::BufferUsageFlags::STORAGE_BUFFER
            | rhi::BufferUsageFlags::SHADER_DEVICE_ADDRESS
            | rhi::BufferUsageFlags::VERTEX_BUFFER
            | rhi::BufferUsageFlags::TRANSFER_DST);
        auto vertex_buffer = device->create_buffer(
            vertex_desc,
            "point_vert_buf",
            reinterpret_cast<u8*>(point_asset->vertices.data()));
        if (!vertex_buffer)
            return;

        const size_t gpu_memory_size = point_asset->vertices.size() * sizeof(PointCloudVertex);

        PointCloudGpu gpu;
        gpu.vertex_buffer = vertex_buffer;
        gpu.resident_version = version;
        gpu.gpu_memory_size = gpu_memory_size;

        uint32_t slot = 0;
        {
            std::unique_lock lock(gpu_resources_mutex);
            auto it = point_cloud_gpu_cache.find(id);
            if (it == point_cloud_gpu_cache.end())
                slot = next_point_cloud_slot++;
            else
                slot = it->second.bindless_slot;

            gpu.bindless_slot = slot;
            point_cloud_gpu_cache[id] = gpu;

            GpuResourceEntry& entry = gpu_resources[id];
            if (entry.is_resident)
                subtract_usage(buffer_memory_usage, entry.gpu_memory_size);
            entry.resource = vertex_buffer;
            entry.gpu_version = gpu.resident_version;
            entry.last_used_frame = current_frame;
            entry.is_resident = true;
            entry.gpu_memory_size = gpu_memory_size;
            entry.priority = ResidentPriority::Normal;
            buffer_memory_usage += gpu_memory_size;

            upsert_lru(point_cloud_lru, id, current_frame, gpu_memory_size, ResidentPriority::Normal);
        }

        bind_point_cloud_to_slot(slot, gpu);
        AssetSystem::get_instance().point_cloud_cache().mark_gpu_uploaded(id);
        registry.set_state(id, AssetState::ResidentGpu);
    }

    void GpuResourceSystem::process_gaussian_upload(const AssetId& id)
    {
        auto gaussian_asset = AssetSystem::get_instance().get_asset<GaussianAsset>(id);
        if (!gaussian_asset || gaussian_asset->pos.empty())
            return;

        auto& registry = AssetRegistry::get_instance();
        const uint32_t version = registry.get_version(id);
        {
            std::unique_lock lock(gpu_resources_mutex);
            auto it = gaussian_gpu_cache.find(id);
            if (it != gaussian_gpu_cache.end() && it->second.resident_version == version)
                return;
        }

        auto packed = pack_gaussian_asset(*gaussian_asset);
        const GaussianBufferUpload* existing = nullptr;
        {
            std::shared_lock lock(gpu_resources_mutex);
            auto it = gaussian_buffer_uploads.find(id);
            if (it != gaussian_buffer_uploads.end())
                existing = &it->second;
        }

        auto upload = upload_gaussian_buffers(
            device,
            packed,
            existing,
            gaussian_asset->pos.size(),
            10000,
            true,
            gaussian_asset->splat_state,
            gaussian_asset->splat_transform_index);

        if (!upload.gaussians_buf)
            return;

        uint32_t slot = 0;
        GaussianGpu gpu;
        {
            std::unique_lock lock(gpu_resources_mutex);
            auto it = gaussian_gpu_cache.find(id);
            if (it == gaussian_gpu_cache.end())
                slot = next_gaussian_slot++;
            else
                slot = it->second.bindless_slot;

            auto resource_it = gpu_resources.find(id);
            if (resource_it != gpu_resources.end() && resource_it->second.is_resident)
                subtract_usage(buffer_memory_usage, resource_it->second.gpu_memory_size);

            gaussian_buffer_uploads[id] = upload;
            gpu = make_gaussian_gpu(upload, version, slot);
            gaussian_gpu_cache[id] = gpu;

            GpuResourceEntry& entry = gpu_resources[id];
            entry.resource = upload.gaussians_buf;
            entry.gpu_version = gpu.resident_version;
            entry.last_used_frame = current_frame;
            entry.is_resident = true;
            entry.gpu_memory_size = upload.gpu_memory_size;
            entry.priority = ResidentPriority::Normal;
            buffer_memory_usage += upload.gpu_memory_size;

            upsert_lru(gaussian_lru, id, current_frame, upload.gpu_memory_size, ResidentPriority::Normal);
        }

        bind_gaussian_to_slot(slot, gpu, nullptr);
        AssetSystem::get_instance().gaussian_cache().mark_gpu_uploaded(id);
        registry.set_state(id, AssetState::ResidentGpu);
    }

    GaussianGpu GpuResourceSystem::upload_gaussian_from_model(GaussianModel& model, bool compact)
    {
        if (!device || model.position().empty())
            return {};

        if (!model.get_asset_id().is_valid())
            return {};

        const AssetId asset_id = model.get_asset_id();
        GaussianAsset snapshot;
        snapshot.id = asset_id;
        snapshot.source_path = model.get_file_path();
        snapshot.pos = model.position();
        snapshot.shs_0 = model.sh0();
        snapshot.shs_n = model.shn();
        snapshot.opacities = model.opacity();
        snapshot.scales = model.scale();
        snapshot.rot = model.rotation();
        snapshot.splat_state = model.state();
        snapshot.splat_select_flag = model.flags();
        snapshot.splat_transform_index = model.transform_index();

        auto packed = pack_gaussian_asset(snapshot);
        const GaussianBufferUpload* existing = nullptr;
        {
            std::shared_lock lock(gpu_resources_mutex);
            auto it = gaussian_buffer_uploads.find(asset_id);
            if (it != gaussian_buffer_uploads.end())
                existing = &it->second;
        }

        auto upload = upload_gaussian_buffers(
            device,
            packed,
            existing,
            model.position().size(),
            model.get_max_splats(),
            compact,
            model.state(),
            model.transform_index());

        if (!upload.gaussians_buf)
            return {};

        auto& registry = AssetRegistry::get_instance();
        const uint32_t version = registry.get_version(asset_id);
        uint32_t slot = 0;
        GaussianGpu gpu;
        {
            std::unique_lock lock(gpu_resources_mutex);
            auto it = gaussian_gpu_cache.find(asset_id);
            if (it == gaussian_gpu_cache.end())
                slot = next_gaussian_slot++;
            else
                slot = it->second.bindless_slot;

            auto resource_it = gpu_resources.find(asset_id);
            if (resource_it != gpu_resources.end() && resource_it->second.is_resident)
                subtract_usage(buffer_memory_usage, resource_it->second.gpu_memory_size);

            gaussian_buffer_uploads[asset_id] = upload;
            gpu = make_gaussian_gpu(upload, version, slot);
            gaussian_gpu_cache[asset_id] = gpu;

            GpuResourceEntry& entry = gpu_resources[asset_id];
            entry.resource = upload.gaussians_buf;
            entry.gpu_version = gpu.resident_version;
            entry.last_used_frame = current_frame;
            entry.is_resident = true;
            entry.gpu_memory_size = upload.gpu_memory_size;
            entry.priority = ResidentPriority::Normal;
            buffer_memory_usage += upload.gpu_memory_size;

            upsert_lru(gaussian_lru, asset_id, current_frame, upload.gpu_memory_size, ResidentPriority::Normal);
        }

        bind_gaussian_to_slot(slot, gpu, model.splat_transforms.splat_transform_buffer.get());
        AssetSystem::get_instance().gaussian_cache().mark_gpu_uploaded(asset_id);
        registry.set_state(asset_id, AssetState::ResidentGpu);
        return gpu;
    }

    GaussianGpu GpuResourceSystem::get_gaussian_gpu(const AssetId& id) const
    {
        std::shared_lock lock(gpu_resources_mutex);
        auto it = gaussian_gpu_cache.find(id);
        return it != gaussian_gpu_cache.end() ? it->second : GaussianGpu{};
    }

    void GpuResourceSystem::make_resident(const AssetId& id, ResidentPriority priority)
    {
        std::unique_lock lock(gpu_resources_mutex);

        auto it = gpu_resources.find(id);
        if (it != gpu_resources.end())
        {
            it->second.is_resident = true;
            it->second.priority = priority;
            it->second.last_used_frame = current_frame;
        }
    }

    void GpuResourceSystem::make_evictable(const AssetId& id)
    {
        std::unique_lock lock(gpu_resources_mutex);

        auto it = gpu_resources.find(id);
        if (it != gpu_resources.end())
        {
            it->second.priority = ResidentPriority::Low;
        }
    }

    bool GpuResourceSystem::check_budget(size_t required_bytes, AssetType type) const
    {
        size_t current_usage = (type == AssetType::Texture) ?
            calculate_total_texture_usage() : calculate_total_buffer_usage();

        size_t budget_mb = (type == AssetType::Texture) ?
            budget.total_texture_budget_mb : budget.total_buffer_budget_mb;

        size_t budget_bytes = budget_mb * 1024 * 1024;

        // Allow if under high watermark
        return (current_usage + required_bytes) <= (budget_bytes * budget.high_watermark);
    }

    void GpuResourceSystem::enforce_budget()
    {
        // Check if we're over budget
        size_t texture_usage = calculate_total_texture_usage();
        size_t texture_budget = budget.total_texture_budget_mb * 1024 * 1024;

        if (texture_usage > texture_budget * budget.high_watermark)
        {
            evict_textures_to_budget();
        }

        size_t buffer_usage = calculate_total_buffer_usage();
        size_t buffer_budget = budget.total_buffer_budget_mb * 1024 * 1024;

        if (buffer_usage > buffer_budget * budget.high_watermark)
        {
            evict_buffers_to_budget();
        }
    }

    void GpuResourceSystem::evict_textures_to_budget()
    {
        size_t target = budget.total_texture_budget_mb * 1024 * 1024 * budget.low_watermark;
        size_t current = calculate_total_texture_usage();

        if (current <= target)
        {
            return;
        }

        {
            std::unique_lock lock(gpu_resources_mutex);

            // Sort LRU entries by last access (oldest first)
            std::sort(texture_lru.begin(), texture_lru.end(),
                [](const auto& a, const auto& b)
                {
                    // Critical and High priority assets are never evicted
                    if (a.priority <= ResidentPriority::High) return false;
                    if (b.priority <= ResidentPriority::High) return true;
                    return a.last_used_frame < b.last_used_frame;
                });
        }

        std::vector<AssetId> evict_ids;
        {
            std::shared_lock lock(gpu_resources_mutex);
            for (const auto& entry : texture_lru)
            {
                if (current <= target)
                    break;

                auto it = gpu_resources.find(entry.id);
                if (it != gpu_resources.end() &&
                    it->second.priority > ResidentPriority::High &&
                    it->second.is_resident)
                {
                    if (std::find(evict_ids.begin(), evict_ids.end(), entry.id) == evict_ids.end())
                    {
                        evict_ids.push_back(entry.id);
                        current -= it->second.gpu_memory_size;
                    }
                }
            }
        }

        for (const auto& id : evict_ids)
            retire_asset_gpu(id, current_frame);
    }

    void GpuResourceSystem::evict_buffers_to_budget()
    {
        // Similar to texture eviction but for buffers
        size_t target = budget.total_buffer_budget_mb * 1024 * 1024 * budget.low_watermark;
        size_t current = calculate_total_buffer_usage();

        if (current <= target)
        {
            return;
        }

        {
            std::unique_lock lock(gpu_resources_mutex);

            std::sort(mesh_lru.begin(), mesh_lru.end(),
                [](const auto& a, const auto& b)
                {
                    if (a.priority <= ResidentPriority::High) return false;
                    if (b.priority <= ResidentPriority::High) return true;
                    return a.last_used_frame < b.last_used_frame;
                });
        }

        std::vector<AssetId> evict_ids;
        {
            std::shared_lock lock(gpu_resources_mutex);
            for (const auto& entry : mesh_lru)
            {
                if (current <= target)
                    break;

                auto it = gpu_resources.find(entry.id);
                if (it != gpu_resources.end() &&
                    it->second.priority > ResidentPriority::High &&
                    it->second.is_resident)
                {
                    if (std::find(evict_ids.begin(), evict_ids.end(), entry.id) == evict_ids.end())
                    {
                        evict_ids.push_back(entry.id);
                        current -= it->second.gpu_memory_size;
                    }
                }
            }
        }

        for (const auto& id : evict_ids)
            retire_asset_gpu(id, current_frame);
    }

    size_t GpuResourceSystem::calculate_total_texture_usage() const
    {
        std::shared_lock lock(gpu_resources_mutex);
        return texture_memory_usage;
    }

    size_t GpuResourceSystem::calculate_total_buffer_usage() const
    {
        std::shared_lock lock(gpu_resources_mutex);
        return buffer_memory_usage;
    }

    BindlessImageHandle GpuResourceSystem::allocate_bindless_slot(rhi::GpuTexture* texture)
    {
        if (!texture || !bindless)
            return {};
        auto handle = bindless->allocate_texture(texture);
        return BindlessImageHandle(handle.index);
    }

    void GpuResourceSystem::update_bindless_descriptor(BindlessImageHandle handle, rhi::GpuTexture* texture)
    {
        if (!handle.is_valid() || !texture || !bindless)
            return;
        bindless->update_texture(BindlessHandle{ handle.index, 0 }, texture);
    }

    void GpuResourceSystem::free_bindless_slot(BindlessImageHandle handle)
    {
        if (!handle.is_valid() || !bindless)
            return;
        bindless->free_later(BindlessHandle{ handle.index, 0 }, current_frame + MAX_FRAMES_IN_FLIGHT);
    }

    void GpuResourceSystem::attach_bindless_descriptor_set(rhi::DescriptorSet* set, uint32_t texture_binding_id, uint32_t size_binding_id)
    {
        bindless_descriptor_set = set;
        if (bindless)
        {
            bindless->set_descriptor_set(set, texture_binding_id, bindless->get_size_buffer(), size_binding_id);
            if (bindless_descriptor_set && device)
            {
                if (auto* size_buffer = bindless->get_size_buffer())
                    device->write_descriptor_set(bindless_descriptor_set, size_binding_id, size_buffer);
            }
        }
    }

    void GpuResourceSystem::attach_mesh_buffer_bindings(uint32_t vertex_binding_id, uint32_t index_binding_id)
    {
        vertex_buffer_binding_id = vertex_binding_id;
        index_buffer_binding_id = index_binding_id;
    }

    void GpuResourceSystem::bind_mesh_to_slot(uint32_t slot, const MeshGpu& mesh)
    {
        if (!bindless_descriptor_set || !device || !mesh.is_valid())
            return;

        device->write_descriptor_set(bindless_descriptor_set, vertex_buffer_binding_id, mesh.vertex_buffer.get(), slot);
        device->write_descriptor_set(bindless_descriptor_set, index_buffer_binding_id, mesh.index_buffer.get(), slot);
    }

    void GpuResourceSystem::bind_point_cloud_to_slot(uint32_t slot, const PointCloudGpu& point_cloud)
    {
        if (!bindless_descriptor_set || !device || !point_cloud.is_valid())
            return;
        if (point_cloud_buffer_binding_id == 0xFFFFFFFF)
            return;

        device->write_descriptor_set(
            bindless_descriptor_set,
            point_cloud_buffer_binding_id,
            point_cloud.vertex_buffer.get(),
            slot);
    }

    void GpuResourceSystem::attach_material_buffer_binding(uint32_t binding_id)
    {
        material_buffer_binding_id = binding_id;
        if (material_buffer && bindless_descriptor_set && device)
            device->write_descriptor_set(bindless_descriptor_set, binding_id, material_buffer.get());
    }

    void GpuResourceSystem::attach_point_cloud_buffer_binding(uint32_t binding_id)
    {
        point_cloud_buffer_binding_id = binding_id;
    }

    void GpuResourceSystem::attach_gaussian_buffer_bindings(uint32_t gs_binding_id, uint32_t splat_state_binding_id)
    {
        gaussian_buffer_binding_id = gs_binding_id;
        gaussian_state_binding_id = splat_state_binding_id;
    }

    void GpuResourceSystem::bind_gaussian_to_slot(uint32_t slot, const GaussianGpu& gaussian, rhi::GpuBuffer* splat_transform_buffer)
    {
        if (!bindless_descriptor_set || !device || !gaussian.is_valid())
            return;
        if (gaussian_buffer_binding_id == 0xFFFFFFFF || gaussian_state_binding_id == 0xFFFFFFFF)
            return;

        device->write_descriptor_set(bindless_descriptor_set, gaussian_buffer_binding_id, gaussian.gaussians_buf.get(), slot * 4 + 0);
        device->write_descriptor_set(bindless_descriptor_set, gaussian_buffer_binding_id, gaussian.sh_0_buf.get(), slot * 4 + 1);
        device->write_descriptor_set(bindless_descriptor_set, gaussian_buffer_binding_id, gaussian.sh_n_buf.get(), slot * 4 + 2);
        if (splat_transform_buffer)
            device->write_descriptor_set(bindless_descriptor_set, gaussian_buffer_binding_id, splat_transform_buffer, slot * 4 + 3);
        device->write_descriptor_set(bindless_descriptor_set, gaussian_state_binding_id, gaussian.state_buf.get(), slot);
    }

    void GpuResourceSystem::flush_bindless_updates()
    {
        if (bindless)
            bindless->flush_descriptor_updates();
    }

    BindlessImageHandle GpuResourceSystem::ensure_texture_at_slot(const AssetId& id, uint32_t slot)
    {
        if (!id.is_valid() || !bindless || slot >= bindless->capacity())
            return {};

        {
            std::shared_lock lock(gpu_resources_mutex);
            auto cached = texture_gpu_cache.find(id);
            if (cached != texture_gpu_cache.end() && cached->second.srv.is_valid())
                return cached->second.srv;
        }

        auto texture_asset = AssetSystem::get_instance().get_asset<TextureAsset>(id);
        if (!texture_asset)
            return {};

        auto gpu_texture = upload_texture_asset(*texture_asset, device);
        if (!gpu_texture)
            return {};

        auto bh = bindless->reserve_slot(slot);
        bindless->update_texture(bh, gpu_texture.get());

        std::unique_lock lock(gpu_resources_mutex);
        texture_bindless[id] = bh;

        TextureGpu gpu;
        gpu.texture = gpu_texture;
        gpu.srv = BindlessImageHandle(slot);
        gpu.gpu_memory_size = texture_asset->calculate_memory_size();
        gpu.resident_version = AssetRegistry::get_instance().get_version(id);
        texture_gpu_cache[id] = gpu;

        GpuResourceEntry& entry = gpu_resources[id];
        entry.resource = gpu_texture;
        entry.gpu_version = gpu.resident_version;
        entry.last_used_frame = current_frame;
        entry.is_resident = true;
        entry.gpu_memory_size = gpu.gpu_memory_size;
        entry.priority = ResidentPriority::Critical;
        texture_memory_usage += gpu.gpu_memory_size;

        AssetRegistry::get_instance().set_state(id, AssetState::ResidentGpu);
        flush_bindless_updates();
        return gpu.srv;
    }

    void GpuResourceSystem::ensure_default_texture_slots()
    {
        auto& sys = AssetSystem::get_instance();
        if (auto white = sys.get_default_white_texture())
            ensure_texture_at_slot(white->id, BindlessTable::DEFAULT_WHITE);
        if (auto normal = sys.get_default_normal_texture())
            ensure_texture_at_slot(normal->id, BindlessTable::DEFAULT_NORMAL);
    }

    BindlessImageHandle GpuResourceSystem::bind_texture_at_slot(rhi::GpuTexture* texture, uint32_t slot)
    {
        if (!texture || !bindless || slot >= bindless->capacity())
            return {};

        auto bh = bindless->reserve_slot(slot);
        bindless->update_texture(bh, texture);
        flush_bindless_updates();
        return BindlessImageHandle(slot);
    }

    TextureGpu GpuResourceSystem::request_texture(const AssetId& id, UploadPriority priority)
    {
        (void)priority;
        {
            std::unique_lock lock(gpu_resources_mutex);
            auto it = texture_gpu_cache.find(id);
            if (it != texture_gpu_cache.end())
            {
                // Update last_used_frame in LRU entry
                auto lru_it = std::find_if(texture_lru.begin(), texture_lru.end(),
                    [&id](const GpuLruEntry<TextureAsset>& e) { return e.id == id; });
                if (lru_it != texture_lru.end())
                {
                    lru_it->last_used_frame = current_frame;
                }
                return it->second;
            }
        }
        process_texture_upload(id);
        flush_bindless_updates();
        std::shared_lock lock(gpu_resources_mutex);
        auto found = texture_gpu_cache.find(id);
        return found != texture_gpu_cache.end() ? found->second : TextureGpu{};
    }

    MeshGpu GpuResourceSystem::request_mesh(const AssetId& id, UploadPriority priority)
    {
        (void)priority;
        {
            std::unique_lock lock(gpu_resources_mutex);
            auto it = mesh_gpu_cache.find(id);
            if (it != mesh_gpu_cache.end())
            {
                // Update last_used_frame in LRU entry
                auto lru_it = std::find_if(mesh_lru.begin(), mesh_lru.end(),
                    [&id](const GpuLruEntry<MeshAsset>& e) { return e.id == id; });
                if (lru_it != mesh_lru.end())
                {
                    lru_it->last_used_frame = current_frame;
                }
                return it->second;
            }
        }
        process_mesh_upload(id);
        std::shared_lock lock(gpu_resources_mutex);
        auto found = mesh_gpu_cache.find(id);
        return found != mesh_gpu_cache.end() ? found->second : MeshGpu{};
    }

    MaterialGpu GpuResourceSystem::request_material(const AssetId& id, UploadPriority priority)
    {
        (void)priority;
        if (!id.is_valid())
            return MaterialGpu{};

        process_material_upload(id);

        std::shared_lock lock(gpu_resources_mutex);
        auto found = material_gpu_cache.find(id);
        return found != material_gpu_cache.end() ? found->second : MaterialGpu{};
    }

    PointCloudGpu GpuResourceSystem::request_point_cloud(const AssetId& id, UploadPriority priority)
    {
        (void)priority;
        {
            std::unique_lock lock(gpu_resources_mutex);
            auto it = point_cloud_gpu_cache.find(id);
            if (it != point_cloud_gpu_cache.end())
            {
                // Update last_used_frame in LRU entry
                auto lru_it = std::find_if(point_cloud_lru.begin(), point_cloud_lru.end(),
                    [&id](const GpuLruEntry<PointCloudAsset>& e) { return e.id == id; });
                if (lru_it != point_cloud_lru.end())
                {
                    lru_it->last_used_frame = current_frame;
                }
                return it->second;
            }
        }
        process_point_cloud_upload(id);
        std::shared_lock lock(gpu_resources_mutex);
        auto found = point_cloud_gpu_cache.find(id);
        return found != point_cloud_gpu_cache.end() ? found->second : PointCloudGpu{};
    }

    GaussianGpu GpuResourceSystem::request_gaussian(const AssetId& id, UploadPriority priority)
    {
        (void)priority;
        {
            std::unique_lock lock(gpu_resources_mutex);
            auto it = gaussian_gpu_cache.find(id);
            if (it != gaussian_gpu_cache.end())
            {
                // Update last_used_frame in LRU entry
                auto lru_it = std::find_if(gaussian_lru.begin(), gaussian_lru.end(),
                    [&id](const GpuLruEntry<GaussianAsset>& e) { return e.id == id; });
                if (lru_it != gaussian_lru.end())
                {
                    lru_it->last_used_frame = current_frame;
                }
                return it->second;
            }
        }
        process_gaussian_upload(id);
        std::shared_lock lock(gpu_resources_mutex);
        auto found = gaussian_gpu_cache.find(id);
        return found != gaussian_gpu_cache.end() ? found->second : GaussianGpu{};
    }

    void GpuResourceSystem::enqueue_uploads(uint64_t frame_index)
    {
        process_upload_queue(frame_index);
        flush_bindless_updates();
    }

    void GpuResourceSystem::retire(uint64_t completed_frame)
    {
        process_deferred_releases(completed_frame);
        if (bindless)
            bindless->process_deferred_frees(completed_frame);
    }

    rhi::DescriptorSet* GpuResourceSystem::get_bindless_descriptor_set() const
    {
        return bindless_descriptor_set;
    }

    StagingBuffer* GpuResourceSystem::acquire_staging_buffer(size_t size)
    {
        std::lock_guard lock(staging_mutex);

        // Try to find a free buffer with enough space
        for (auto& entry : staging_buffers)
        {
            if (entry.buffer && entry.buffer->has_space(size))
            {
                entry.last_used_frame = current_frame;
                return entry.buffer.get();
            }
        }

        // Create new staging buffer if needed
        size_t buffer_size = std::max(size, STAGING_BUFFER_SIZE);
        auto staging_buffer = std::make_shared<StagingBuffer>(device, buffer_size);

        StagingBufferPoolEntry entry;
        entry.buffer = staging_buffer;
        entry.last_used_frame = current_frame;
        entry.size = buffer_size;

        staging_buffers.push_back(std::move(entry));

        return staging_buffers.back().buffer.get();
    }

    void GpuResourceSystem::return_staging_buffer(StagingBuffer* buffer)
    {
        if (!buffer)
        {
            return;
        }

        // Staging buffers are managed via frame-based cleanup
        // Just update last used frame
        std::lock_guard lock(staging_mutex);

        for (auto& entry : staging_buffers)
        {
            if (entry.buffer.get() == buffer)
            {
                entry.last_used_frame = current_frame;
                break;
            }
        }
    }

    void GpuResourceSystem::defer_release(std::shared_ptr<rhi::GpuResource> resource, uint64_t release_frame, const AssetId& asset_id)
    {
        std::lock_guard lock(deferred_release_mutex);

        DeferredRelease entry;
        entry.resource = resource;
        entry.release_frame = release_frame;
        entry.associated_asset = asset_id;

        deferred_releases.push_back(entry);
    }

    void GpuResourceSystem::process_deferred_releases(uint64_t completed_frame)
    {
        std::lock_guard lock(deferred_release_mutex);

        auto it = std::remove_if(deferred_releases.begin(), deferred_releases.end(),
            [completed_frame](const DeferredRelease& entry)
            {
                return entry.release_frame <= completed_frame;
            });

        // Resources are automatically released when removed from vector
        deferred_releases.erase(it, deferred_releases.end());
    }

    void GpuResourceSystem::retire_asset_gpu(const AssetId& id, uint64_t completed_frame)
    {
        const uint64_t release_frame = completed_frame + MAX_FRAMES_IN_FLIGHT;

        std::unique_lock lock(gpu_resources_mutex);

        const auto tex_it = texture_gpu_cache.find(id);
        if (tex_it != texture_gpu_cache.end())
        {
            const auto bindless_it = texture_bindless.find(id);
            if (bindless && bindless_it != texture_bindless.end())
                bindless->free_later(bindless_it->second, release_frame);

            texture_bindless.erase(id);

            if (tex_it->second.texture)
            {
                defer_release(
                    std::static_pointer_cast<rhi::GpuResource>(tex_it->second.texture),
                    release_frame,
                    id);
            }

            subtract_usage(texture_memory_usage, tex_it->second.gpu_memory_size);
            texture_gpu_cache.erase(tex_it);
        }

        const auto mesh_it = mesh_gpu_cache.find(id);
        if (mesh_it != mesh_gpu_cache.end())
        {
            if (mesh_it->second.vertex_buffer)
            {
                defer_release(
                    std::static_pointer_cast<rhi::GpuResource>(mesh_it->second.vertex_buffer),
                    release_frame,
                    id);
            }

            if (mesh_it->second.index_buffer)
            {
                defer_release(
                    std::static_pointer_cast<rhi::GpuResource>(mesh_it->second.index_buffer),
                    release_frame,
                    id);
            }

            subtract_usage(buffer_memory_usage, mesh_it->second.vertex_buffer_size + mesh_it->second.index_buffer_size);
            mesh_gpu_cache.erase(mesh_it);
        }

        const auto point_it = point_cloud_gpu_cache.find(id);
        if (point_it != point_cloud_gpu_cache.end())
        {
            if (point_it->second.vertex_buffer)
            {
                defer_release(
                    std::static_pointer_cast<rhi::GpuResource>(point_it->second.vertex_buffer),
                    release_frame,
                    id);
            }

            subtract_usage(buffer_memory_usage, point_it->second.gpu_memory_size);
            point_cloud_gpu_cache.erase(point_it);
        }

        const auto gaussian_it = gaussian_gpu_cache.find(id);
        if (gaussian_it != gaussian_gpu_cache.end())
        {
            const std::shared_ptr<rhi::GpuBuffer> buffers[] = {
                gaussian_it->second.gaussians_buf,
                gaussian_it->second.sh_0_buf,
                gaussian_it->second.sh_n_buf,
                gaussian_it->second.state_buf,
                gaussian_it->second.points_key_buf,
                gaussian_it->second.points_value_buf,
                gaussian_it->second.splat_transforms
            };

            for (const auto& buffer : buffers)
            {
                if (buffer)
                {
                    defer_release(
                        std::static_pointer_cast<rhi::GpuResource>(buffer),
                        release_frame,
                        id);
                }
            }

            subtract_usage(buffer_memory_usage, gaussian_it->second.gpu_memory_size);
            gaussian_gpu_cache.erase(gaussian_it);
            gaussian_buffer_uploads.erase(id);
        }

        gpu_resources.erase(id);

        texture_lru.erase(
            std::remove_if(texture_lru.begin(), texture_lru.end(),
                [&id](const GpuLruEntry<TextureAsset>& entry) { return entry.id == id; }),
            texture_lru.end());

        mesh_lru.erase(
            std::remove_if(mesh_lru.begin(), mesh_lru.end(),
                [&id](const GpuLruEntry<MeshAsset>& entry) { return entry.id == id; }),
            mesh_lru.end());

        point_cloud_lru.erase(
            std::remove_if(point_cloud_lru.begin(), point_cloud_lru.end(),
                [&id](const GpuLruEntry<PointCloudAsset>& entry) { return entry.id == id; }),
            point_cloud_lru.end());

        gaussian_lru.erase(
            std::remove_if(gaussian_lru.begin(), gaussian_lru.end(),
                [&id](const GpuLruEntry<GaussianAsset>& entry) { return entry.id == id; }),
            gaussian_lru.end());
    }

    void GpuResourceSystem::reload_asset(const AssetId& id)
    {
        retire_asset_gpu(id, current_frame);

        queue_upload(id, AssetRegistry::get_instance().get_type(id), UploadPriority::High);
    }

    uint32_t GpuResourceSystem::get_gpu_version(const AssetId& id) const
    {
        std::shared_lock lock(gpu_resources_mutex);

        auto it = gpu_resources.find(id);
        if (it != gpu_resources.end())
        {
            return it->second.gpu_version;
        }
        return 0;
    }

    std::shared_ptr<rhi::GpuResource> GpuResourceSystem::get_gpu_resource(const AssetId& id) const
    {
        std::shared_lock lock(gpu_resources_mutex);

        auto it = gpu_resources.find(id);
        if (it != gpu_resources.end())
        {
            return it->second.resource;
        }
        return nullptr;
    }

    TextureGpu GpuResourceSystem::get_texture_gpu(const AssetId& id) const
    {
        std::shared_lock lock(gpu_resources_mutex);
        auto it = texture_gpu_cache.find(id);
        return it != texture_gpu_cache.end() ? it->second : TextureGpu{};
    }

    MeshGpu GpuResourceSystem::get_mesh_gpu(const AssetId& id) const
    {
        std::shared_lock lock(gpu_resources_mutex);
        auto it = mesh_gpu_cache.find(id);
        return it != mesh_gpu_cache.end() ? it->second : MeshGpu{};
    }

    size_t GpuResourceSystem::get_texture_memory_usage() const
    {
        return calculate_total_texture_usage();
    }

    size_t GpuResourceSystem::get_buffer_memory_usage() const
    {
        return calculate_total_buffer_usage();
    }

    size_t GpuResourceSystem::get_total_memory_usage() const
    {
        return get_texture_memory_usage() + get_buffer_memory_usage();
    }

    uint32_t GpuResourceSystem::get_bindless_slot_count() const
    {
        return bindless ? bindless->live_count() : 0;
    }

    void GpuResourceSystem::update(uint64_t frame)
    {
        current_frame = frame;
        retire(frame);
        flush_bindless_updates();

        // Clean up old staging buffers
        {
            std::lock_guard lock(staging_mutex);
            staging_buffers.erase(
                std::remove_if(staging_buffers.begin(), staging_buffers.end(),
                    [frame](const StagingBufferPoolEntry& entry)
                    {
                        // Keep buffers used in the last few frames
                        return (frame - entry.last_used_frame) > MAX_FRAMES_IN_FLIGHT;
                    }),
                staging_buffers.end());
        }

        // Enforce budget
        enforce_budget();
    }

    void GpuResourceSystem::initialize_material_buffer(uint32_t capacity)
    {
        std::unique_lock lock(gpu_resources_mutex);

        material_capacity = capacity;
        material_count = 0;

        rhi::GpuBufferDesc mat_buffer_desc = rhi::GpuBufferDesc::new_cpu_to_gpu(
            capacity * sizeof(MaterialProperties),
            rhi::BufferUsageFlags::STORAGE_BUFFER |
            rhi::BufferUsageFlags::SHADER_DEVICE_ADDRESS |
            rhi::BufferUsageFlags::TRANSFER_DST
        );

        material_buffer = device->create_buffer(mat_buffer_desc, "material buffer", nullptr);

        if (material_buffer && bindless_descriptor_set && material_buffer_binding_id != 0xFFFFFFFF)
            device->write_descriptor_set(bindless_descriptor_set, material_buffer_binding_id, material_buffer.get());
    }

    void GpuResourceSystem::upload_material_data(const AssetId& id, const MaterialProperties& props)
    {
        std::shared_lock lock(gpu_resources_mutex);

        auto it = material_gpu_cache.find(id);
        if (it == material_gpu_cache.end())
            return;

        uint32_t slot = it->second.material_buffer_index;
        if (slot >= material_capacity || !material_buffer)
            return;

        // Upload material data directly to buffer
        // In production, this should use staging buffer for better sync
        auto material_data = reinterpret_cast<MaterialProperties*>(material_buffer->map(device));
        if (material_data)
        {
            material_data[slot] = props;
            material_buffer->unmap(device);
        }
    }

    MaterialGpu GpuResourceSystem::get_material_gpu(const AssetId& id) const
    {
        std::shared_lock lock(gpu_resources_mutex);

        auto it = material_gpu_cache.find(id);
        if (it != material_gpu_cache.end())
            return it->second;

        return MaterialGpu{};
    }

    void GpuResourceSystem::release()
    {
        std::unique_lock lock(gpu_resources_mutex);

        gpu_resources.clear();
        texture_lru.clear();
        mesh_lru.clear();
        point_cloud_lru.clear();
        gaussian_lru.clear();
        texture_bindless.clear();
        texture_gpu_cache.clear();
        mesh_gpu_cache.clear();
        material_gpu_cache.clear();
        point_cloud_gpu_cache.clear();
        gaussian_gpu_cache.clear();
        gaussian_buffer_uploads.clear();
        material_buffer.reset();
        material_capacity = 0;
        material_count = 0;

        texture_memory_usage = 0;
        buffer_memory_usage = 0;
        bindless.reset();
        bindless_descriptor_set = nullptr;
    }

} // namespace diverse
