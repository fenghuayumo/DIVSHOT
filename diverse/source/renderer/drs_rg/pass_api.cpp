#include "pass_api.h"

namespace diverse
{
    namespace rg
    {
        auto RenderPassApi::device() -> rhi::GpuDevice*
        {
            return resources.execution_params.device;
        }
        auto RenderPassApi::dynamic_constants() -> rhi::DynamicConstants*
        {
            return resources.dynamic_constants;
        }

        auto RenderPassApi::bind_compute_pipeline(const RenderPassPipelineBinding<RgComputePipelineHandle>& binding) -> BoundComputePipeline
        {
           auto device = this->resources.execution_params.device;
           auto pipeline = resources.compute_pipeline(binding.pipeline);
           bind_pipeline_common(device, pipeline.get(), binding.binding);

           return {
               *this,
               pipeline
           };
        }

        auto RenderPassApi::bind_raster_pipeline(const RenderPassPipelineBinding<RgRasterPipelineHandle>& binding) -> BoundRasterPipeline
        {
            auto device = this->resources.execution_params.device;
            auto pipeline = resources.raster_pipeline(binding.pipeline);
            bind_pipeline_common(device, pipeline.get(), binding.binding);

            return {
                *this,
                pipeline
            };
        }

        auto RenderPassApi::bind_ray_tracing_pipeline(const RenderPassPipelineBinding<RgRtPipelineHandle>& binding) -> BoundRayTracingPipeline
        {
            auto device = this->resources.execution_params.device;
            auto pipeline = resources.ray_tracing_pipeline(binding.pipeline);
            bind_pipeline_common(device, pipeline.get(), binding.binding);

            return {
                *this,
                pipeline
            };
        }

        auto RenderPassApi::bind_mesh_shader_pipeline(const RenderPassPipelineBinding<RgMeshShaderPipelineHandle>& binding) -> BoundMeshShaderPipeline
        {
            auto device = this->resources.execution_params.device;
            auto pipeline = resources.mesh_shader_pipeline(binding.pipeline);
            bind_pipeline_common(device, pipeline.get(), binding.binding);

            return {
                *this,
                pipeline
            };
        }
        
        auto RenderPassApi::bind_pipeline_common(rhi::GpuDevice* device, rhi::GpuPipeline* pipeline, const RenderPassCommonShaderPipelineBinding& binding) -> void
        {
            device->bind_pipeline(cb, pipeline);
            //bind frame constants
            auto& frame_constants_layout = resources.execution_params.frame_constants_layout;
            std::array<u32, 4> dynamic_offset = { frame_constants_layout .globals_offset, frame_constants_layout .instance_dynamic_parameters_offset, frame_constants_layout.triangle_lights_offset, frame_constants_layout .scene_lights_offset};
            device->bind_descriptor_set(cb, pipeline, 2, resources.execution_params.frame_descriptor_set, dynamic_offset.size(), dynamic_offset.data());

            for (auto& [set_idx, bindings] : binding.bindings)
            {
                std::vector<rhi::DescriptorSetBinding> set_bindings;
                for (auto& binding : *bindings) {
                    switch ( binding.ty)
                    {
                    case RenderPassBinding::Type::Image:
                    {
                        auto& image = binding.Image();
                        set_bindings.emplace_back(rhi::DescriptorSetBinding::Image(
                            rhi::DescriptorImageInfo{
                                image.image_layout,
                                resources.image_view(std::move(image.handle), image.view_desc)
                            }
                        ));
                    }
                    break;
                    case RenderPassBinding::Type::Buffer:
                    {
                        auto& buffer = binding.Buffer();
                        set_bindings.emplace_back(rhi::DescriptorSetBinding::Buffer(
                            rhi::DescriptorBufferInfo{
                               resources.buffer_from_raw_handle<GpuSrv>(buffer.handle),
                               0,
                               std::numeric_limits<u64>::max()
                            }
                        ));
                    }
                    break;
                    case RenderPassBinding::Type::ImageArray:
                    {
                        auto& iamge_array = binding.ImageArray();
                        std::vector<rhi::DescriptorImageInfo>   image_infos;
                        for (auto& image : iamge_array)
                        {
                            image_infos.emplace_back(rhi::DescriptorImageInfo{
                                    image.image_layout,
                                    resources.image_view(std::move(image.handle), image.view_desc)
                            });
                        }
                        set_bindings.emplace_back(rhi::DescriptorSetBinding::ImageArray(
                            image_infos
                        ));
                    }
                    break;
                    case RenderPassBinding::Type::RayTracingAcceleration:
                    {
                        auto& acc = binding.RayTracingAcceleration();
                        set_bindings.emplace_back(rhi::DescriptorSetBinding::RayTracingAcceleration(resources.rt_acceleration_from_raw_handle<GpuSrv>(acc.handle)));
                    }
                    break;
                    case RenderPassBinding::Type::DynamicConstants:
                    {
                        auto& offset = binding.DynamicConstants();
                        set_bindings.emplace_back(rhi::DescriptorSetBinding::DynamicBuffer(
                                {resources.dynamic_constants->buffer.get(), offset}
                        ));
                    }
                    break;
                    case RenderPassBinding::Type::DynamicConstantsStorageBuffer:
                    {
                        auto& offset = binding.DynamicConstantsStorageBuffer();
                        set_bindings.emplace_back(rhi::DescriptorSetBinding::DynamicStorageBuffer(
                            { resources.dynamic_constants->buffer.get(), offset }
                        ));
                    }
                    break;
                    default:
                        break;
                    }
                }
                device->bind_descriptor_set(cb, pipeline, set_bindings, set_idx);
            }
      

            for (auto [set_idx, binding] : binding.raw_bindings)
            {
                device->bind_descriptor_set(cb, pipeline, set_idx,binding ,0,nullptr);
            }
        }

