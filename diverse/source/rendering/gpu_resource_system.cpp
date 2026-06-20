#include "gpu_resource_system.h"
#include "assets/asset_registry.h"
#include "assets/asset_system.h"
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

        size_t align_up(size_t value, size_t alignment)
        {
            return (value + alignment - 1) & ~(alignment - 1);
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

        std::lock_guard lock(upload_queue_mutex);

        while (!upload_queue.empty())
        {
            auto request = upload_queue.top();
            upload_queue.pop();

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

        auto texture_asset = AssetSystem::get_instance().get_asset<TextureAsset>(id);
        if (!texture_asset)
            return;

        auto gpu_texture = upload_texture_asset(*texture_asset, device);
        if (!gpu_texture)
            return;

        const size_t gpu_memory_size = texture_asset->calculate_memory_size();

        std::unique_lock lock(gpu_resources_mutex);

        TextureGpu gpu;
        gpu.texture = gpu_texture;
        gpu.gpu_memory_size = gpu_memory_size;
        gpu.resident_version = registry.get_version(id);

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
        entry.resource = gpu_texture;
        entry.gpu_version = gpu.resident_version;
        entry.last_used_frame = current_frame;
        entry.is_resident = true;
        entry.gpu_memory_size = gpu_memory_size;
        entry.priority = ResidentPriority::Normal;
        texture_memory_usage += gpu_memory_size;

        registry.set_state(id, AssetState::ResidentGpu);
    }

    void GpuResourceSystem::process_mesh_upload(const AssetId& id)
    {
        auto mesh_asset = AssetSystem::get_instance().get_asset<MeshAsset>(id);
        if (!mesh_asset)
            return;

        MeshUploadResult upload;
        if (!upload_mesh_asset(*mesh_asset, device, upload))
            return;

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
        gpu.resident_version = AssetRegistry::get_instance().get_version(id);
        gpu.vertex_buffer_size = upload.vertex_buffer_size;
        gpu.index_buffer_size = upload.index_buffer_size;
        mesh_gpu_cache[id] = gpu;

        GpuResourceEntry& entry = gpu_resources[id];
        entry.resource = upload.vertex_buffer;
        entry.gpu_version = gpu.resident_version;
        entry.last_used_frame = current_frame;
        entry.is_resident = true;
        entry.gpu_memory_size = gpu.vertex_buffer_size + gpu.index_buffer_size;
        entry.priority = ResidentPriority::Normal;
        buffer_memory_usage += entry.gpu_memory_size;

        AssetRegistry::get_instance().set_state(id, AssetState::ResidentGpu);
    }

    void GpuResourceSystem::process_material_upload(const AssetId& id)
    {
        // Materials are uploaded to a uniform buffer, not as separate GPU resources
        // This would update the material buffer
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

        // Evict until under budget
        for (const auto& entry : texture_lru)
        {
            if (current <= target)
            {
                break;
            }

            auto it = gpu_resources.find(entry.id);
            if (it != gpu_resources.end() &&
                it->second.priority > ResidentPriority::High &&
                it->second.is_resident)
            {
                // Defer release of GPU resource
                defer_release(it->second.resource, current_frame + MAX_FRAMES_IN_FLIGHT, entry.id);

                // Mark as evicted
                it->second.is_resident = false;
                current -= it->second.gpu_memory_size;
            }
        }
    }

    void GpuResourceSystem::evict_buffers_to_budget()
    {
        // Similar to texture eviction but for buffers
        size_t target = budget.total_buffer_budget_mb * 1024 * 1024 * budget.low_watermark;
        size_t current = calculate_total_buffer_usage();

        std::unique_lock lock(gpu_resources_mutex);

        std::sort(mesh_lru.begin(), mesh_lru.end(),
            [](const auto& a, const auto& b)
            {
                if (a.priority <= ResidentPriority::High) return false;
                if (b.priority <= ResidentPriority::High) return true;
                return a.last_used_frame < b.last_used_frame;
            });

        for (const auto& entry : mesh_lru)
        {
            if (current <= target)
            {
                break;
            }

            auto it = gpu_resources.find(entry.id);
            if (it != gpu_resources.end() &&
                it->second.priority > ResidentPriority::High &&
                it->second.is_resident)
            {
                defer_release(it->second.resource, current_frame + MAX_FRAMES_IN_FLIGHT, entry.id);
                it->second.is_resident = false;
                current -= it->second.gpu_memory_size;
            }
        }
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
            bindless->set_descriptor_set(set, texture_binding_id, bindless->get_size_buffer(), size_binding_id);
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
        {
            std::shared_lock lock(gpu_resources_mutex);
            auto it = texture_gpu_cache.find(id);
            if (it != texture_gpu_cache.end())
                return it->second;
        }
        queue_upload(id, AssetType::Texture, priority);
        process_texture_upload(id);
        flush_bindless_updates();
        std::shared_lock lock(gpu_resources_mutex);
        auto found = texture_gpu_cache.find(id);
        return found != texture_gpu_cache.end() ? found->second : TextureGpu{};
    }

    MeshGpu GpuResourceSystem::request_mesh(const AssetId& id, UploadPriority priority)
    {
        {
            std::shared_lock lock(gpu_resources_mutex);
            auto it = mesh_gpu_cache.find(id);
            if (it != mesh_gpu_cache.end())
                return it->second;
        }
        queue_upload(id, AssetType::MeshModel, priority);
        process_mesh_upload(id);
        std::shared_lock lock(gpu_resources_mutex);
        auto found = mesh_gpu_cache.find(id);
        return found != mesh_gpu_cache.end() ? found->second : MeshGpu{};
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

            mesh_gpu_cache.erase(mesh_it);
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

    void GpuResourceSystem::release()
    {
        std::unique_lock lock(gpu_resources_mutex);

        gpu_resources.clear();
        texture_lru.clear();
        mesh_lru.clear();
        texture_bindless.clear();
        texture_gpu_cache.clear();
        mesh_gpu_cache.clear();

        texture_memory_usage = 0;
        buffer_memory_usage = 0;
        bindless.reset();
        bindless_descriptor_set = nullptr;
    }

} // namespace diverse
