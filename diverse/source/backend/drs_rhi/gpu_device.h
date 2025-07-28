#pragma once
#include "drs_rhi.h"
#include "gpu_buffer.h"
#include "gpu_texture.h"
#include "gpu_raytracing.h"
#include "gpu_render_pass.h"
#include "gpu_pipeline_cache.h"
#include "gpu_cmd_buffer.h"
#include "gpu_swapchain.h"
#include "gpu_barrier.h"
#include "dynamic_constants.h"
#include <memory>

#define RESERVED_DESCRIPTOR_COUNT 32

namespace diverse
{
    namespace rhi
    {
        struct IndirectDrawArgsInstanced
        {
            uint32_t vertex_count_per_instance = 0;
            uint32_t instance_count = 0;
            uint32_t start_vertex_location = 0;
            uint32_t start_instance_location = 0;
        };

        struct IndirectDrawArgsIndexedInstanced
        {
            uint32_t index_count_per_instance = 0;
            uint32_t instance_count = 0;
            uint32_t start_index_location = 0;
            int32_t base_vertex_location = 0;
            uint32_t start_instance_location = 0;
        };

        struct IndirectDispatchArgs
        {
            uint32_t thread_group_count_x = 0;
            uint32_t thread_group_count_y = 0;
            uint32_t thread_group_count_z = 0;
        };

        struct DeviceFrame
        {
            DeviceFrame(const std::shared_ptr<CommandBuffer>& main_cb,const std::shared_ptr<CommandBuffer>& present_cb)
            : main_cmd_buf(main_cb), presentation_cmd_buf(present_cb)
            {}
            DeviceFrame() = default;
            std::shared_ptr<CommandBuffer> main_cmd_buf;
            std::shared_ptr<CommandBuffer> presentation_cmd_buf;
        };

        struct ViewPort
        {
            f32 x = 0;
            f32 y = 0;
            f32 width = 1;
            f32 height = 1;
            f32 min_depth = 0.0f;
            f32 max_depth = 1.0f;
        };

        struct Scissor
        {
            std::array<i32,2> offset;
            std::array<u32, 2> extent;
        };

        struct GpuLimits
        {
            u32 max_image_dimension1_d;
            u32 max_image_dimension2_d;
            u32 max_image_dimension3_d;
            u32 max_image_dimension_cube;
            u32 max_per_stage_descriptor_sampled_images;
            u32 max_per_stage_descriptor_storage_images;
            u32 max_per_stage_descriptor_storage_buffers;
            u32 max_per_stage_descriptor_unifrom_texel_buffers;
            u64 minUniformBufferOffsetAlignment;
            u64 minStorageBufferOffsetAlignment;
            u64 minTexelBufferOffsetAlignment;
            u32 maxDescriptorSetUpdateAfterBindStorageBuffers;
            u32 maxDescriptorSetUpdateAfterBindUniformBuffers;
            u32 maxDescriptorSetUpdateAfterBindStorageImages;
            u32 maxDescriptorSetUpdateAfterBindSampledImages;
            u32 maxPerStageDescriptorUpdateAfterBindSampledImages;
            u32 maxPerStageDescriptorUpdateAfterBindStorageImages;
            u32 maxPerStageDescriptorUpdateAfterBindStorageBuffers;
            u32 maxPerStageDescriptorUpdateAfterBindUniformBuffers;
            u32 minImportedHostPointerAlignment = 4096;
            bool rayQuery = false;
            bool ray_tracing_enabled = false;
            bool support_bindless = true;
            u64  vram_size = 0;
        };
        enum class IndexBufferFormat
        {
            UINT16,
            UINT32,
        };
        struct DeferedReleaseResource
        {
			std::function<void()> release;
            u16   frame_counter = 0;
            bool is_can_release() const
            {
                //after 20 frames release
				return frame_counter > 20;
			}
		};

        struct GpuDevice
        {
        public:
            virtual ~GpuDevice() {}
        public:
            auto max_bindless_storage_images()const->u32
            {
                return std::min<u32>(512 * 1024, gpu_limits.max_per_stage_descriptor_storage_images - RESERVED_DESCRIPTOR_COUNT);
            }
            auto max_bindless_sampled_images()const->u32
            {
                return std::min<u32>(512 * 1024, gpu_limits.max_per_stage_descriptor_sampled_images - RESERVED_DESCRIPTOR_COUNT);
            }
            auto max_bindless_storage_buffers()const->u32
            {
                return std::min<u32>(512 * 1024, gpu_limits.max_per_stage_descriptor_storage_buffers - RESERVED_DESCRIPTOR_COUNT);
            }
            auto max_bindless_uniform_textel_buffers()const->u32
            {
                return std::min<u32>(512 * 1024, gpu_limits.max_per_stage_descriptor_unifrom_texel_buffers - RESERVED_DESCRIPTOR_COUNT);
            }
        public:
            virtual std::shared_ptr<GpuTexture>  create_texture(const GpuTextureDesc& desc,const std::vector<ImageSubData>&  initial_data,const char* name = nullptr) = 0;
            virtual std::shared_ptr<GpuBuffer>   create_buffer(const GpuBufferDesc& desc, const char* name, uint8* initial_data) = 0;
            
