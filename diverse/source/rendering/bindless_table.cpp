#include "bindless_table.h"
#include "backend/drs_rhi/gpu_buffer.h"
#include "core/ds_log.h"
#include <algorithm>

namespace diverse
{
    BindlessTable::BindlessTable(rhi::GpuDevice* dev, uint32_t cap)
        : device(dev)
        , max_slots(cap)
    {
        slots.resize(max_slots);
        rhi::GpuBufferDesc desc = rhi::GpuBufferDesc::new_cpu_to_gpu(
            max_slots * sizeof(float) * 4,
            rhi::BufferUsageFlags::STORAGE_BUFFER | rhi::BufferUsageFlags::TRANSFER_DST);
        texture_sizes = device->create_buffer(desc, "bindless_texture_sizes", nullptr);
    }

    void BindlessTable::set_descriptor_set(rhi::DescriptorSet* set, uint32_t tex_binding, rhi::GpuBuffer* sizes, uint32_t sizes_binding)
    {
        DS_UNUSED(sizes);
        descriptor_set = set;
        texture_binding_id = tex_binding;
        size_binding_id = sizes_binding;
        // texture_sizes is owned by BindlessTable. attach_bindless_descriptor_set
        // passes get_size_buffer() (same pointer); never replace the owning ref
        // with a non-owning alias or the buffer is freed while still in use.
    }

    BindlessHandle BindlessTable::allocate_texture(rhi::GpuTexture* texture)
    {
        if (!texture)
            return {};

        std::lock_guard lock(mutex);
        uint32_t slot = INVALID;
        if (!free_list.empty())
        {
            slot = free_list.back();
            free_list.pop_back();
        }
        else if (next_slot < max_slots)
        {
            slot = next_slot++;
        }
        else
        {
            DS_LOG_ERROR("BindlessTable capacity exceeded ({})", max_slots);
            return {};
        }

        auto& s = slots[slot];
        if (!s.alive)
            ++live_slots;
        ++s.generation;
        s.texture = texture;
        s.alive = true;

        pending_updates.push_back({ BindlessHandle{ slot, s.generation }, texture });
        return BindlessHandle{ slot, s.generation };
    }

    BindlessHandle BindlessTable::reserve_slot(uint32_t fixed_index)
    {
        if (fixed_index >= max_slots)
            return {};

        std::lock_guard lock(mutex);
        free_list.erase(
            std::remove(free_list.begin(), free_list.end(), fixed_index),
            free_list.end());

        auto& s = slots[fixed_index];
        if (!s.alive)
            ++live_slots;
        ++s.generation;
        s.alive = true;
        next_slot = std::max(next_slot, fixed_index + 1);
        return BindlessHandle{ fixed_index, s.generation };
    }

    void BindlessTable::update_texture(BindlessHandle handle, rhi::GpuTexture* texture)
    {
        if (!handle.is_valid() || handle.index >= max_slots)
            return;
        std::lock_guard lock(mutex);
        if (slots[handle.index].generation != handle.generation)
            return;
        slots[handle.index].texture = texture;
        pending_updates.push_back({ handle, texture });
    }

    void BindlessTable::free_later(BindlessHandle handle, uint64_t frame)
    {
        if (!handle.is_valid())
            return;
        std::lock_guard lock(mutex);
        deferred_frees.push_back({ handle, frame });
    }

    void BindlessTable::flush_descriptor_updates()
    {
        if (!descriptor_set || pending_updates.empty())
            return;

        std::vector<PendingDescriptorUpdate> batch;
        {
            std::lock_guard lock(mutex);
            batch.swap(pending_updates);
        }

        for (const auto& upd : batch)
        {
            if (!upd.texture || upd.handle.index >= max_slots)
                continue;
            {
                std::lock_guard lock(mutex);
                if (slots[upd.handle.index].generation != upd.handle.generation)
                    continue;
            }

            rhi::DescriptorImageInfo info{};
            info.image_layout = rhi::ImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            info.view = upd.texture->view(device, rhi::GpuTextureViewDesc()).get();
            device->write_descriptor_set(descriptor_set, texture_binding_id, upd.handle.index, info);
            write_texture_size(upd.handle.index, upd.texture);
        }
    }

    void BindlessTable::process_deferred_frees(uint64_t completed_frame)
    {
        std::lock_guard lock(mutex);
        deferred_frees.erase(
            std::remove_if(deferred_frees.begin(), deferred_frees.end(),
                [&](PendingBindlessFree& entry) {
                    if (entry.release_frame > completed_frame)
                        return false;
                    if (entry.handle.index >= max_slots)
                        return true;
                    auto& s = slots[entry.handle.index];
                    if (s.generation != entry.handle.generation)
                        return true;
                    s.alive = false;
                    s.texture = nullptr;
                    free_list.push_back(entry.handle.index);
                    if (live_slots > 0)
                        --live_slots;
                    return true;
                }),
            deferred_frees.end());
    }

    bool BindlessTable::validate(BindlessHandle handle) const
    {
        if (!handle.is_valid() || handle.index >= max_slots)
            return false;
        std::lock_guard lock(mutex);
        const auto& s = slots[handle.index];
        return s.alive && s.generation == handle.generation;
    }

    void BindlessTable::write_texture_size(uint32_t slot, const rhi::GpuTexture* texture)
    {
        if (!texture_sizes || !texture)
            return;
        auto sz = texture->desc.extent_inv_extent_2d();
        float data[4] = { sz[0], sz[1], sz[2], sz[3] };
        texture_sizes->copy_from(device, reinterpret_cast<u8*>(data), sizeof(data), slot * sizeof(data));
    }

} // namespace diverse
