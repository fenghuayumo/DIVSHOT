#include "rasterize_mesh.h"
#include "defered_renderer.h"

namespace diverse
{
    namespace
    {
        struct MeshIndirectDrawResources
        {
            rg::Handle<rhi::GpuBuffer> args_buffer;
            rg::Handle<rhi::GpuBuffer> count_buffer;
            u32 draw_count = 0;
        };

        auto prepare_mesh_indirect_draws(
            rg::RenderGraph& rg,
            DeferedRenderer* renderer,
            const char* args_name,
            const char* count_name) -> MeshIndirectDrawResources
        {
            MeshIndirectDrawResources resources = {};
            resources.draw_count = static_cast<u32>(renderer->mesh_draw_data.size());
            if (resources.draw_count == 0)
                return resources;

            resources.args_buffer = rg.create<rhi::GpuBuffer>(
                rhi::GpuBufferDesc::new_gpu_only(
                    sizeof(rhi::IndirectDrawArgsInstanced) * resources.draw_count,
                    rhi::BufferUsageFlags::STORAGE_BUFFER | rhi::BufferUsageFlags::INDIRECT_BUFFER),
                args_name);

            resources.count_buffer = rg.create<rhi::GpuBuffer>(
                rhi::GpuBufferDesc::new_gpu_only(
                    sizeof(u32) * 4,
                    rhi::BufferUsageFlags::STORAGE_BUFFER | rhi::BufferUsageFlags::INDIRECT_BUFFER),
                count_name);

            rg::RenderPass::new_compute(
                rg.add_pass("mesh cull indirect"),
                "/shaders/mesh/mesh_cull_indirect.hlsl")
                .dynamic_storage_buffer_vec(renderer->mesh_draw_data)
                .dynamic_storage_buffer_vec(renderer->gpu_scene.instance_transforms)
                .write(resources.args_buffer)
                .write(resources.count_buffer)
                .constants(glm::uvec4(resources.draw_count, 0, 0, 0))
                .dispatch({ resources.draw_count, 1, 1 });

            return resources;
        }
    }

    RasterizeMesh::RasterizeMesh(class DeferedRenderer* render)
        : renderer(render)
    {
        auto device = renderer->get_device();
        rhi::RenderPassDesc desc = {
            {
                rhi::RenderPassAttachmentDesc::create(PixelFormat::R10G10B10A2_UNorm).clear_input(),
                rhi::RenderPassAttachmentDesc::create(PixelFormat::R32G32B32A32_Float).clear_input(),
                rhi::RenderPassAttachmentDesc::create(PixelFormat::R16G16B16A16_Float).clear_input()
            },
            rhi::RenderPassAttachmentDesc::create(PixelFormat::D32_Float)
        };
        raster_render_pass = device->create_render_pass(desc);
        desc = {
           {
               rhi::RenderPassAttachmentDesc::create(PixelFormat::R16G16B16A16_Float).load_input(),
           },
           rhi::RenderPassAttachmentDesc::create(PixelFormat::D32_Float)
        };
        wire_render_pass = device->create_render_pass(desc);
    }

    RasterizeMesh::~RasterizeMesh()
    {
    }