            virtual auto create_render_command_buffer(const char* name = nullptr) -> std::shared_ptr<CommandBuffer> = 0;
            virtual auto create_swapchain(SwapchainDesc desc, void* window_handle) -> std::shared_ptr<Swapchain> = 0;
            virtual auto create_descriptor_set(GpuBuffer* dynamic_constants,const std::unordered_map<u32,DescriptorInfo>& descriptors,const char* name = nullptr)->std::shared_ptr<DescriptorSet> = 0;
            virtual auto create_descriptor_set(const std::unordered_map<u32, DescriptorInfo>& descriptors,const char* name = nullptr) -> std::shared_ptr<DescriptorSet> = 0;
            virtual auto bind_descriptor_set(CommandBuffer* cb,GpuPipeline* pipeline,std::vector<DescriptorSetBinding>& bindings,uint32 set_index)->void = 0;
            virtual auto bind_descriptor_set(CommandBuffer* cb,GpuPipeline* pipeline,uint32 set_idx,DescriptorSet* set, u32 dynamic_offset_count, u32* dynamic_offset)->void = 0;
            virtual auto bind_pipeline(CommandBuffer* cb, GpuPipeline* pipeline)->void = 0;
            virtual auto push_constants(CommandBuffer* cb, GpuPipeline* pipeline, u32 offset, u8* constants, u32 size_) -> void = 0;

            virtual auto create_compute_pipeline(const CompiledShaderCode& spirv, const ComputePipelineDesc& desc)->std::shared_ptr<ComputePipeline> = 0;
            virtual auto create_raster_pipeline(const std::vector<PipelineShader>& shaders,const RasterPipelineDesc& desc)->std::shared_ptr<RasterPipeline> = 0;
            virtual auto create_ray_tracing_pipeline(const std::vector<PipelineShader>& shaders, const RayTracingPipelineDesc& desc) -> std::shared_ptr<RayTracingPipeline> = 0;
            virtual auto create_render_pass(const RenderPassDesc& desc,const char* name = nullptr) -> std::shared_ptr<RenderPass> = 0;

            virtual auto begin_frame()->DeviceFrame* = 0;
            virtual auto end_frame(DeviceFrame* device)->void = 0;
            virtual auto begin_cmd(CommandBuffer* cb)->void = 0;
            virtual auto end_cmd(CommandBuffer* cb)->void = 0;
            virtual auto submit_cmd(CommandBuffer* cb)->void = 0;
            virtual auto execute_cmd(CommandBuffer* cb)->void = 0;
            virtual auto record_image_barrier(CommandBuffer* cb,const ImageBarrier& barrier)->void = 0;
            virtual auto record_buffer_barrier(CommandBuffer* cb, const BufferBarrier& barrier)->void = 0 ;
            virtual auto record_global_barrier(CommandBuffer* cb, const std::vector<rhi::AccessType>& previous_accesses, const std::vector<rhi::AccessType>& next_accesses)->void = 0;
            virtual auto dispatch(CommandBuffer* cb, const std::array<u32, 3>& group_dim, const std::array<u32, 3>& group_size)->void = 0;
            virtual auto dispatch_indirect(CommandBuffer* cb,GpuBuffer* args_buffer, u64 args_buffer_offset)->void = 0;
            virtual auto write_descriptor_set(DescriptorSet* descriptor_set, u32 dst_binding, rhi::GpuBuffer* buffer, u32 array_index = 0)->void = 0;
            virtual auto write_descriptor_set(DescriptorSet* descriptor_set, u32 dst_binding, u32 rray_index, const rhi::DescriptorImageInfo& img_info) -> void = 0;
            virtual auto clear_depth_stencil(CommandBuffer* cb, GpuTexture* texture,float depth,u32 stencil)->void = 0;
            virtual auto clear_color(CommandBuffer* cb, GpuTexture* texture,const std::array<f32,4>& color) -> void = 0;
            virtual auto copy_image(GpuTexture* src, GpuTexture* dst,CommandBuffer* cmd_buf) -> void = 0;
            virtual auto copy_image(GpuBuffer* src, GpuTexture* dst, CommandBuffer* cmd_buf) -> void = 0;
            virtual auto update_texture(GpuTexture* src, const std::vector<ImageSubData>& update_data, const TextureRegion& tex_region)->void = 0;
            virtual auto set_point_size(CommandBuffer* cb,float point_size = 1.0f)->void = 0;
            virtual auto set_line_width(CommandBuffer* cb, float line_width = 1.0f)->void = 0;
            virtual auto set_viewport(CommandBuffer* cb,const ViewPort& view, const Scissor& scissors)->void = 0;
            virtual auto begin_render_pass(CommandBuffer* cb, const std::array<u32, 2>& dims, RenderPass* render_pass, const std::vector<rhi::GpuTexture*>& color_desc, rhi::GpuTexture* depth_desc)->void = 0;
            virtual auto end_render_pass(CommandBuffer* cb)->void = 0;