        auto RenderPassApi::begin_render_pass(rhi::RenderPass& render_pass,
            const std::array<u32, 2>& dims,
            const std::vector<std::pair<Ref<rhi::GpuTexture, GpuRt>, rhi::GpuTextureViewDesc>>& color_attachments,
            const std::pair<Ref<rhi::GpuTexture, GpuRt>, rhi::GpuTextureViewDesc>& depth_attch) -> void
        {
            auto device = resources.execution_params.device;
            std::vector<rhi::GpuTexture*> color_texs(color_attachments.size());
            for(size_t i = 0; i < color_attachments.size(); i++)
                color_texs[i] = resources.image(color_attachments[i].first);

            device->begin_render_pass(cb,dims ,&render_pass, color_texs, resources.image(depth_attch.first));
        }

        auto RenderPassApi::begin_render_pass(rhi::RenderPass& render_pass, 
            const std::array<u32,2>& dims,
            const std::vector<Ref<rhi::GpuTexture, GpuRt>>& color_attachments,
            const Ref<rhi::GpuTexture, GpuRt>& depth_attch) -> void
        {
            auto device = resources.execution_params.device;
            std::vector<rhi::GpuTexture*> color_texs(color_attachments.size());
            for(size_t i = 0; i < color_attachments.size(); i++)
                color_texs[i] = resources.image(color_attachments[i]);

            device->begin_render_pass(cb, dims ,&render_pass, color_texs, resources.image(depth_attch));
        }

        auto RenderPassApi::end_render_pass() -> void
        {
            auto device = resources.execution_params.device;
            device->end_render_pass(cb);
        }

        auto RenderPassApi::set_default_view_and_scissor(const std::array<u32, 2>& extents) -> void
        {
            auto device = resources.execution_params.device;
            
            device->set_viewport(cb, rhi::ViewPort{0, (f32)extents[1],
                    (f32)extents[0], -(f32)extents[1],
                     0.0f,1.0f},
                    rhi::Scissor{
                        {0,0},
                        {extents[0], extents[1]}
                    });
        }



