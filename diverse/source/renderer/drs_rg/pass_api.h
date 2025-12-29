#pragma once

#include "resource_registry.h"

namespace diverse
{
    namespace rg
    {
        
        struct RenderPassCommonShaderPipelineBinding
        {
            std::vector<std::pair<u32, std::vector<RenderPassBinding>*>> bindings;
            std::vector<std::pair<u32, rhi::DescriptorSet*>> raw_bindings;

        };
        template<typename HandleType>
        struct RenderPassPipelineBinding
        {
            HandleType pipeline;
            RenderPassCommonShaderPipelineBinding binding;

            static auto from(HandleType const& handle) -> RenderPassPipelineBinding<HandleType>
            {
                return {
                    handle
                };
            }

            auto descriptor_set(u32 set_idx, std::vector<RenderPassBinding>* bindings)->RenderPassPipelineBinding<HandleType>&
            {
                this->binding.bindings.push_back({set_idx, bindings});
                return *this;
            }

            auto raw_descriptor_set(u32 set_idx, rhi::DescriptorSet* binding) ->RenderPassPipelineBinding<HandleType>&
            {
                this->binding.raw_bindings.push_back({ set_idx, binding });
                return *this;
            }
        };

        struct RenderPassApi;

        struct BoundComputePipeline
        {
            RenderPassApi& api;
            std::shared_ptr<rhi::ComputePipeline>   pipeline;

            auto dispatch(const std::array<u32, 3>& threads)->void;

            auto dispatch_indirect(Ref<rhi::GpuBuffer, GpuSrv>&& args_buffer,u64 args_buffer_offset)->void;

            auto push_constants(rhi::CommandBuffer* cb, u32 offset, u8* constants, u32 size_)->void;

        };

        struct BoundRasterPipeline
        {
            RenderPassApi& api;
            std::shared_ptr<rhi::RasterPipeline>   pipeline;

            auto draw_instanced(const u32& vertex_count,const u32& instance_count) -> void;
            auto indirect_draw_instanced(const Ref<rhi::GpuBuffer, GpuSrv>& args_buffer, u64 args_buffer_offset) -> void;
            auto push_constants(rhi::CommandBuffer* cb, u32 offset, u8* constants, u32 size_) -> void;
        };
        struct BoundRayTracingPipeline
        {
            RenderPassApi& api;
            std::shared_ptr<rhi::RayTracingPipeline>   pipeline;

            auto trace_rays(const std::array<u32, 3>& threads) -> void;

            auto trace_rays_indirect(const Ref<rhi::GpuBuffer, GpuSrv>& args_buffer,u64 args_buffer_offset) -> void;
        };

        struct BoundMeshShaderPipeline
        {
            RenderPassApi& api;
            std::shared_ptr<rhi::MeshShaderPipeline>   pipeline;

            auto draw_mesh_tasks(uint32 group_count_x, uint32 group_count_y, uint32 group_count_z) -> void;
            auto draw_mesh_tasks_indirect(const Ref<rhi::GpuBuffer, GpuSrv>& args_buffer, u64 args_buffer_offset, uint32 draw_count, uint32 stride) -> void;
            auto draw_mesh_tasks_indirect_count(const Ref<rhi::GpuBuffer, GpuSrv>& args_buffer, u64 args_buffer_offset,
                                                const Ref<rhi::GpuBuffer, GpuSrv>& count_buffer, u64 count_buffer_offset,
                                                uint32 max_count, uint32 stride) -> void;
            auto push_constants(rhi::CommandBuffer* cb, u32 offset, u8* constants, u32 size_) -> void;
        };

        struct RenderPassApi
        {

            rhi::CommandBuffer* cb;
            ResourceRegistry&    resources;


            auto device()->rhi::GpuDevice*;

            auto dynamic_constants()->rhi::DynamicConstants*;

            auto bind_compute_pipeline(const RenderPassPipelineBinding<RgComputePipelineHandle>& binding)-> BoundComputePipeline;

            auto bind_raster_pipeline(const RenderPassPipelineBinding<RgRasterPipelineHandle>& binding)-> BoundRasterPipeline;

            auto bind_ray_tracing_pipeline(const RenderPassPipelineBinding<RgRtPipelineHandle>& binding)-> BoundRayTracingPipeline;

            auto bind_mesh_shader_pipeline(const RenderPassPipelineBinding<RgMeshShaderPipelineHandle>& binding)-> BoundMeshShaderPipeline;

            auto bind_pipeline_common(rhi::GpuDevice* device,rhi::GpuPipeline* pipeline,const RenderPassCommonShaderPipelineBinding& binding)->void;

            auto begin_render_pass(rhi::RenderPass& render_pass, 
                                   const std::array<u32,2>& dims,
                                   const std::vector<std::pair<Ref<rhi::GpuTexture, GpuRt>, rhi::GpuTextureViewDesc>>& color_attachments,
                                   const std::pair<Ref<rhi::GpuTexture, GpuRt>, rhi::GpuTextureViewDesc>& depth_attch)->void;
            auto begin_render_pass(rhi::RenderPass& render_pass, 
                                    const std::array<u32,2>& dims,
                                    const std::vector<Ref<rhi::GpuTexture, GpuRt>>& color_attachments,
                                    const Ref<rhi::GpuTexture, GpuRt>& depth_attch)->void;
            auto end_render_pass()->void;

            auto set_default_view_and_scissor(const std::array<u32,2>& extents)->void;


        };
    }
}
