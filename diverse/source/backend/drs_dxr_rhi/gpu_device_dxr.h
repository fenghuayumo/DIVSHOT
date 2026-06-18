#pragma once

#include "backend/drs_rhi/gpu_device.h"

#if defined(DS_PLATFORM_WINDOWS)
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <stdexcept>

namespace diverse
{
    namespace rhi
    {
        struct GpuDeviceDXR;

        struct GpuCommandBufferDXR : public CommandBuffer
        {
            explicit GpuCommandBufferDXR(GpuDeviceDXR* device);
            ~GpuCommandBufferDXR();

            auto begin() -> void override;
            auto end() -> void override;
            auto wait() -> void override;

            GpuDeviceDXR* owner = nullptr;
            Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> command_list;
            Microsoft::WRL::ComPtr<ID3D12Fence> fence;
            HANDLE fence_event = nullptr;
            u64 fence_value = 0;
        };

        struct DeviceFrameDXR : public DeviceFrame
        {
            DeviceFrameDXR() = default;
            DeviceFrameDXR(const std::shared_ptr<CommandBuffer>& main_cb, const std::shared_ptr<CommandBuffer>& present_cb)
                : DeviceFrame(main_cb, present_cb)
            {}
        };

        struct GpuBufferViewDXR : public GpuBufferView
        {
        };

        struct GpuBufferDXR : public GpuBuffer
        {
            GpuBufferDXR(const GpuBufferDesc& desc, Microsoft::WRL::ComPtr<ID3D12Resource> resource);

            auto device_address(const GpuDevice* device) -> u64 override;
            auto view(const GpuDevice* device, const GpuBufferViewDesc& view_desc) -> std::shared_ptr<GpuBufferView> override;
            auto map(const GpuDevice* device) -> u8* override;
            auto unmap(const GpuDevice* device) -> void override;

            Microsoft::WRL::ComPtr<ID3D12Resource> resource;
            u8* mapped = nullptr;
        };

        struct GpuTextureViewDXR : public GpuTextureView
        {
        };

        struct GpuTextureDXR : public GpuTexture
        {
            explicit GpuTextureDXR(const GpuTextureDesc& desc);
            auto view(const GpuDevice* device, const GpuTextureViewDesc& desc) -> std::shared_ptr<GpuTextureView> override;
        };

        struct RenderPassDXR : public RenderPass
        {
            auto begin_render_pass() -> void override {}
            auto end_render_pass() -> void override {}
        };

        struct DescriptorSetDXR : public DescriptorSet
        {
        };

        struct PipelineDXR : public GpuPipeline
        {
            explicit PipelineDXR(PieplineType type)
            {
                ty = type;
            }
        };

        struct GpuRayTracingAccelerationDXR : public GpuRayTracingAcceleration
        {
            explicit GpuRayTracingAccelerationDXR(const std::shared_ptr<GpuBuffer>& buffer)
                : GpuRayTracingAcceleration(buffer)
            {}

            auto as_device_address(const GpuDevice* device) -> u64 override;
        };

        struct SwapchainDXR : public Swapchain
        {
            explicit SwapchainDXR(SwapchainDesc desc)
            {
                this->desc = desc;
            }

            auto acquire_next_image() -> SwapchainImage override;
            auto present_image(const SwapchainImage& swap_chain, CommandBuffer* present_cb) -> void override;
            auto resize(u32 width, u32 height) -> void override;
            auto current_buffer_index() -> u64 override { return 0; }
            auto current_frame_index() -> u64 override { return frame_index; }
            auto reset_frame_index() -> void override { frame_index = 0; }

            u64 frame_index = 0;
        };

        struct GpuDeviceDXR : public GpuDevice
        {
            explicit GpuDeviceDXR(u32 device_index);
            ~GpuDeviceDXR() override;