        auto BoundComputePipeline::dispatch(const std::array<u32, 3>& threads)->void
        {
            auto group_size = pipeline->group_size;

            api.device()->dispatch(
                api.cb,
                {
                (threads[0] + group_size[0] - 1) / group_size[0],
                (threads[1] + group_size[1] - 1) / group_size[1],
                (threads[2] + group_size[2] - 1) / group_size[2]
                },
                group_size
            );
        }

        auto BoundComputePipeline::dispatch_indirect(Ref<rhi::GpuBuffer, GpuSrv>&& args_buffer, u64 args_buffer_offset)->void
        {
            api.device()->dispatch_indirect(api.cb,
                api.resources.buffer(args_buffer),
                args_buffer_offset);
        }

        auto BoundComputePipeline::push_constants(rhi::CommandBuffer* cb, u32 offset, u8* constants, u32 size_)->void
        {
            api.resources.execution_params.device->push_constants(
                cb,
                pipeline.get(),
                offset,
                constants,
                size_
            );
        }
        auto BoundRasterPipeline::draw_instanced(const u32& vertex_count, const u32& instance_count) -> void
        {
            api.device()->draw_instanced(api.cb, vertex_count, instance_count, 0, 0);
        }
        auto BoundRasterPipeline::indirect_draw_instanced(const Ref<rhi::GpuBuffer, GpuSrv>& args_buffer, u64 args_buffer_offset) -> void
        {
            api.device()->draw_instanced_indirect(api.cb,
                api.resources.buffer(args_buffer),
                args_buffer_offset);
        }

        auto BoundRasterPipeline::push_constants(rhi::CommandBuffer* cb, u32 offset, u8* constants, u32 size_) -> void
        {
            api.resources.execution_params.device->push_constants(
                cb,
                pipeline.get(),
                offset,
                constants,
                size_
            );
        }

        auto BoundRayTracingPipeline::trace_rays(const std::array<u32, 3>& threads) -> void
        {
            api.device()->trace_rays(api.cb, pipeline.get(), threads);
        }

        auto BoundRayTracingPipeline::trace_rays_indirect(const Ref<rhi::GpuBuffer, GpuSrv>& args_buffer, u64 args_buffer_offset) -> void
        {
            auto indirect_device_address = api.resources.buffer(args_buffer)->device_address(api.device()) + args_buffer_offset;
            {
                api.device()->trace_rays_indirect(api.cb,
                    pipeline.get(),
                    indirect_device_address);
            }
        }

        auto BoundMeshShaderPipeline::draw_mesh_tasks(uint32 group_count_x, uint32 group_count_y, uint32 group_count_z) -> void
        {
            api.device()->draw_mesh_tasks(api.cb, group_count_x, group_count_y, group_count_z);
        }

        auto BoundMeshShaderPipeline::draw_mesh_tasks_indirect(const Ref<rhi::GpuBuffer, GpuSrv>& args_buffer, u64 args_buffer_offset, uint32 draw_count, uint32 stride) -> void
        {
            api.device()->draw_mesh_tasks_indirect(api.cb,
                api.resources.buffer(args_buffer),
                args_buffer_offset,
                draw_count,
                stride);
        }

        auto BoundMeshShaderPipeline::draw_mesh_tasks_indirect_count(const Ref<rhi::GpuBuffer, GpuSrv>& args_buffer, u64 args_buffer_offset,
                                            const Ref<rhi::GpuBuffer, GpuSrv>& count_buffer, u64 count_buffer_offset,
                                            uint32 max_count, uint32 stride) -> void
        {
            api.device()->draw_mesh_tasks_indirect_count(api.cb,
                api.resources.buffer(args_buffer),
                args_buffer_offset,
                api.resources.buffer(count_buffer),
                count_buffer_offset,
                max_count,
                stride);
        }

        auto BoundMeshShaderPipeline::push_constants(rhi::CommandBuffer* cb, u32 offset, u8* constants, u32 size_) -> void
        {
            api.resources.execution_params.device->push_constants(
                cb,
                pipeline.get(),
                offset,
                constants,
                size_
            );
        }

}
}
