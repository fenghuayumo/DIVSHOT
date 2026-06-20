#pragma once

#include "backend/drs_rhi/gpu_resource.h"
#include "backend/drs_rhi/gpu_texture.h"
#include "backend/drs_rhi/gpu_device.h"
#include <cstdint>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <vector>

namespace diverse
{
    namespace rhi { struct DescriptorSet; }

    // Stable bindless handle with generation for stale detection
    struct BindlessHandle
    {
        uint32_t index = 0xFFFFFFFF;
        uint32_t generation = 0;

        bool is_valid() const { return index != 0xFFFFFFFF; }
        bool operator==(const BindlessHandle& o) const { return index == o.index && generation == o.generation; }
    };

    struct PendingBindlessFree
    {
        BindlessHandle handle;
        uint64_t release_frame = 0;
    };

    struct PendingDescriptorUpdate
    {
        BindlessHandle handle;
        rhi::GpuTexture* texture = nullptr;
    };

    // First-class bindless texture table: allocation, free list, generation, batch updates
    class BindlessTable
    {
    public:
        static constexpr uint32_t INVALID = 0xFFFFFFFF;
        static constexpr uint32_t DEFAULT_WHITE = 3;
        static constexpr uint32_t DEFAULT_NORMAL = 4;

        explicit BindlessTable(rhi::GpuDevice* device, uint32_t capacity = 16384);

        void set_descriptor_set(rhi::DescriptorSet* set, uint32_t texture_binding_id, rhi::GpuBuffer* size_buffer, uint32_t size_binding_id);

        BindlessHandle allocate_texture(rhi::GpuTexture* texture);
        BindlessHandle reserve_slot(uint32_t fixed_index);
        void update_texture(BindlessHandle handle, rhi::GpuTexture* texture);
        void free_later(BindlessHandle handle, uint64_t frame);

        void flush_descriptor_updates();
        void process_deferred_frees(uint64_t completed_frame);

        uint32_t capacity() const { return max_slots; }
        uint32_t live_count() const { return live_slots; }
        bool validate(BindlessHandle handle) const;

        rhi::GpuBuffer* get_size_buffer() const { return texture_sizes.get(); }

    private:
        struct Slot
        {
            rhi::GpuTexture* texture = nullptr;
            uint32_t generation = 0;
            bool alive = false;
        };

        rhi::GpuDevice* device = nullptr;
        rhi::DescriptorSet* descriptor_set = nullptr;
        uint32_t texture_binding_id = 6;
        uint32_t size_binding_id = 7;
        uint32_t max_slots = 0;
        uint32_t live_slots = 0;
        uint32_t next_slot = 0;

        std::vector<Slot> slots;
        std::vector<uint32_t> free_list;
        std::vector<PendingBindlessFree> deferred_frees;
        std::vector<PendingDescriptorUpdate> pending_updates;
        mutable std::mutex mutex;
        std::shared_ptr<rhi::GpuBuffer> texture_sizes;

        void write_texture_size(uint32_t slot, const rhi::GpuTexture* texture);
    };

} // namespace diverse