            auto create_texture(const GpuTextureDesc& desc, const std::vector<ImageSubData>& initial_data, const char* name = nullptr)->std::shared_ptr<GpuTexture> override;
            auto create_buffer(const GpuBufferDesc& desc, const char* name, uint8* initial_data)->std::shared_ptr<GpuBuffer> override;
            auto create_render_command_buffer(const char* name = nullptr) -> std::shared_ptr<CommandBuffer> override;
            auto create_swapchain(SwapchainDesc desc, void* window_handle) -> std::shared_ptr<Swapchain> override;
            auto create_descriptor_set(GpuBuffer* dynamic_constants, const std::unordered_map<u32, DescriptorInfo>& descriptors, const char* name = nullptr)->std::shared_ptr<DescriptorSet> override;
            auto create_descriptor_set(const std::unordered_map<u32, DescriptorInfo>& descriptors, const char* name = nullptr) -> std::shared_ptr<DescriptorSet> override;
            auto bind_descriptor_set(CommandBuffer* cb, GpuPipeline* pipeline, std::vector<DescriptorSetBinding>& bindings, uint32 set_index)->void override;
            auto bind_descriptor_set(CommandBuffer* cb, GpuPipeline* pipeline, uint32 set_idx, DescriptorSet* set, u32 dynamic_offset_count, u32* dynamic_offset)->void override;
            auto bind_pipeline(CommandBuffer* cb, GpuPipeline* pipeline)->void override;
            auto push_constants(CommandBuffer* cb, GpuPipeline* pipeline, u32 offset, u8* constants, u32 size_) -> void override;
            auto create_compute_pipeline(const CompiledShaderCode& spirv, const ComputePipelineDesc& desc)->std::shared_ptr<ComputePipeline> override;
            auto create_raster_pipeline(const std::vector<PipelineShader>& shaders, const RasterPipelineDesc& desc)->std::shared_ptr<RasterPipeline> override;
            auto create_ray_tracing_pipeline(const std::vector<PipelineShader>& shaders, const RayTracingPipelineDesc& desc) -> std::shared_ptr<RayTracingPipeline> override;
            auto create_mesh_shader_pipeline(const std::vector<PipelineShader>& shaders, const MeshShaderPipelineDesc& desc) -> std::shared_ptr<MeshShaderPipeline> override;
            auto create_render_pass(const RenderPassDesc& desc, const char* name = nullptr) -> std::shared_ptr<RenderPass> override;
            auto begin_frame()->DeviceFrame* override;
            auto end_frame(DeviceFrame* device)->void override;
            auto begin_cmd(CommandBuffer* cb)->void override;
            auto end_cmd(CommandBuffer* cb)->void override;
            auto submit_cmd(CommandBuffer* cb)->void override;
            auto execute_cmd(CommandBuffer* cb)->void override;
            auto record_image_barrier(CommandBuffer* cb, const ImageBarrier& barrier)->void override;
            auto record_buffer_barrier(CommandBuffer* cb, const BufferBarrier& barrier)->void override;
            auto record_global_barrier(CommandBuffer* cb, const std::vector<rhi::AccessType>& previous_accesses, const std::vector<rhi::AccessType>& next_accesses)->void override;
            auto dispatch(CommandBuffer* cb, const std::array<u32, 3>& group_dim, const std::array<u32, 3>& group_size)->void override;
            auto dispatch_indirect(CommandBuffer* cb, GpuBuffer* args_buffer, u64 args_buffer_offset)->void override;
            auto write_descriptor_set(DescriptorSet* descriptor_set, u32 dst_binding, rhi::GpuBuffer* buffer, u32 array_index = 0)->void override;
            auto write_descriptor_set(DescriptorSet* descriptor_set, u32 dst_binding, u32 rray_index, const rhi::DescriptorImageInfo& img_info) -> void override;
            auto clear_depth_stencil(CommandBuffer* cb, GpuTexture* texture, float depth, u32 stencil)->void override;
            auto clear_color(CommandBuffer* cb, GpuTexture* texture, const std::array<f32, 4>& color) -> void override;
            auto copy_image(GpuTexture* src, GpuTexture* dst, CommandBuffer* cmd_buf) -> void override;
            auto copy_image(GpuBuffer* src, GpuTexture* dst, CommandBuffer* cmd_buf) -> void override;
            auto update_texture(GpuTexture* src, const std::vector<ImageSubData>& update_data, const TextureRegion& tex_region)->void override;
            auto set_point_size(CommandBuffer* cb, float point_size = 1.0f)->void override;
            auto set_line_width(CommandBuffer* cb, float line_width = 1.0f)->void override;
            auto set_viewport(CommandBuffer* cb, const ViewPort& view, const Scissor& scissors)->void override;
            auto begin_render_pass(CommandBuffer* cb, const std::array<u32, 2>& dims, RenderPass* render_pass, const std::vector<rhi::GpuTexture*>& color_desc, rhi::GpuTexture* depth_desc)->void override;
            auto end_render_pass(CommandBuffer* cb)->void override;
            auto create_ray_tracing_bottom_acceleration(const RayTracingBottomAccelerationDesc& desc)-> std::shared_ptr<GpuRayTracingAcceleration> override;
            auto create_ray_tracing_top_acceleration(const RayTracingTopAccelerationDesc& desc, const RayTracingAccelerationScratchBuffer& scratch_buffer)-> std::shared_ptr<GpuRayTracingAcceleration> override;
            auto rebuild_ray_tracing_top_acceleration(CommandBuffer* cb, u64 instance_buffer_address, u64 instance_count, GpuRayTracingAcceleration* tlas, RayTracingAccelerationScratchBuffer* scratch_buffer)->void override;
            auto bind_vertex_buffers(CommandBuffer* cb, const GpuBuffer* const* vertexBuffers, uint32_t slot, uint32_t count, const uint32_t* strides, const uint64_t* offsets)->void override;
            auto bind_index_buffer(CommandBuffer* cb, const GpuBuffer* indexBuffer, const IndexBufferFormat format, uint64_t offset)->void override;
            auto draw(CommandBuffer* cb, uint32_t vertexCount, uint32_t startVertexLocation) ->void override;
            auto draw_indexed(CommandBuffer* cb, uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation)->void override;
            auto draw_instanced(CommandBuffer* cb, uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation)->void override;
            auto draw_indexed_instanced(CommandBuffer* cb, uint32_t indexCount, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocationd)->void override;
            auto draw_instanced_indirect(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset)->void override;
            auto draw_indexed_instanced_indirect(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset)->void override;
            auto draw_instanced_indirect_count(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset, const GpuBuffer* count, uint64_t count_offset, uint32_t max_count)->void override;
            auto draw_indexed_instanced_indirect_count(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset, const GpuBuffer* count, uint64_t count_offset, uint32_t max_count)->void override;
            auto draw_mesh_tasks(CommandBuffer* cb, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)->void override;
            auto draw_mesh_tasks_indirect(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset, uint32_t draw_count, uint32_t stride)->void override;
            auto draw_mesh_tasks_indirect_count(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset, const GpuBuffer* count, uint64_t count_offset, uint32_t max_count, uint32_t stride)->void override;
            auto with_setup_cb(std::function<void(CommandBuffer* cmd)>&& callback)->void override;
            auto copy_buffer(CommandBuffer* cmd, GpuBuffer* src, u64 src_offset, GpuBuffer* dst, u64 dst_offset, u64 size_)->void override;
            auto trace_rays(CommandBuffer* cb, RayTracingPipeline* rtpipeline, const std::array<u32, 3>& threads)->void override;
            auto trace_rays_indirect(CommandBuffer* cb, RayTracingPipeline* rtpipeline, u64 args_buffer_address) -> void override;
            auto event_begin(const char* name, CommandBuffer* cb)->void override;
            auto event_end(CommandBuffer* cb) -> void override;
            auto set_name(GpuResource* resource, const char* name)const->void override;
            auto destroy_resource(GpuResource* resource)->void override;
            auto defer_release(std::function<void()> f)->void override;
            auto export_image(rhi::GpuTexture* image)->std::vector<u8> override;
            auto blit_image(rhi::GpuTexture* src, rhi::GpuTexture* dst, CommandBuffer* cmd_buf = nullptr) ->void override;
            auto fill_buffer(CommandBuffer* cb, GpuBuffer* buffer, uint32_t value) -> void override;

            auto get_graphics_cmd_buffer()->CommandBuffer* override;

            Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
            Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
            Microsoft::WRL::ComPtr<ID3D12Device5> device;
            Microsoft::WRL::ComPtr<ID3D12CommandQueue> graphics_queue;
            std::unique_ptr<GpuCommandBufferDXR> setup_cb;
            DeviceFrameDXR frames[DYNAMIC_CONSTANTS_BUFFER_COUNT];
            u32 active_frame = 0;
            bool supports_dxr = false;
        };

        auto create_dxr_device(u32 device_index)->GpuDevice*;
    }
}

#endif
