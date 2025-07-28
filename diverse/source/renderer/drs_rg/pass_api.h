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

            auto push_constants(rhi::CommandBuffer* cb, u32 offset, u8* constants, u32 size_) -> void;
        };
        struct BoundRayTracingPipeline
        {
            RenderPassApi& api;
            std::shared_ptr<rhi::RayTracingPipeline>   pipeline;

            auto trace_rays(const std::array<u32, 3>& threads) -> void;

            auto trace_rays_indirect(const Ref<rhi::GpuBuffer, GpuSrv>& args_buffer,u64 args_buffer_offset) -> void;
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

            auto bind_pipeline_common(rhi::GpuDevice* device,rhi::GpuPipeline* pipeline,const RenderPassCommonShaderPipelineBinding& binding)->void;

            auto begin_render_pass(rhi::RenderPass& render_pass, 
                                   const std::array<u32,2>& dims,
                                   const std::vector<std::pair<Ref<rhi::GpuTexture, GpuRt>, rhi::GpuTextureViewDesc>>& color_attachments,
                                   const std::pair<Ref<rhi::GpuTexture, GpuRt>, rhi::GpuTextureViewDesc>& depth_attch)->void;

            auto end_render_pass()->void;

            auto set_default_view_and_scissor(const std::array<u32,2>& extents)->void;


        };
    }
}
