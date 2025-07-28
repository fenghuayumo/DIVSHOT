#include "rasterize_mesh.h"
#include "defered_renderer.h"
#include "assets/mesh.h"
namespace diverse
{
    RasterizeMesh::RasterizeMesh(struct DeferedRenderer* render)
        : renderer(render)
    {
        rhi::RenderPassDesc desc = {
            {
                rhi::RenderPassAttachmentDesc::create(PixelFormat::R10G10B10A2_UNorm).clear_input(),
                rhi::RenderPassAttachmentDesc::create(PixelFormat::R32G32B32A32_Float).clear_input(),
                rhi::RenderPassAttachmentDesc::create(PixelFormat::R16G16B16A16_Float).clear_input()
            },
            rhi::RenderPassAttachmentDesc::create(PixelFormat::D32_Float)
        };
        raster_render_pass = g_device->create_render_pass(desc);
        desc = {
           {
               rhi::RenderPassAttachmentDesc::create(PixelFormat::R16G16B16A16_Float).load_input(),
           },
           rhi::RenderPassAttachmentDesc::create(PixelFormat::D32_Float)
        };
        wire_render_pass = g_device->create_render_pass(desc);
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
            .with_primitive_type(rhi::PrimitiveTopType::TriangleList)
            .with_push_constants(2 * sizeof(u32));
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

        pass.render([this, normal = std::move(geometric_normal_ref),
            velocity = std::move(velocity_ref),
            gbuffer = std::move(gbuffer_ref),
            depth = std::move(depth_ref),
            pipeline_raster = std::move(pipeline)](rg::RenderPassApi& api) {
                auto [width, height, _] = gbuffer.desc.extent;

                auto instance_transforms_offset = api.dynamic_constants()
                    ->push_from_vec(renderer->instantce_transforms);

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

                std::vector<rg::RenderPassBinding> bindings = { rg::RenderPassBinding::DynamicConstantsStorageBuffer(instance_transforms_offset) };
                auto res = rg::RenderPassPipelineBinding<rg::RgRasterPipelineHandle>::from(pipeline_raster)
                    .descriptor_set(0, &bindings)
                    .raw_descriptor_set(1, renderer->binldess_descriptorset());
                auto bound_raster = api.bind_raster_pipeline(res);

                auto device = api.device();
                const auto& mesh_cmds = renderer->mesh_command_queue;
                for (u32 draw_idx = 0; draw_idx < mesh_cmds.size(); draw_idx++)
                {
                    const auto& cmd = mesh_cmds[draw_idx];
                    auto mesh = renderer->mesh_buf_id_2_mesh[cmd.mesh_id];
                    device->bind_index_buffer(api.cb, mesh->get_index_buffer().get(), rhi::IndexBufferFormat::UINT32, 0);
                    struct PushConstants {
                        u32 draw_index;
                        u32 mesh_index;
                    } push_constants;
                    
        
                    push_constants.draw_index = cmd.mesh_instance_id;
                    push_constants.mesh_index = cmd.mesh_id;
                    bound_raster.push_constants(
                        api.cb,
                        0,
                        (u8*)&push_constants,
                        sizeof(PushConstants)
                    );
                    device->draw_indexed(api.cb, mesh->get_index_count(), 0, 0);
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
            .with_primitive_type(rhi::PrimitiveTopType::TriangleList)
            .with_push_constants(2 * sizeof(u32));
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

        pass.render([this, 
            color = std::move(color_ref),
            depth = std::move(depth_ref),
            pipeline_raster = std::move(pipeline)](rg::RenderPassApi& api) {
                auto [width, height, _] = depth.desc.extent;

                auto instance_transforms_offset = api.dynamic_constants()
                    ->push_from_vec(renderer->instantce_transforms);

                api.begin_render_pass(
                    *wire_render_pass,
                    { width, height },
                    {
                        std::pair{color,rhi::GpuTextureViewDesc()},
                    },
                    std::pair{ depth, rhi::GpuTextureViewDesc().with_aspect_mask(rhi::ImageAspectFlags::DEPTH) }
                );

                api.set_default_view_and_scissor({ width,height });

                std::vector<rg::RenderPassBinding> bindings = { rg::RenderPassBinding::DynamicConstantsStorageBuffer(instance_transforms_offset) };
                auto res = rg::RenderPassPipelineBinding<rg::RgRasterPipelineHandle>::from(pipeline_raster)
                    .descriptor_set(0, &bindings)
                    .raw_descriptor_set(1, renderer->binldess_descriptorset());
                auto bound_raster = api.bind_raster_pipeline(res);

                auto device = api.device();
                const auto& mesh_cmds = renderer->mesh_command_queue;
                for (u32 draw_idx = 0; draw_idx < mesh_cmds.size(); draw_idx++)
                {
                    const auto& cmd = mesh_cmds[draw_idx];
                    auto mesh = renderer->mesh_buf_id_2_mesh[cmd.mesh_id];
                    device->bind_index_buffer(api.cb, mesh->get_index_buffer().get(), rhi::IndexBufferFormat::UINT32, 0);
                    struct PushConstants {
                        u32 draw_index;
                        u32 mesh_index;
                    } push_constants;


                    push_constants.draw_index = cmd.mesh_instance_id;
                    push_constants.mesh_index = cmd.mesh_id;
                    bound_raster.push_constants(
                        api.cb,
                        0,
                        (u8*)&push_constants,
                        sizeof(PushConstants)
                    );
                    device->draw_indexed(api.cb, mesh->get_index_count(), 0, 0);
                }

                api.end_render_pass();
            });
        pass.rg->record_pass(std::move(pass.pass));
    }
}