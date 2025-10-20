#pragma once
#include "backend/drs_rhi/gpu_device.h"
#include "dvs_metal_utils.h"
#include "gpu_buffer_metal.h"
#include "gpu_texture_metal.h"
#include "gpu_pipeline_metal.h"
#include "gpu_render_pass_metal.h"
#include "gpu_swapchain_metal.h"
#include "gpu_raytracing_metal.h"
namespace diverse
{
    namespace rhi
    {

        struct GpuCommandBufferMetal : public CommandBuffer
        {
            GpuCommandBufferMetal(struct GpuDeviceMetal* dev);
            void    begin() override;
            void    end() override;
            void    wait() override;
            void    signal();
            id<MTLCommandBuffer>    cmd_buf;
            id<MTLRenderCommandEncoder> renderEncoder;
            id<MTLComputeCommandEncoder> computeEncoder;
            struct DeviceFrameMetal* frame;
            struct GpuDeviceMetal* device;
            std::vector<const char*>    cmd_labels;
            u32     current_id = 0;
        };
        struct DeviceFrameMetal : public DeviceFrame
        {
            DeviceFrameMetal(id<MTLDevice> device,const std::shared_ptr<CommandBuffer>& main_cb, const std::shared_ptr<CommandBuffer>& presentation_cb);
            DeviceFrameMetal() = default;
            ~DeviceFrameMetal();
            id<MTLEvent>    submit_done_event;
            int64_t     event_cnt = 1;
        };

        struct GpuDeviceMetal : public GpuDevice
        {
        public:
            //GpuDeviceMetal() {}
            GpuDeviceMetal(u32  device_index);
            ~GpuDeviceMetal();
        public:
       
        public:
            //auto get_min_offset_alignment(const GpuBufferDesc& desc) -> u64 override; 
            auto create_swapchain(SwapchainDesc desc, void* window_handle) -> std::shared_ptr<Swapchain> override;

            auto	begin_frame() -> DeviceFrame* override;
            auto	end_frame(DeviceFrame* frameRes)->void override;
            auto create_render_command_buffer(const char* name = nullptr) -> std::shared_ptr<CommandBuffer> override;
            auto create_texture(const GpuTextureDesc& dec,const std::vector<ImageSubData>&  initial_data,const char* name = nullptr)->std::shared_ptr<GpuTexture> override;
            auto create_buffer(const GpuBufferDesc& desc,const char* name, uint8* initial_data)->std::shared_ptr<GpuBuffer> override;

            auto create_render_pass(const RenderPassDesc& desc,const char* name = nullptr) -> std::shared_ptr<RenderPass> override;
            auto create_descriptor_set(GpuBuffer* dynamic_constants, const std::unordered_map<u32, DescriptorInfo>& descriptors,const char* name = nullptr) -> std::shared_ptr<DescriptorSet> override;
            auto create_descriptor_set(const std::unordered_map<u32, DescriptorInfo>& descriptors,const char* name = nullptr) -> std::shared_ptr<DescriptorSet> override;
            auto bind_descriptor_set(CommandBuffer* cb,GpuPipeline* pipeline,std::vector<DescriptorSetBinding>& bindings,uint32 set_index)->void override;
            auto bind_descriptor_set(CommandBuffer* cb, GpuPipeline* pipeline, uint32 set_idx,DescriptorSet* set, u32 dynamic_offset_count, u32* dynamic_offset) -> void override;
            auto bind_pipeline(CommandBuffer* cb, GpuPipeline* pipeline) -> void;
            auto push_constants(CommandBuffer* cb, GpuPipeline* pipeline, u32 offset, u8* constants, u32 size_) -> void;

            auto create_compute_pipeline(const CompiledShaderCode& spirv, const ComputePipelineDesc& desc) -> std::shared_ptr<ComputePipeline> override;
            auto create_raster_pipeline(const std::vector<PipelineShader>& shaders, const RasterPipelineDesc& desc) -> std::shared_ptr<RasterPipeline> override;
            auto create_ray_tracing_pipeline(const std::vector<PipelineShader>& shaders, const RayTracingPipelineDesc& desc)->std::shared_ptr<RayTracingPipeline> override;

            auto bind_vertex_buffers(CommandBuffer* cb, const GpuBuffer* const* vertexBuffers, uint32_t slot, uint32_t count, const uint32_t* strides, const uint64_t* offsets) -> void override;
            auto bind_index_buffer(CommandBuffer* cb, const GpuBuffer* indexBuffer, const IndexBufferFormat format, uint64_t offset) -> void override;
            auto draw(CommandBuffer* cb, uint32_t vertexCount, uint32_t startVertexLocation) -> void override;
            auto draw_indexed(CommandBuffer* cb, uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation) -> void override;
            auto draw_instanced(CommandBuffer* cb, uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation) -> void override;
            auto draw_indexed_instanced(CommandBuffer* cb, uint32_t indexCount, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocationd) -> void override;
            auto draw_instanced_indirect(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset) -> void override;
            auto draw_indexed_instanced_indirect(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset) -> void override;
            auto draw_instanced_indirect_count(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset, const GpuBuffer* count, uint64_t count_offset, uint32_t max_count) -> void override;
            auto draw_indexed_instanced_indirect_count(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset, const GpuBuffer* count, uint64_t count_offset, uint32_t max_count) -> void override;