    auto RasterizeMesh::raster_gbuffer(rg::RenderGraph& rg, 
        GbufferDepth& gbuffer_depth,
        rg::Handle<rhi::GpuTexture>& velocity_img)->void
    {
        auto pass = rg.add_pass("raster gbuffer");

        auto pipeline_desc = rhi::RasterPipelineDesc()
            .with_render_pass(raster_render_pass)
            .with_vetex_attribute(false)
            .with_cull_mode(rhi::CullMode::NONE)
            .with_primitive_type(rhi::PrimitiveTopType::TriangleList);
        auto pipeline = pass.register_raster_pipeline(
            {
               rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Vertex).with_shader_source({"/shaders/rasterize_gbuffer_vs.hlsl"}),
               rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Pixel).with_shader_source({"/shaders/rasterize_gbuffer_ps.hlsl"})
            },
            std::move(pipeline_desc)
        );

        auto depth_ref = pass.raster(gbuffer_depth.depth, rhi::AccessType::DepthAttachmentWriteStencilReadOnly);
        auto geometric_normal_ref = pass.raster(
            gbuffer_depth.geometric_normal,
            rhi::AccessType::ColorAttachmentWrite
        );

        auto gbuffer_ref = pass.raster(gbuffer_depth.gbuffer, rhi::AccessType::ColorAttachmentWrite);
        auto velocity_ref = pass.raster(velocity_img, rhi::AccessType::ColorAttachmentWrite);
        auto indirect_resources = prepare_mesh_indirect_draws(
            rg,
            renderer,
            "mesh.indirect_args.gbuffer",
            "mesh.indirect_count.gbuffer");
        std::optional<rg::Ref<rhi::GpuBuffer, rg::GpuSrv>> indirect_args_ref;
        std::optional<rg::Ref<rhi::GpuBuffer, rg::GpuSrv>> indirect_count_ref;
        if (indirect_resources.draw_count > 0)
        {
            indirect_args_ref = pass.read(indirect_resources.args_buffer, rhi::AccessType::IndirectBuffer);
            indirect_count_ref = pass.read(indirect_resources.count_buffer, rhi::AccessType::IndirectBuffer);
        }

        pass.render([this, normal = std::move(geometric_normal_ref),
            velocity = std::move(velocity_ref),
            gbuffer = std::move(gbuffer_ref),
            depth = std::move(depth_ref),
            pipeline_raster = std::move(pipeline),
            indirect_args_ref = std::move(indirect_args_ref),
            indirect_count_ref = std::move(indirect_count_ref),
            indirect_draw_count = indirect_resources.draw_count](rg::RenderPassApi& api) {
                auto [width, height, _] = gbuffer.desc.extent;

                auto instance_transforms_offset = api.dynamic_constants()
                    ->push_from_vec(renderer->gpu_scene.instance_transforms);
                auto mesh_draw_data_offset = api.dynamic_constants()
                    ->push_from_vec(renderer->mesh_draw_data);

                api.begin_render_pass(
                    *raster_render_pass,
                    { width, height },
                {
                    std::pair{normal,rhi::GpuTextureViewDesc()},
                    std::pair{gbuffer,rhi::GpuTextureViewDesc()},
                    std::pair{velocity,rhi::GpuTextureViewDesc()}
                },
                    std::pair{ depth, rhi::GpuTextureViewDesc().with_aspect_mask(rhi::ImageAspectFlags::DEPTH) }
                );

                api.set_default_view_and_scissor({ width,height });

                std::vector<rg::RenderPassBinding> bindings = {
                    rg::RenderPassBinding::DynamicConstantsStorageBuffer(instance_transforms_offset),
                    rg::RenderPassBinding::DynamicConstantsStorageBuffer(mesh_draw_data_offset)
                };
                auto res = rg::RenderPassPipelineBinding<rg::RgRasterPipelineHandle>::from(pipeline_raster)
                    .descriptor_set(0, &bindings)
                    .raw_descriptor_set(1, renderer->binldess_descriptorset());
                auto bound_raster = api.bind_raster_pipeline(res);

                if (indirect_draw_count > 0 && indirect_args_ref && indirect_count_ref)
                {
                    bound_raster.indirect_draw_instanced_count(
                        *indirect_args_ref,
                        0,
                        *indirect_count_ref,
                        0,
                        indirect_draw_count);
                }

                api.end_render_pass();
            });

        pass.rg->record_pass(std::move(pass.pass));
    }

    auto RasterizeMesh::raster_wire_frame(rg::RenderGraph& rg,
        rg::Handle<rhi::GpuTexture>& color_img,
        rg::Handle<rhi::GpuTexture>& depth_img)->void
    {
        auto pass = rg.add_pass("raster wire frame");

        auto pipeline_desc = rhi::RasterPipelineDesc()
            .with_render_pass(wire_render_pass)
            .with_vetex_attribute(false)
            .with_cull_mode(rhi::CullMode::NONE)
            .with_polygon_mode(rhi::PolygonMode::WireFrame)
            .with_primitive_type(rhi::PrimitiveTopType::TriangleList);
        auto pipeline = pass.register_raster_pipeline(
            {
               rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Vertex).with_shader_source({"/shaders/rasterize_wireframe_vs.hlsl"}),
               rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Pixel).with_shader_source({"/shaders/rasterize_wireframe_ps.hlsl"})
            },
            std::move(pipeline_desc)
        );
        auto depth_ref = pass.raster(depth_img, rhi::AccessType::DepthAttachmentWriteStencilReadOnly);  
        auto color_ref = pass.raster(
            color_img,
            rhi::AccessType::ColorAttachmentWrite
        );
        auto indirect_resources = prepare_mesh_indirect_draws(
            rg,
            renderer,
            "mesh.indirect_args.wireframe",
            "mesh.indirect_count.wireframe");
        std::optional<rg::Ref<rhi::GpuBuffer, rg::GpuSrv>> indirect_args_ref;
        std::optional<rg::Ref<rhi::GpuBuffer, rg::GpuSrv>> indirect_count_ref;
        if (indirect_resources.draw_count > 0)
        {
            indirect_args_ref = pass.read(indirect_resources.args_buffer, rhi::AccessType::IndirectBuffer);
            indirect_count_ref = pass.read(indirect_resources.count_buffer, rhi::AccessType::IndirectBuffer);
        }

        pass.render([this, 
            color = std::move(color_ref),
            depth = std::move(depth_ref),
            pipeline_raster = std::move(pipeline),
            indirect_args_ref = std::move(indirect_args_ref),
            indirect_count_ref = std::move(indirect_count_ref),
            indirect_draw_count = indirect_resources.draw_count](rg::RenderPassApi& api) {
                auto [width, height, _] = depth.desc.extent;

                auto instance_transforms_offset = api.dynamic_constants()
                    ->push_from_vec(renderer->gpu_scene.instance_transforms);
                auto mesh_draw_data_offset = api.dynamic_constants()
                    ->push_from_vec(renderer->mesh_draw_data);

                api.begin_render_pass(
                    *wire_render_pass,
                    { width, height },
                    {
                        std::pair{color,rhi::GpuTextureViewDesc()},
                    },
                    std::pair{ depth, rhi::GpuTextureViewDesc().with_aspect_mask(rhi::ImageAspectFlags::DEPTH) }
                );

                api.set_default_view_and_scissor({ width,height });

                std::vector<rg::RenderPassBinding> bindings = {
                    rg::RenderPassBinding::DynamicConstantsStorageBuffer(instance_transforms_offset),
                    rg::RenderPassBinding::DynamicConstantsStorageBuffer(mesh_draw_data_offset)
                };
                auto res = rg::RenderPassPipelineBinding<rg::RgRasterPipelineHandle>::from(pipeline_raster)
                    .descriptor_set(0, &bindings)
                    .raw_descriptor_set(1, renderer->binldess_descriptorset());
                auto bound_raster = api.bind_raster_pipeline(res);

                if (indirect_draw_count > 0 && indirect_args_ref && indirect_count_ref)
                {
                    bound_raster.indirect_draw_instanced_count(
                        *indirect_args_ref,
                        0,
                        *indirect_count_ref,
                        0,
                        indirect_draw_count);
                }

                api.end_render_pass();
            });
        pass.rg->record_pass(std::move(pass.pass));
    }
}
