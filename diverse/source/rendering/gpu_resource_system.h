#pragma once

#include "assets/asset.h"
#include "assets/asset_id.h"
#include "assets/cpu_assets.h"
#include "assets/gpu_assets.h"
#include "bindless_table.h"
#include "gaussian_gpu_utils.h"
#include "texture_gpu_utils.h"
#include "backend/drs_rhi/gpu_resource.h"
#include "backend/drs_rhi/gpu_device.h"
#include "backend/drs_rhi/gpu_texture.h"
#include "backend/drs_rhi/gpu_buffer.h"
#include <unordered_map>
#include <shared_mutex>
#include <functional>
#include <queue>
#include <vector>
#include <memory>
#include <atomic>

namespace diverse
{
    // Forward declarations
    class StagingBuffer;
    class GaussianModel;
    struct MaterialProperties;

    // Upload priority for GPU resource uploads
    enum class UploadPriority : uint8_t
    {
        Critical = 0,  // Must upload immediately (e.g., currently rendering)
        High = 1,      // Important assets (e.g., player character)
        Normal = 2,    // Standard assets
        Low = 3        // Background/distant assets
    };

    // Upload request
    struct UploadRequest
    {
        AssetId asset_id;
        AssetType asset_type;
        UploadPriority priority;
        uint64_t request_frame;

        UploadRequest()
            : asset_type(AssetType::None)
            , priority(UploadPriority::Normal)
            , request_frame(0)
        {}

        bool operator<(const UploadRequest& other) const
        {
            return priority > other.priority;  // Higher priority first
        }
    };

    // Budget configuration for GPU memory control
    struct BudgetConfig
    {
        size_t total_texture_budget_mb = 512;   // 512MB default for textures
        size_t total_buffer_budget_mb = 256;    // 256MB default for buffers
        float high_watermark = 0.9f;            // Start eviction at 90%
        float low_watermark = 0.7f;             // Stop eviction at 70%
    };

    // LRU cache entry for GPU resources
    template<typename AssetType>
    struct GpuLruEntry
    {
        AssetId id;
        uint64_t last_used_frame;
        size_t memory_size;
        ResidentPriority priority;

        GpuLruEntry()
            : last_used_frame(0)
            , memory_size(0)
            , priority(ResidentPriority::Normal)
        {}

        GpuLruEntry(const AssetId& i, uint64_t frame, size_t size, ResidentPriority p)
            : id(i)
            , last_used_frame(frame)
            , memory_size(size)
            , priority(p)
        {}
    };

    // GPU resource entry with version tracking
    struct GpuResourceEntry
    {
        std::shared_ptr<rhi::GpuResource> resource;
        uint32_t gpu_version;
        uint64_t last_used_frame;
        size_t gpu_memory_size;
        ResidentPriority priority;
        bool is_resident;

        GpuResourceEntry()
            : gpu_version(0)
            , last_used_frame(0)
            , gpu_memory_size(0)
            , priority(ResidentPriority::Normal)
            , is_resident(false)
        {}
    };

    // Staging buffer pool entry
    struct StagingBufferPoolEntry
    {
        std::shared_ptr<StagingBuffer> buffer;
        uint64_t last_used_frame;
        size_t size;

        StagingBufferPoolEntry()
            : last_used_frame(0)
            , size(0)
        {}
    };

    // Deferred release entry
    struct DeferredRelease
    {
        std::shared_ptr<rhi::GpuResource> resource;
        uint64_t release_frame;
        AssetId associated_asset;

        DeferredRelease()
            : release_frame(0)
        {}
    };

    // GpuResourceSystem - Centralized GPU resource management
    // Responsibilities:
    // - Upload queue with prioritization
    // - Memory budget enforcement
    // - Bindless descriptor management
    // - Deferred release (frame-safe)
    // - Staging buffer pool
    class GpuResourceSystem
    {
    public:
        explicit GpuResourceSystem(rhi::GpuDevice* device);
        ~GpuResourceSystem();

        // Budget configuration
        void set_budget_config(const BudgetConfig& config);
        const BudgetConfig& get_budget_config() const;

        // Upload queue
        void queue_upload(const AssetId& id, AssetType type, UploadPriority priority = UploadPriority::Normal);
        void process_upload_queue(uint64_t current_frame);

