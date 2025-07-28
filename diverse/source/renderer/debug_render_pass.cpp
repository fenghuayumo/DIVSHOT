#include "defered_renderer.h"
#include "debug_render_pass.h"
#include "core/profiler.h"
#include "debug_renderer.h"

static const uint32_t MaxPoints = 20000;
static const uint32_t MaxPointVertices = MaxPoints * 4;
static const uint32_t MaxPointIndices = MaxPoints * 6;
static const uint32_t MAX_BATCH_DRAW_CALLS = 1000;
static const uint32_t RENDERER_POINT_SIZE = sizeof(diverse::PointVertexData) * 4;
static const uint32_t RENDERER_POINT_BUFFER_SIZE = RENDERER_POINT_SIZE * MaxPointVertices;

static const uint32_t MaxLines = 20000;
static const uint32_t MaxLineVertices = MaxLines * 2;
static const uint32_t MaxLineIndices = MaxLines * 6;
static const uint32_t MAX_LINE_BATCH_DRAW_CALLS = 1000;
static const uint32_t RENDERER_LINE_SIZE = sizeof(diverse::LineVertexData) * 4;
static const uint32_t RENDERER_LINE_BUFFER_SIZE = RENDERER_LINE_SIZE * MaxLineVertices;

namespace diverse
{
    DebugRenderPass::DebugRenderPass(DeferedRenderer* render)
    {
        renderer = render;
        rhi::RenderPassDesc desc = {
            {
                rhi::RenderPassAttachmentDesc::create(PixelFormat::R8G8B8A8_UNorm).load_input(),
            },
            rhi::RenderPassAttachmentDesc::create(PixelFormat::D32_Float).load_input()
        };
        debug_line_pass = g_device->create_render_pass(desc);
        
        //points
		uint32_t* indices = new uint32_t[MaxPointIndices];
		int32_t offset = 0;
		for (int32_t i = 0; i < MaxPointIndices; i += 6)
		{
			indices[i] = offset + 0;
			indices[i + 1] = offset + 1;
			indices[i + 2] = offset + 2;

			indices[i + 3] = offset + 2;
			indices[i + 4] = offset + 3;
			indices[i + 5] = offset + 0;
			offset += 4;
		}
		debug_draw_data.point_index_buffer = g_device->create_buffer(rhi::GpuBufferDesc::new_cpu_to_gpu(MaxPointIndices * sizeof(u32), rhi::BufferUsageFlags::INDEX_BUFFER), "point_index_buf", (u8*)indices);
		delete[] indices;

        //lines
		indices = new uint32_t[MaxLineIndices];
		for (int32_t i = 0; i < MaxLineIndices; i++)
		{
			indices[i] = i;
		}
		debug_draw_data.line_index_buffer = g_device->create_buffer(rhi::GpuBufferDesc::new_cpu_to_gpu(MaxLineIndices * sizeof(u32), rhi::BufferUsageFlags::INDEX_BUFFER), "line_index_buf", (u8*)indices);
		delete[] indices;

        //debug quads
        debug_draw_data.renderer2d_data.index_count = 0;
        debug_draw_data.renderer2d_data.vertex_data = nullptr;
        debug_draw_data.renderer2d_data.limits.SetMaxQuads(10000);

        indices = new uint32_t[debug_draw_data.renderer2d_data.limits.IndiciesSize];

        {
            for (uint32_t i = 0; i < debug_draw_data.renderer2d_data.limits.IndiciesSize; i++)
            {
                indices[i] = i;
            }
        }

        debug_draw_data.renderer2d_data.index_buffer = g_device->create_buffer(rhi::GpuBufferDesc::new_cpu_to_gpu(debug_draw_data.renderer2d_data.limits.IndiciesSize * sizeof(u32), rhi::BufferUsageFlags::INDEX_BUFFER), "point_index_buf", (u8*)indices);
        delete[] indices;
        debug_draw_data.renderer2d_data.vertex_buffers.resize(1);

        line_buffer.push_back(new LineVertexData[debug_draw_data.renderer2d_data.limits.MaxQuads * 4]);
        point_buffers.push_back(new PointVertexData[debug_draw_data.renderer2d_data.limits.MaxQuads * 4]);
        quad_buffers.push_back(new VertexData[debug_draw_data.renderer2d_data.limits.MaxQuads * 4]);
    }

