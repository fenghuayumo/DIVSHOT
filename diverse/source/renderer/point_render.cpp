#include "point_render.h"
#include "defered_renderer.h"
#include "drs_rg/image_op.h"
#include "scene/components/global_transform_component.h"
#include "scene/component/point_cloud_component.h"
#include "utility/pack_utils.h"
#include "maths/maths_utils.h"
#include "core/ds_log.h"
#include "utility/cmd_variable.h"

namespace diverse
{
    PointRenderPass::PointRenderPass(DeferedRenderer* defrenderer)
    	: renderer(defrenderer)
	{
		auto device = renderer->get_device();
		rhi::RenderPassDesc desc = {
		{
			rhi::RenderPassAttachmentDesc::create(PixelFormat::R8G8B8A8_UNorm).load_input(),
		},
			rhi::RenderPassAttachmentDesc::create(PixelFormat::D32_Float).load_input()
		};
		point_render_pass = device->create_render_pass(desc);
    }
    
    PointRenderPass::PointRenderPass()
    {
    }

    auto PointRenderPass::render(rg::TemporalGraph& rg,
			rg::Handle<rhi::GpuTexture>& color_img,
			rg::Handle<rhi::GpuTexture>& depth_img) -> rg::Handle<rhi::GpuTexture>
    {
        auto& point_cmd = renderer->point_command_queue;
        if(point_cmd.size() <= 0) return color_img;

		const auto& render_settings = renderer->get_frame_render_settings();
		if(render_settings.gs_point_size <= 0 ) return color_img;
        auto pass = rg.add_pass("raster gs points");
		auto pipeline_desc = rhi::RasterPipelineDesc()
			.with_render_pass(point_render_pass)
			.with_cull_mode(rhi::CullMode::NONE)
			.with_vetex_attribute(false)
			.with_primitive_type(rhi::PrimitiveTopType::TriangleStrip);
		std::vector<std::pair<std::string, std::string>> defines;
		auto pipeline = pass.register_raster_pipeline(
			{
			   rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Vertex).with_shader_source({"/shaders/point_cloud_vs.hlsl","main",defines}),
			//    rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Geometry).with_shader_source({"/shaders/gaussian/gs_point_gs.hlsl"}),
			   rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Pixel).with_shader_source({"/shaders/point_cloud_ps.hlsl","main",defines})
			},
			std::move(pipeline_desc)
		);
		auto depth_ref = pass.raster(depth_img, rhi::AccessType::DepthAttachmentWriteStencilReadOnly);
		auto color_ref = pass.raster(color_img, rhi::AccessType::ColorAttachmentWrite);

        for (const auto& cmd : point_cmd)
		{
            const auto num_points = cmd.model->get_num_points();
	        struct GaussianConstants {
				glm::mat4 transform;
				float point_size;
				u32 surface_width;
                u32 surface_height;
                uint buf_id;
            }gs_constants;
            gs_constants.transform = glm::transpose(cmd.transform.world_matrix);
			gs_constants.point_size = glm::clamp(render_settings.gs_point_size,0.0f,100.0f) / 2.0f;
            gs_constants.surface_width = color_img.desc.extent[0];
            gs_constants.surface_height = color_img.desc.extent[1];
			gs_constants.buf_id = renderer->get_buf_id(cmd.model.get());
            pass.render([this, color_img = std::move(color_ref),
				depth = std::move(depth_ref),
				num_points, gs_constants,
				pipeline_raster = std::move(pipeline)](rg::RenderPassApi& api) mutable {
					auto [width, height, _] = color_img.desc.extent;

					auto dynamic_offset = api.dynamic_constants()
						->push(gs_constants);

					api.begin_render_pass(
						*point_render_pass,
						{ width, height },
						{
							std::pair{color_img,rhi::GpuTextureViewDesc()},
						},
						std::pair{ depth, rhi::GpuTextureViewDesc().with_aspect_mask(rhi::ImageAspectFlags::DEPTH) }
					);

					api.set_default_view_and_scissor({ width,height });

					std::vector<rg::RenderPassBinding> bindings = { 
						rg::RenderPassBinding::DynamicConstants(dynamic_offset),
					};
					auto res = rg::RenderPassPipelineBinding<rg::RgRasterPipelineHandle>::from(pipeline_raster)
						.descriptor_set(0, &bindings)
						.raw_descriptor_set(1, renderer->binldess_descriptorset());

					auto bound_raster = api.bind_raster_pipeline(res);

					auto device = api.device();
					device->draw_instanced(api.cb, 4, num_points,0,0);
					api.end_render_pass();
				});
        }
        pass.rg->record_pass(std::move(pass.pass));
		return color_img;
    }
}