        // GPU residency management
        void make_resident(const AssetId& id, ResidentPriority priority = ResidentPriority::Normal);
        void make_evictable(const AssetId& id);

        // Budget enforcement
        bool check_budget(size_t required_bytes, AssetType type) const;
        void enforce_budget();

        // Bindless management
        BindlessTable& bindless_table() { return *bindless; }
        const BindlessTable& bindless_table() const { return *bindless; }

        BindlessImageHandle allocate_bindless_slot(rhi::GpuTexture* texture);
        void update_bindless_descriptor(BindlessImageHandle handle, rhi::GpuTexture* texture);
        void free_bindless_slot(BindlessImageHandle handle);

        // Fixed-slot bindless registration (default white/normal at slots 3/4)
        BindlessImageHandle ensure_texture_at_slot(const AssetId& id, uint32_t slot);
        void ensure_default_texture_slots();
        void flush_bindless_updates();
        BindlessImageHandle bind_texture_at_slot(rhi::GpuTexture* texture, uint32_t slot);

        // Typed GPU residency requests
        TextureGpu request_texture(const AssetId& id, UploadPriority priority = UploadPriority::Normal);
        MeshGpu request_mesh(const AssetId& id, UploadPriority priority = UploadPriority::Normal);
        MaterialGpu request_material(const AssetId& id, UploadPriority priority = UploadPriority::Normal);
        PointCloudGpu request_point_cloud(const AssetId& id, UploadPriority priority = UploadPriority::Normal);
        GaussianGpu request_gaussian(const AssetId& id, UploadPriority priority = UploadPriority::Normal);
        GaussianGpu upload_gaussian_from_model(GaussianModel& model, bool compact);
        GaussianGpu get_gaussian_gpu(const AssetId& id) const;
        void enqueue_uploads(uint64_t frame_index);
        void retire(uint64_t completed_frame);

        // Get bindless descriptor set (for renderer integration)
        rhi::DescriptorSet* get_bindless_descriptor_set() const;
        void attach_bindless_descriptor_set(rhi::DescriptorSet* set, uint32_t texture_binding_id, uint32_t size_binding_id);
        void attach_mesh_buffer_bindings(uint32_t vertex_binding_id, uint32_t index_binding_id);
        void attach_material_buffer_binding(uint32_t binding_id);
        void attach_point_cloud_buffer_binding(uint32_t binding_id);
        void attach_gaussian_buffer_bindings(uint32_t gs_binding_id, uint32_t splat_state_binding_id);
        void bind_mesh_to_slot(uint32_t slot, const MeshGpu& mesh);
        void bind_point_cloud_to_slot(uint32_t slot, const PointCloudGpu& point_cloud);
        void bind_gaussian_to_slot(uint32_t slot, const GaussianGpu& gaussian, rhi::GpuBuffer* splat_transform_buffer);

        // Staging buffer pool
        StagingBuffer* acquire_staging_buffer(size_t size);
        void return_staging_buffer(StagingBuffer* buffer);

        // Deferred release
        void defer_release(std::shared_ptr<rhi::GpuResource> resource, uint64_t release_frame, const AssetId& asset_id);
        void process_deferred_releases(uint64_t completed_frame);

        // Hot reload support
        void retire_asset_gpu(const AssetId& id, uint64_t completed_frame);
        void reload_asset(const AssetId& id);
        uint32_t get_gpu_version(const AssetId& id) const;

        // Resource access
        std::shared_ptr<rhi::GpuResource> get_gpu_resource(const AssetId& id) const;
        TextureGpu get_texture_gpu(const AssetId& id) const;
        MeshGpu get_mesh_gpu(const AssetId& id) const;
        MaterialGpu get_material_gpu(const AssetId& id) const;

        // Material buffer management (delegated from renderer)
        void initialize_material_buffer(uint32_t capacity);
        void upload_material_data(const AssetId& id, const MaterialProperties& props);
        rhi::GpuBuffer* get_material_buffer() const { return material_buffer.get(); }
        uint32_t get_material_capacity() const { return material_capacity; }
        uint32_t get_material_count() const { return material_count; }

        // Statistics
        size_t get_texture_memory_usage() const;
        size_t get_buffer_memory_usage() const;
        size_t get_total_memory_usage() const;
        uint32_t get_bindless_slot_count() const;