    DebugRenderPass::~DebugRenderPass()
    {
        for (auto buf : line_buffer)
		{
			delete[] buf;
		}
    }

    auto DebugRenderPass::render(rg::TemporalGraph& rg,rg::Handle<rhi::GpuTexture>& color_img,rg::Handle<rhi::GpuTexture>& depth_img)->void
    {
        DS_PROFILE_FUNCTION();
        if (!renderer->get_camera() || !renderer->get_camera_transform())
            return;
        auto main_render_tex = renderer->get_main_render_image();
        //auto depth_img = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(PixelFormat::D32_Float, main_render_tex->desc.extent_2d()), "depth");
        //auto color_img = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(PixelFormat::R8G8B8A8_UNorm, main_render_tex->desc.extent_2d()), "debug_color");
        for(int i =0;i<2;i++)
        {
            bool depthTest = i == 1;
            auto& lines = DebugRenderer::GetInstance()->GetLines(depthTest);
            auto& thickLines = DebugRenderer::GetInstance()->GetThickLines(depthTest);
            auto& triangles = DebugRenderer::GetInstance()->GetTriangles(depthTest);
            auto& points = DebugRenderer::GetInstance()->GetPoints(depthTest);
            auto projView = renderer->get_camera()->get_projection_matrix() * glm::inverse(renderer->get_camera_transform()->get_world_matrix());

            if (!lines.empty())
            {
                auto pass = rg.add_pass("raster line");

                auto pipeline_desc = rhi::RasterPipelineDesc()
                    .with_render_pass(debug_line_pass)
                    .with_cull_mode(rhi::CullMode::BACK)
                    .with_primitive_type(rhi::PrimitiveTopType::LineList)
                    .with_depth_test(depthTest);
                auto pipeline = pass.register_raster_pipeline(
                    {
                        rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Vertex).with_shader_source({"/shaders/debug_draw/batch2d_line_vs.hlsl"}),
                        rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Pixel).with_shader_source({"/shaders/debug_draw/batch2d_line_ps.hlsl"})
                    },
                    std::move(pipeline_desc)
                );
                if ((int)debug_draw_data.line_vertex_buffers.size() - 1 < (int)debug_draw_data.line_batch_drawcall_index)
                {
                    debug_draw_data.line_vertex_buffers.emplace_back(rg.device->create_buffer(rhi::GpuBufferDesc::new_cpu_to_gpu(
                                        RENDERER_LINE_BUFFER_SIZE, rhi::BufferUsageFlags::VERTEX_BUFFER), "line_vertex_buf", nullptr));
                }
            
                auto depth_ref = pass.raster(depth_img, rhi::AccessType::DepthAttachmentWriteStencilReadOnly);
                auto color_ref = pass.raster(color_img, rhi::AccessType::ColorAttachmentWrite);
                debug_draw_data.line_buffer = line_buffer[0];
                for (auto& line : lines)
                {
            		if (debug_draw_data.line_index_count >= debug_draw_data.renderer2d_data.limits.MaxQuads)
                        break;

                    debug_draw_data.line_buffer->vertex = line.p1;
                    debug_draw_data.line_buffer->colour = line.col;
                    debug_draw_data.line_buffer++;

                    debug_draw_data.line_buffer->vertex = line.p2;
                    debug_draw_data.line_buffer->colour = line.col;
                    debug_draw_data.line_buffer++;

                    debug_draw_data.line_index_count += 2;
                }

                u32 dataSize = (u8*)debug_draw_data.line_buffer - (u8*)line_buffer[0];
                debug_draw_data.line_vertex_buffers[debug_draw_data.line_batch_drawcall_index]->copy_from(rg.device, (u8*)line_buffer[0], dataSize, 0);

                auto line_batch_drawcall_index = debug_draw_data.line_batch_drawcall_index;
                auto line_index_count = debug_draw_data.line_index_count;

                pass.render([this, color_img = std::move(color_ref),
                    depth = std::move(depth_ref), 
                    line_batch_drawcall_index,
                    line_index_count,
                    pipeline_raster = std::move(pipeline)](rg::RenderPassApi& api) {
                        auto [width, height, _] = renderer->get_main_render_image()->desc.extent;

                        std::vector<std::pair<glm::mat3x4, glm::mat3x4>>   trs;

                        auto instance_transforms_offset = api.dynamic_constants()
                            ->push_from_vec(trs);

                        api.begin_render_pass(
                            *debug_line_pass,
                            { width, height },
                            {
                                std::pair{color_img,rhi::GpuTextureViewDesc()},
                            },
                            std::pair{ depth, rhi::GpuTextureViewDesc().with_aspect_mask(rhi::ImageAspectFlags::DEPTH) }
                        );

                        api.set_default_view_and_scissor({ width,height });

                        std::vector<rg::RenderPassBinding> bindings = { rg::RenderPassBinding::DynamicConstantsStorageBuffer(instance_transforms_offset) };
                        auto res = rg::RenderPassPipelineBinding<rg::RgRasterPipelineHandle>::from(pipeline_raster)
                            .descriptor_set(0, &bindings);
    /*						.raw_descriptor_set(1, bindless_descriptor_set);*/
                        auto bound_raster = api.bind_raster_pipeline(res);

                        auto device = api.device();
                        
                        device->bind_index_buffer(api.cb, debug_draw_data.line_index_buffer.get(), rhi::IndexBufferFormat::UINT32, 0);
                        rhi::GpuBuffer* vertex_buffers[] = {
                            debug_draw_data.line_vertex_buffers[line_batch_drawcall_index].get()
                        };
                        u32 strides[] = {
                            sizeof(LineVertexData)
                        };
                        u64 offsets[] = {
                            0
                        };
                        device->bind_vertex_buffers(api.cb, vertex_buffers, 0, 1, strides, offsets);
                        device->draw_indexed(api.cb, line_index_count, 0,0);

                        api.end_render_pass();
                    });

                debug_draw_data.line_buffer = line_buffer[0];
                debug_draw_data.line_index_count = 0;
                debug_draw_data.line_batch_drawcall_index++;
                pass.rg->record_pass(std::move(pass.pass));
            }

