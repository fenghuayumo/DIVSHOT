#pragma once

#include "asset_id.h"
#include "core/core.h"
#include "backend/drs_rhi/pixel_format.h"
#include "backend/drs_rhi/gpu_resource.h"
#include <array>
#include <cstdint>
#include <memory>

namespace diverse
{
    namespace rhi
    {
        struct GpuTexture;
        struct GpuBuffer;
        struct GpuDevice;
    }

    // Bindless image handle - stable index into bindless descriptor array
    struct BindlessImageHandle
    {
        uint32_t index;

        static constexpr uint32_t INVALID = 0xFFFFFFFF;

        BindlessImageHandle() : index(INVALID) {}
        explicit BindlessImageHandle(uint32_t idx) : index(idx) {}

        bool is_valid() const { return index != INVALID; }
        bool operator==(const BindlessImageHandle& other) const { return index == other.index; }
        bool operator!=(const BindlessImageHandle& other) const { return !(*this == other); }
    };

    // GPU buffer handle - wrapper for GPU buffer reference
    struct GpuBufferHandle
    {
        uint32_t index;
        uint64_t offset;
        uint64_t size;

        static constexpr uint32_t INVALID = 0xFFFFFFFF;

        GpuBufferHandle() : index(INVALID), offset(0), size(0) {}
        GpuBufferHandle(uint32_t idx, uint64_t off, uint64_t sz)
            : index(idx), offset(off), size(sz) {}

        bool is_valid() const { return index != INVALID; }
    };

    // Forward declarations for CPU asset types
    class TextureAsset;
    class MeshAsset;
    class MaterialAsset;

    // GPU-side texture representation
    // This is managed by GpuResourceSystem, not by asset classes
    struct TextureGpu
    {
        std::shared_ptr<rhi::GpuTexture> texture;      // The GPU texture resource
        BindlessImageHandle srv;                       // Bindless shader resource view
        uint32_t resident_version;                    // Version counter for hot reload detection

        // Memory tracking for budget control
        size_t gpu_memory_size;                       // Size in bytes

        TextureGpu()
            : resident_version(0)
            , gpu_memory_size(0)
        {}
    };

    // GPU-side mesh representation (managed exclusively by GpuResourceSystem)
    struct MeshGpu
    {
        std::shared_ptr<rhi::GpuBuffer> vertex_buffer;
        std::shared_ptr<rhi::GpuBuffer> index_buffer;
        uint32_t vertex_pos_nor_offset = 0;
        uint32_t vertex_uv_offset = 0;
        uint32_t vertex_tangent_offset = 0;
        uint32_t vertex_color_offset = 0;
        uint32_t vertex_count = 0;
        uint32_t index_count = 0;
        uint32_t bindless_slot = 0xFFFFFFFF;
        uint32_t resident_version = 0;
        size_t vertex_buffer_size = 0;
        size_t index_buffer_size = 0;

        bool is_valid() const { return vertex_buffer && index_buffer; }
    };

    // GPU-side material representation
    // MaterialGpu only contains GPU-relevant data (buffer index and bindless indices)
    // Material properties are stored in a GPU buffer
    struct MaterialGpu
    {
        uint32_t material_buffer_index;
        uint32_t resident_version;
        std::array<uint32_t, 8> texture_bindless_indices;

        static constexpr size_t TEXTURE_SLOT_ALBEDO = 0;
        static constexpr size_t TEXTURE_SLOT_NORMAL = 1;
        static constexpr size_t TEXTURE_SLOT_METALLIC = 2;
        static constexpr size_t TEXTURE_SLOT_ROUGHNESS = 3;
        static constexpr size_t TEXTURE_SLOT_AO = 4;
        static constexpr size_t TEXTURE_SLOT_EMISSIVE = 5;
        static constexpr size_t TEXTURE_SLOT_TRANSMISSION = 6;
        static constexpr size_t TEXTURE_SLOT_NORMAL_DETAIL = 7;

        MaterialGpu()
            : material_buffer_index(0xFFFFFFFF)
            , resident_version(0)
        {
            texture_bindless_indices.fill(0xFFFFFFFF);
        }

        bool is_valid() const { return material_buffer_index != 0xFFFFFFFF; }

        uint32_t get_bindless_index(size_t slot) const
        {
            return slot < texture_bindless_indices.size() ? texture_bindless_indices[slot] : 0xFFFFFFFF;
        }

        void set_bindless_index(size_t slot, uint32_t index)
        {
            if (slot < texture_bindless_indices.size())
                texture_bindless_indices[slot] = index;
        }
    };

    // Resident priority for GPU memory budget control
    enum class ResidentPriority : uint8_t
    {
        Critical = 0,  // Should never be evicted (e.g., currently rendering)
        High = 1,      // Important assets (e.g., player character)
        Normal = 2,    // Standard assets
        Low = 3        // Background/distant assets
    };

    // Asset-GPU mapping entry with budget tracking
    template<typename AssetType>
    struct AssetGpuMapping
    {
        AssetId asset_id;
        uint32_t gpu_version;                         // Incremented on GPU reload
        std::shared_ptr<rhi::GpuResource> gpu_resource;
        uint64_t last_used_frame;                    // For LRU tracking

        // Budget tracking
        size_t gpu_memory_size;
        ResidentPriority priority;
        bool is_resident;

        AssetGpuMapping()
            : gpu_version(0)
            , last_used_frame(0)
            , gpu_memory_size(0)
            , priority(ResidentPriority::Normal)
            , is_resident(false)
        {}
    };

    // Specializations for specific asset types
    using TextureGpuMapping = AssetGpuMapping<TextureAsset>;
    using MeshGpuMapping = AssetGpuMapping<MeshAsset>;
    using MaterialGpuMapping = AssetGpuMapping<MaterialAsset>;
}