        // Frame update (call once per frame)
        void update(uint64_t current_frame);

        // Shutdown
        void release();

    private:
        rhi::GpuDevice* device;

        // Budget configuration
        BudgetConfig budget;

        // Upload queue (priority queue)
        std::priority_queue<UploadRequest> upload_queue;
        std::mutex upload_queue_mutex;

        // GPU resource storage
        std::unordered_map<AssetId, GpuResourceEntry> gpu_resources;
        mutable std::shared_mutex gpu_resources_mutex;

        // LRU tracking
        std::vector<GpuLruEntry<TextureAsset>> texture_lru;
        std::vector<GpuLruEntry<MeshAsset>> mesh_lru;
        std::vector<GpuLruEntry<class PointCloudAsset>> point_cloud_lru;
        std::vector<GpuLruEntry<class GaussianAsset>> gaussian_lru;

        // Bindless management
        std::unique_ptr<BindlessTable> bindless;
        rhi::DescriptorSet* bindless_descriptor_set = nullptr;
        uint32_t vertex_buffer_binding_id = 4;
        uint32_t index_buffer_binding_id = 5;
        uint32_t material_buffer_binding_id = 0xFFFFFFFF;
        uint32_t point_cloud_buffer_binding_id = 0xFFFFFFFF;
        uint32_t gaussian_buffer_binding_id = 0xFFFFFFFF;
        uint32_t gaussian_state_binding_id = 0xFFFFFFFF;
        uint32_t next_point_cloud_slot = 0;
        uint32_t next_gaussian_slot = 0;
        std::unordered_map<AssetId, GaussianBufferUpload> gaussian_buffer_uploads;
        std::unordered_map<AssetId, BindlessHandle> texture_bindless;
        std::unordered_map<AssetId, TextureGpu> texture_gpu_cache;
        std::unordered_map<AssetId, MeshGpu> mesh_gpu_cache;
        std::unordered_map<AssetId, MaterialGpu> material_gpu_cache;
        std::unordered_map<AssetId, PointCloudGpu> point_cloud_gpu_cache;
        std::unordered_map<AssetId, GaussianGpu> gaussian_gpu_cache;

        // Staging buffer pool
        std::vector<StagingBufferPoolEntry> staging_buffers;
        std::mutex staging_mutex;

        // Deferred release queue
        std::vector<DeferredRelease> deferred_releases;
        std::mutex deferred_release_mutex;

        // Current frame counter
        uint64_t current_frame;

        // Material buffer (delegated from renderer)
        std::shared_ptr<rhi::GpuBuffer> material_buffer;
        uint32_t material_capacity = 0;
        uint32_t material_count = 0;

        // Memory usage tracking
        size_t texture_memory_usage;
        size_t buffer_memory_usage;

        // Initialization helpers removed — BindlessTable owns bindless state

        // Budget enforcement helpers
        void evict_textures_to_budget();
        void evict_buffers_to_budget();
        size_t calculate_total_texture_usage() const;
        size_t calculate_total_buffer_usage() const;

        // Upload helpers
        void process_texture_upload(const AssetId& id);
        void process_mesh_upload(const AssetId& id);
        void process_material_upload(const AssetId& id);
        void process_point_cloud_upload(const AssetId& id);
        void process_gaussian_upload(const AssetId& id);
    };

    // Staging buffer for GPU uploads
    class StagingBuffer
    {
    public:
        explicit StagingBuffer(rhi::GpuDevice* device, size_t size);
        ~StagingBuffer();

        void* map();
        void unmap();
        void reset();

        rhi::GpuBuffer* get_buffer() const { return buffer.get(); }
        size_t get_size() const { return size; }
        size_t get_used_bytes() const { return used_bytes; }
        bool has_space(size_t bytes) const { return (used_bytes + bytes) <= size; }

        size_t allocate(size_t bytes, size_t alignment);
        void copy_data(size_t offset, const void* data, size_t bytes);

    private:
        rhi::GpuDevice* device;
        std::shared_ptr<rhi::GpuBuffer> buffer;
        size_t size;
        size_t used_bytes;
        void* mapped_ptr;
    };

} // namespace diverse