            auto begin_cmd(CommandBuffer* cb) -> void override;
            auto end_cmd(CommandBuffer* cb) -> void override;
            auto submit_cmd(CommandBuffer* cb) -> void override;
            auto execute_cmd(CommandBuffer* cb)->void override;
            auto record_image_barrier(CommandBuffer* cb, const ImageBarrier& barrier) -> void override;
            auto record_buffer_barrier(CommandBuffer* cb, const BufferBarrier& barrier)->void override;
            auto record_global_barrier(CommandBuffer* cb,const std::vector<rhi::AccessType>& previous_accesses, const std::vector<rhi::AccessType>& next_accesses) -> void override;

            auto dispatch(CommandBuffer* cb,const std::array<u32, 3>& group_dim, const std::array<u32, 3>& group_size) -> void;
            auto dispatch_indirect(CommandBuffer* cb,GpuBuffer* args_buffer, u64 args_buffer_offset) -> void;
            auto write_descriptor_set(DescriptorSet* descriptor_set, u32 dst_binding, rhi::GpuBuffer* buffer, u32 array_index = 0)->void;
            auto write_descriptor_set(DescriptorSet* descriptor_set, u32 dst_binding, u32 rray_index, const rhi::DescriptorImageInfo& img_info) -> void;
            auto clear_depth_stencil(CommandBuffer* cb, GpuTexture* texture, float depth, u32 stencil) -> void override;
            auto clear_color(CommandBuffer* cb, GpuTexture* texture, const std::array<f32, 4>& color) -> void override;
            auto copy_image(GpuTexture* src, GpuTexture* dst, CommandBuffer* cmd_buf) -> void override;
            auto copy_image(GpuBuffer* src, GpuTexture* dst, CommandBuffer* cmd_buf) -> void  override;
            auto set_point_size(CommandBuffer* cb, float point_size = 1.0f)->void override;
            auto set_line_width(CommandBuffer* cb, float line_width = 1.0f) -> void override;
            auto set_viewport(CommandBuffer* cb, const ViewPort& view, const Scissor& scissors) -> void override;
            auto begin_render_pass(CommandBuffer* cb, const std::array<u32, 2>& dims, RenderPass* render_pass, const std::vector<rhi::GpuTexture*>& color_desc,  rhi::GpuTexture* depth_desc) -> void override;
            auto end_render_pass(CommandBuffer* cb) -> void override;
            auto create_ray_tracing_bottom_acceleration(const RayTracingBottomAccelerationDesc& desc) -> std::shared_ptr<GpuRayTracingAcceleration> override;
            auto create_ray_tracing_top_acceleration(const RayTracingTopAccelerationDesc& desc, const RayTracingAccelerationScratchBuffer& scratch_buffer) -> std::shared_ptr<GpuRayTracingAcceleration> override;

            auto rebuild_ray_tracing_top_acceleration(CommandBuffer* cb, u64 instance_buffer_address, u64 instance_count, GpuRayTracingAcceleration* tlas, RayTracingAccelerationScratchBuffer* scratch_buffer) -> void override;

            auto with_setup_cb(std::function<void(CommandBuffer* cmd)>&& callback) -> void override;
            auto copy_buffer(CommandBuffer* cmd, GpuBuffer* src, u64 src_offset, GpuBuffer* dst, u64 dst_offset, u64 size_) -> void override;
            auto trace_rays(CommandBuffer* cb, RayTracingPipeline* rtpipeline, const std::array<u32, 3>& threads) -> void override;
            auto trace_rays_indirect(CommandBuffer* cb, RayTracingPipeline* rtpipeline, u64 args_buffer_address) -> void override;
            auto event_begin(const char* name, CommandBuffer* cb) -> void  override;
            auto event_end(CommandBuffer* cb) -> void override;
            auto set_name(GpuResource* resource, const char* name) const-> void override;
            auto destroy_resource(GpuResource* resource) -> void override;
            auto defer_release(std::function<void()> f) -> void override;
            auto export_image(rhi::GpuTexture* image) -> std::vector<u8> override;
            auto blit_image(rhi::GpuTexture* src, rhi::GpuTexture* dst, CommandBuffer* cmd_buf = nullptr) -> void override;
            auto fill_buffer(CommandBuffer* cb, GpuBuffer* buffer, uint32_t value) -> void override;
        protected:
            auto initialize()->void;
            auto create_argument_buffer(const DescriptorSetLayout& set)->id<MTLBuffer>;
            auto create_samplers()->std::unordered_map<rhi::GpuSamplerDesc, id<MTLSamplerState>>;
        protected:
            DeviceFrameMetal		frames[2];
            std::mutex			cb_mutex;
            std::mutex			frame_mutex[2];
            dispatch_semaphore_t acquire_semaphore;
            id<MTLLibrary>  defaultlibrary;
            MTLRenderPipelineDescriptor *pipelineStateDescriptor;
            id<MTLRenderPipelineState> quadRenderPipeline;
        public:
            void    waitIdle();
        public:
            id<MTLDevice> device;
            id<MTLCommandQueue> commandQueue;
            std::unordered_map<GpuSamplerDesc, id<MTLSamplerState>>    immutable_samplers;
            std::unique_ptr<struct GpuCommandBufferMetal>        setup_cb;
            std::shared_ptr<GpuBuffer>                crash_tracking_buffer;
        };
    }
}