            virtual auto create_ray_tracing_bottom_acceleration(const RayTracingBottomAccelerationDesc& desc)-> std::shared_ptr<GpuRayTracingAcceleration> = 0;
            virtual auto create_ray_tracing_top_acceleration(const RayTracingTopAccelerationDesc& desc,const RayTracingAccelerationScratchBuffer& scratch_buffer)-> std::shared_ptr<GpuRayTracingAcceleration> = 0;
            virtual auto rebuild_ray_tracing_top_acceleration(CommandBuffer* cb, u64 instance_buffer_address,u64 instance_count,GpuRayTracingAcceleration* tlas, RayTracingAccelerationScratchBuffer* scratch_buffer)->void = 0;

            virtual auto bind_vertex_buffers(CommandBuffer* cb,const GpuBuffer* const* vertexBuffers, uint32_t slot, uint32_t count, const uint32_t* strides, const uint64_t* offsets)->void = 0;
            virtual auto bind_index_buffer(CommandBuffer* cb, const GpuBuffer* indexBuffer, const IndexBufferFormat format, uint64_t offset)->void = 0;
            virtual auto draw(CommandBuffer* cb,uint32_t vertexCount, uint32_t startVertexLocation) ->void = 0;
            virtual auto draw_indexed(CommandBuffer* cb, uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation)->void = 0;
            virtual auto draw_instanced(CommandBuffer* cb, uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation)->void = 0;
            virtual auto draw_indexed_instanced(CommandBuffer* cb, uint32_t indexCount, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocationd)->void = 0;
            virtual auto draw_instanced_indirect(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset)->void = 0;
            virtual auto draw_indexed_instanced_indirect(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset)->void = 0;
            virtual auto draw_instanced_indirect_count(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset, const GpuBuffer* count, uint64_t count_offset, uint32_t max_count)->void = 0;
            virtual auto draw_indexed_instanced_indirect_count(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset, const GpuBuffer* count, uint64_t count_offset, uint32_t max_count)->void = 0;

            virtual auto with_setup_cb(std::function<void(CommandBuffer* cmd)>&& callback)->void = 0;
            virtual auto copy_buffer(CommandBuffer* cmd,GpuBuffer* src,u64 src_offset, GpuBuffer* dst, u64 dst_offset,u64 size_)->void = 0;
            virtual auto trace_rays(CommandBuffer* cb, RayTracingPipeline* rtpipeline, const std::array<u32,3>& threads)->void = 0;
            virtual auto trace_rays_indirect(CommandBuffer* cb,RayTracingPipeline* rtpipeline, u64 args_buffer_address) -> void = 0;
            virtual auto event_begin(const char* name,CommandBuffer* cb)->void = 0;
            virtual auto event_end(CommandBuffer* cb) -> void = 0;
            virtual auto set_name(GpuResource* resource,const char* name)const->void = 0;
            virtual auto destroy_resource(GpuResource* resource)->void = 0;
            virtual auto defer_release(std::function<void()> f)->void = 0;
            virtual auto export_image(rhi::GpuTexture* image)->std::vector<u8> = 0;
            virtual auto blit_image(rhi::GpuTexture* src,rhi::GpuTexture* dst,CommandBuffer* cmd_buf = nullptr) ->void = 0;
        public:
            virtual auto get_graphics_cmd_buffer()->CommandBuffer* {return nullptr;};
        public:
            auto inline create_ray_tracing_acceleration_scratch_buffer() -> RayTracingAccelerationScratchBuffer
            {
                auto buffer_desc = rhi::GpuBufferDesc::new_gpu_only(1024 * 256 * sizeof(RayTracingInstanceDesc), BufferUsageFlags::STORAGE_BUFFER | BufferUsageFlags::SHADER_DEVICE_ADDRESS);
                auto buffer = create_buffer(buffer_desc, "Acceleration structure scratch buffer", nullptr);

                return { buffer };
            }

            auto fill_ray_tracing_instance_buffer(DynamicConstants* dynamic_constants, const std::vector<RayTracingInstanceDesc>& instances) -> u64
            {
                auto instance_buffer_address = dynamic_constants->current_device_address(this);

                std::vector< GeometryInstance>  geo_instances;
                for (auto& inst_desc : instances)
                {
                    auto blas_address = inst_desc.blas->as_device_address(this);

                    auto transform = inst_desc.transformation;
                    geo_instances.push_back(GeometryInstance(transform, inst_desc.mesh_index, inst_desc.mask, 0, GeometryInstanceFlag::TRIANGLE_FACING_CULL_DISABLE, blas_address));
                }
                dynamic_constants->push_from_vec(geo_instances);

                return instance_buffer_address;
            }

            GpuLimits gpu_limits;
            std::shared_ptr<Swapchain> swapchain;
            RayTracingAccelerationScratchBuffer rt_scatch_buffer;
        };
    }
}