            if (!thickLines.empty())
            {
                auto pass = rg.add_pass("raster thick line");

                auto pipeline_desc = rhi::RasterPipelineDesc()
                    .with_render_pass(debug_line_pass)
                    .with_cull_mode(rhi::CullMode::BACK)
                    .with_primitive_type(rhi::PrimitiveTopType::LineList)
                    .with_depth_test(depthTest);
                auto pipeline = pass.register_raster_pipeline(
                    {
                        rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Vertex).with_shader_source({"/shaders/debug_draw/batch2d_line_vs.hlsl"}),
                        rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Pixel).with_shader_source({"/shaders/debug_draw/batch2d_line_ps.hlsl"})
                    },
                    std::move(pipeline_desc)
                );
                if ((int)debug_draw_data.line_vertex_buffers.size() - 1 < (int)debug_draw_data.line_batch_drawcall_index)
                {
                    debug_draw_data.line_vertex_buffers.emplace_back(rg.device->create_buffer(rhi::GpuBufferDesc::new_cpu_to_gpu(
                        RENDERER_LINE_BUFFER_SIZE, rhi::BufferUsageFlags::VERTEX_BUFFER), "line_vertex_buf", nullptr));
                }

                auto depth_ref = pass.raster(depth_img, rhi::AccessType::DepthAttachmentWriteStencilReadOnly);
                auto color_ref = pass.raster(color_img, rhi::AccessType::ColorAttachmentWrite);
                debug_draw_data.line_buffer = line_buffer[0];
                for (auto& line : thickLines)
                {
                    if (debug_draw_data.line_index_count >= debug_draw_data.renderer2d_data.limits.MaxQuads)
                        break;

                    debug_draw_data.line_buffer->vertex = line.p1;
                    debug_draw_data.line_buffer->colour = line.col;
                    debug_draw_data.line_buffer++;

                    debug_draw_data.line_buffer->vertex = line.p2;
                    debug_draw_data.line_buffer->colour = line.col;
                    debug_draw_data.line_buffer++;

                    debug_draw_data.line_index_count += 2;
                }

                u32 dataSize = (u8*)debug_draw_data.line_buffer - (u8*)line_buffer[0];
                debug_draw_data.line_vertex_buffers[debug_draw_data.line_batch_drawcall_index]->copy_from(rg.device, (u8*)line_buffer[0], dataSize, 0);
                auto line_batch_drawcall_index = debug_draw_data.line_batch_drawcall_index;
                auto line_index_count = debug_draw_data.line_index_count;
                pass.render([this, color_img = std::move(color_ref),
                    depth = std::move(depth_ref), 
                    line_batch_drawcall_index,
                    line_index_count,
                    pipeline_raster = std::move(pipeline)](rg::RenderPassApi& api) {
                        auto [width, height, _] = renderer->get_main_render_image()->desc.extent;

                        std::vector<std::pair<glm::mat3x4, glm::mat3x4>>   trs;

                        auto instance_transforms_offset = api.dynamic_constants()
                            ->push_from_vec(trs);

                        api.begin_render_pass(
                            *debug_line_pass,
                            { width, height },
                            {
                                std::pair{color_img,rhi::GpuTextureViewDesc()},
                            },
                            std::pair{ depth, rhi::GpuTextureViewDesc().with_aspect_mask(rhi::ImageAspectFlags::DEPTH) }
                        );

                        api.set_default_view_and_scissor({ width,height });

                        std::vector<rg::RenderPassBinding> bindings = { rg::RenderPassBinding::DynamicConstantsStorageBuffer(instance_transforms_offset) };
                        auto res = rg::RenderPassPipelineBinding<rg::RgRasterPipelineHandle>::from(pipeline_raster)
                            .descriptor_set(0, &bindings);
                        /*						.raw_descriptor_set(1, bindless_descriptor_set);*/
                        auto bound_raster = api.bind_raster_pipeline(res);

                        auto device = api.device();

                        device->bind_index_buffer(api.cb, debug_draw_data.line_index_buffer.get(), rhi::IndexBufferFormat::UINT32, 0);
                        rhi::GpuBuffer* vertex_buffers[] = {
                            debug_draw_data.line_vertex_buffers[line_batch_drawcall_index].get()
                        };
                        u32 strides[] = {
                            sizeof(LineVertexData)
                        };
                        u64 offsets[] = {
                            0
                        };
                        device->bind_vertex_buffers(api.cb, vertex_buffers, 0, 1, strides, offsets);
                        device->draw_indexed(api.cb, line_index_count, 0, 0);

                        api.end_render_pass();
                    });

                debug_draw_data.line_buffer = line_buffer[0];
                debug_draw_data.line_index_count = 0;
                debug_draw_data.line_batch_drawcall_index++;
                pass.rg->record_pass(std::move(pass.pass));
            }
            if (!points.empty())
            {
                auto pass = rg.add_pass("raster points");

                auto pipeline_desc = rhi::RasterPipelineDesc()
                    .with_render_pass(debug_line_pass)
                    .with_cull_mode(rhi::CullMode::BACK)
                    .with_primitive_type(rhi::PrimitiveTopType::TriangleList)
                    .with_depth_test(depthTest);
                auto pipeline = pass.register_raster_pipeline(
                    {
                        rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Vertex).with_shader_source({"/shaders/debug_draw/batch2d_point_vs.hlsl"}),
                        rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Pixel).with_shader_source({"/shaders/debug_draw/batch2d_point_ps.hlsl"})
                    },
                    std::move(pipeline_desc)
                );
                if ((int)debug_draw_data.point_vertex_buffers.size() - 1 < (int)debug_draw_data.point_batch_drawcall_index)
                {
                    auto& vertexBuffer = debug_draw_data.point_vertex_buffers.emplace_back(rg.device->create_buffer(rhi::GpuBufferDesc::new_cpu_to_gpu(
                        RENDERER_POINT_BUFFER_SIZE, rhi::BufferUsageFlags::VERTEX_BUFFER), "point_vertex_buf", nullptr));
                }

                debug_draw_data.point_buffer = point_buffers[0]; 
                for (auto& pointInfo : points)
                {
                    auto cameraTransform = renderer->camera_transform;
                    glm::vec3 right = pointInfo.size * cameraTransform->get_right_direction();
                    glm::vec3 up = pointInfo.size * cameraTransform->get_up_direction();

                    debug_draw_data.point_buffer->vertex = pointInfo.p1 - right - up; // + glm::vec3(-pointInfo.size, -pointInfo.size, 0.0f));
                    debug_draw_data.point_buffer->colour = pointInfo.col;
                    debug_draw_data.point_buffer->size = { pointInfo.size, 0.0f };
                    debug_draw_data.point_buffer->uv = { -1.0f, -1.0f };
                    debug_draw_data.point_buffer++;

                    debug_draw_data.point_buffer->vertex = pointInfo.p1 + right - up; //(pointInfo.p1 + glm::vec3(pointInfo.size, -pointInfo.size, 0.0f));
                    debug_draw_data.point_buffer->colour = pointInfo.col;
                    debug_draw_data.point_buffer->size = { pointInfo.size, 0.0f };
                    debug_draw_data.point_buffer->uv = { 1.0f, -1.0f };
                    debug_draw_data.point_buffer++;

                    debug_draw_data.point_buffer->vertex = pointInfo.p1 + right + up; //(pointInfo.p1 + glm::vec3(pointInfo.size, pointInfo.size, 0.0f));
                    debug_draw_data.point_buffer->colour = pointInfo.col;
                    debug_draw_data.point_buffer->size = { pointInfo.size, 0.0f };
                    debug_draw_data.point_buffer->uv = { 1.0f, 1.0f };
                    debug_draw_data.point_buffer++;

                    debug_draw_data.point_buffer->vertex = pointInfo.p1 - right + up; // (pointInfo.p1 + glm::vec3(-pointInfo.size, pointInfo.size, 0.0f));
                    debug_draw_data.point_buffer->colour = pointInfo.col;
                    debug_draw_data.point_buffer->size = { pointInfo.size, 0.0f };
                    debug_draw_data.point_buffer->uv = { -1.0f, 1.0f };
                    debug_draw_data.point_buffer++;

                    debug_draw_data.point_index_count += 6;
                }

                uint32_t dataSize = (uint32_t)((uint8_t*)debug_draw_data.point_buffer - (uint8_t*)point_buffers[0]);
                debug_draw_data.point_vertex_buffers[debug_draw_data.point_batch_drawcall_index]->copy_from(rg.device, (u8*)point_buffers[0], dataSize, 0);
                auto point_batch_drawcall_index = debug_draw_data.point_batch_drawcall_index;
                auto point_index_count = debug_draw_data.point_index_count;
                auto depth_ref = pass.raster(depth_img, rhi::AccessType::DepthAttachmentWriteStencilReadOnly);
                auto color_ref = pass.raster(color_img, rhi::AccessType::ColorAttachmentWrite);

                pass.render([this, color_img = std::move(color_ref),
                    depth = std::move(depth_ref),
                    point_batch_drawcall_index,
                    point_index_count,
                    pipeline_raster = std::move(pipeline)](rg::RenderPassApi& api) {
                        auto [width, height, _] = renderer->get_main_render_image()->desc.extent;

                        std::vector<std::pair<glm::mat3x4, glm::mat3x4>>   trs;

                        auto instance_transforms_offset = api.dynamic_constants()
                            ->push_from_vec(trs);

                        api.begin_render_pass(
                            *debug_line_pass,
                            { width, height },
                            {
                                std::pair{color_img,rhi::GpuTextureViewDesc()},
                            },
                            std::pair{ depth, rhi::GpuTextureViewDesc().with_aspect_mask(rhi::ImageAspectFlags::DEPTH) }
                        );

                        api.set_default_view_and_scissor({ width,height });

                        std::vector<rg::RenderPassBinding> bindings = { rg::RenderPassBinding::DynamicConstantsStorageBuffer(instance_transforms_offset) };
                        auto res = rg::RenderPassPipelineBinding<rg::RgRasterPipelineHandle>::from(pipeline_raster)
                            .descriptor_set(0, &bindings);
                        /*						.raw_descriptor_set(1, bindless_descriptor_set);*/
                        auto bound_raster = api.bind_raster_pipeline(res);

                        auto device = api.device();

                        device->bind_index_buffer(api.cb, debug_draw_data.point_index_buffer.get(), rhi::IndexBufferFormat::UINT32, 0);
                        rhi::GpuBuffer* vertex_buffers[] = {
                            debug_draw_data.point_vertex_buffers[point_batch_drawcall_index].get()
                        };
                        u32 strides[] = {
                            sizeof(LineVertexData)
                        };
                        u64 offsets[] = {
                            0
                        };
                        device->bind_vertex_buffers(api.cb, vertex_buffers, 0, 1, strides, offsets);
                        device->draw_indexed(api.cb, point_index_count, 0, 0);

                        api.end_render_pass();
                });

                debug_draw_data.point_buffer = point_buffers[0];
                debug_draw_data.point_index_count = 0;
                debug_draw_data.point_batch_drawcall_index++;
                pass.rg->record_pass(std::move(pass.pass));
            }

            if (!triangles.empty())
            {
                u32 current_frame = 0;
                auto pass = rg.add_pass("raster triangles");

                auto pipeline_desc = rhi::RasterPipelineDesc()
                    .with_render_pass(debug_line_pass)
                    .with_cull_mode(rhi::CullMode::BACK)
                    .with_primitive_type(rhi::PrimitiveTopType::TriangleList)
                    .with_depth_test(depthTest);
                auto pipeline = pass.register_raster_pipeline(
                    {
                        rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Vertex).with_shader_source({"/shaders/debug_draw/batch2d_vs.hlsl"}),
                        rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Pixel).with_shader_source({"/shaders/debug_draw/batch2d_ps.hlsl"})
                    },
                    std::move(pipeline_desc)
                );
                if ((int)debug_draw_data.renderer2d_data.vertex_buffers.size() - 1 < (int)debug_draw_data.renderer2d_data.batch_drawcall_index)
                {
                    debug_draw_data.renderer2d_data.vertex_buffers[current_frame].emplace_back(rg.device->create_buffer(rhi::GpuBufferDesc::new_cpu_to_gpu(
                        RENDERER_LINE_BUFFER_SIZE, rhi::BufferUsageFlags::VERTEX_BUFFER), "render2d_vertex_buf", nullptr));
                }
                debug_draw_data.renderer2d_data.vertex_data = quad_buffers[current_frame];
                for (auto& triangleInfo : triangles)
                {
                    float textureSlot = 0.0f;

                    debug_draw_data.renderer2d_data.vertex_data->vertex = triangleInfo.p1;
                    debug_draw_data.renderer2d_data.vertex_data->uv = { 0.0f, 0.0f, 0.0f, 0.0f };
                    debug_draw_data.renderer2d_data.vertex_data->tid = glm::vec2(textureSlot, 0.0f);
                    debug_draw_data.renderer2d_data.vertex_data->colour = triangleInfo.col;
                    debug_draw_data.renderer2d_data.vertex_data++;
                    debug_draw_data.renderer2d_data.vertex_data->vertex = triangleInfo.p2;
                    debug_draw_data.renderer2d_data.vertex_data->uv = { 0.0f, 0.0f, 0.0f, 0.0f };
                    debug_draw_data.renderer2d_data.vertex_data->tid = glm::vec2(textureSlot, 0.0f);
                    debug_draw_data.renderer2d_data.vertex_data->colour = triangleInfo.col;
                    debug_draw_data.renderer2d_data.vertex_data++;
                    debug_draw_data.renderer2d_data.vertex_data->vertex = triangleInfo.p3;
                    debug_draw_data.renderer2d_data.vertex_data->uv = { 0.0f, 0.0f, 0.0f, 0.0f };
                    debug_draw_data.renderer2d_data.vertex_data->tid = glm::vec2(textureSlot, 0.0f);
                    debug_draw_data.renderer2d_data.vertex_data->colour = triangleInfo.col;
                    debug_draw_data.renderer2d_data.vertex_data++;
                    debug_draw_data.renderer2d_data.index_count += 3;
                }
                uint32_t dataSize = (uint32_t)((uint8_t*)debug_draw_data.renderer2d_data.vertex_data - (uint8_t*)quad_buffers[0]);
                debug_draw_data.renderer2d_data.vertex_buffers[current_frame][debug_draw_data.renderer2d_data.batch_drawcall_index]->copy_from(get_global_device(), (u8*)quad_buffers[0], dataSize,0);
                auto depth_ref = pass.raster(depth_img, rhi::AccessType::DepthAttachmentWriteStencilReadOnly);
                auto color_ref = pass.raster(color_img, rhi::AccessType::ColorAttachmentWrite);
                auto quad_batch_drawcall_index = debug_draw_data.renderer2d_data.batch_drawcall_index;
                auto quad_index_count = debug_draw_data.renderer2d_data.index_count;
                
                pass.render([this, color_img = std::move(color_ref),
                    depth = std::move(depth_ref),
                    quad_batch_drawcall_index,
                    quad_index_count,
                    current_frame,
                    pipeline_raster = std::move(pipeline)](rg::RenderPassApi& api) {
                        auto [width, height, _] = renderer->get_main_render_image()->desc.extent;

                        std::vector<std::pair<glm::mat3x4, glm::mat3x4>>   trs;

                        auto instance_transforms_offset = api.dynamic_constants()
                            ->push_from_vec(trs);

                        api.begin_render_pass(
                            *debug_line_pass,
                            { width, height },
                            {
                                std::pair{color_img,rhi::GpuTextureViewDesc()},
                            },
                            std::pair{ depth, rhi::GpuTextureViewDesc().with_aspect_mask(rhi::ImageAspectFlags::DEPTH) }
                        );

                        api.set_default_view_and_scissor({ width,height });

                        std::vector<rg::RenderPassBinding> bindings = { rg::RenderPassBinding::DynamicConstantsStorageBuffer(instance_transforms_offset) };
                        auto res = rg::RenderPassPipelineBinding<rg::RgRasterPipelineHandle>::from(pipeline_raster)
                            .descriptor_set(0, &bindings);
                        /*						.raw_descriptor_set(1, bindless_descriptor_set);*/
                        auto bound_raster = api.bind_raster_pipeline(res);

                        auto device = api.device();

                        device->bind_index_buffer(api.cb, debug_draw_data.renderer2d_data.index_buffer.get(), rhi::IndexBufferFormat::UINT32, 0);
                        rhi::GpuBuffer* vertex_buffers[] = {
                            debug_draw_data.renderer2d_data.vertex_buffers[current_frame][quad_batch_drawcall_index].get()
                        };
                        u32 strides[] = {
                            sizeof(LineVertexData)
                        };
                        u64 offsets[] = {
                            0
                        };
                        device->bind_vertex_buffers(api.cb, vertex_buffers, 0, 1, strides, offsets);
                        device->draw_indexed(api.cb, quad_index_count, 0, 0);

                        api.end_render_pass();
                    });

                debug_draw_data.renderer2d_data.vertex_data = quad_buffers[current_frame];
                debug_draw_data.renderer2d_data.index_count = 0;
                pass.rg->record_pass(std::move(pass.pass));
            }
        }

        debug_draw_data.point_batch_drawcall_index = 0;
        debug_draw_data.line_batch_drawcall_index = 0;
    }

}
