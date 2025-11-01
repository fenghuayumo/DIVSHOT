#include "gaussian.h"
#include "defered_renderer.h"
#include "drs_rg/image_op.h"
#include "drs_rg/buffer_op.h"
#include "scene/component/gaussian_component.h"
#include "scene/component/gaussian_crop.h"
#include "utility/pack_utils.h"
#include "maths/maths_utils.h"
#include "core/ds_log.h"
#include "utility/cmd_variable.h"
namespace diverse
{
	CmdVariable enableOutlineVar("r.enableOutline", false, "enable outline pass");
}

namespace diverse
{
	extern auto inclusive_prefix_scan_u32_1m(rg::RenderGraph& rg,
		const rg::Handle<rhi::GpuBuffer>& input_buf,
		rg::Handle<rhi::GpuBuffer>& output_buf) -> void;

	extern  auto radix_sort(rg::RenderGraph& rg,
		rg::Handle<rhi::GpuBuffer>& keys_src,
		rg::Handle<rhi::GpuBuffer>& keys_dst,
		rg::Handle<rhi::GpuBuffer>& values_src,
		rg::Handle<rhi::GpuBuffer>& values_dst) -> void;
	
	extern  auto radix_sort_indirect(rg::RenderGraph& rg,
		rg::Handle<rhi::GpuBuffer>& keys_src,
		rg::Handle<rhi::GpuBuffer>& keys_dst,
		rg::Handle<rhi::GpuBuffer>& values_src,
		rg::Handle<rhi::GpuBuffer>& values_dst,
		rg::Handle<rhi::GpuBuffer>& count,
		u32 num_pass) -> void;
	extern 	auto gpu_sort(rg::RenderGraph& rg,
		rg::Handle<rhi::GpuBuffer>& keys_src,
		rg::Handle<rhi::GpuBuffer>& values_src,
		u32 count) -> std::pair<rg::Handle<rhi::GpuBuffer>, rg::Handle<rhi::GpuBuffer>>;
	extern auto gpu_sort_indirect(rg::RenderGraph& rg,
		rg::Handle<rhi::GpuBuffer>& keys_src,
		rg::Handle<rhi::GpuBuffer>& values_src,
		const rg::Handle<rhi::GpuBuffer>& count_buffer) -> std::pair<rg::Handle<rhi::GpuBuffer>, rg::Handle<rhi::GpuBuffer>>;

	auto drs_align(u32 address,u32 alignment)->u32
	{
		return (address + alignment - 1) & ~(alignment-1);
	}

	GaussianRenderPass::GaussianRenderPass(DeferedRenderer* render)
		: renderer(render)
	{
		rhi::RenderPassDesc desc = {
		{
			rhi::RenderPassAttachmentDesc::create(PixelFormat::R8G8B8A8_UNorm).load_input(),
		},
			rhi::RenderPassAttachmentDesc::create(PixelFormat::D32_Float).load_input()
		};
		gs_point_render_pass = g_device->create_render_pass(desc);

	 	desc = {
		{
            rhi::RenderPassAttachmentDesc::create(PixelFormat::R8G8B8A8_UNorm).load_input(),
		},
			rhi::RenderPassAttachmentDesc::create(PixelFormat::D32_Float).load_input()
		};
		gsplat_render_pass = g_device->create_render_pass(desc);
	}

	GaussianRenderPass::GaussianRenderPass()
	{
	}

	auto GaussianRenderPass::render(rg::TemporalGraph& rg,
		rg::Handle<rhi::GpuTexture>& color_img,
		rg::Handle<rhi::GpuTexture>& depth_img) -> GSplatRenderOutput
	{
		return render_splats(rg, color_img, depth_img);
	}

    auto GaussianRenderPass::render_points(rg::TemporalGraph& rg,
			rg::Handle<rhi::GpuTexture>& color_img,
			rg::Handle<rhi::GpuTexture>& depth_img,
			const SplatRenderData& splat_data) -> rg::Handle<rhi::GpuTexture>
    {
		if(g_render_settings.gs_point_size <= 0 ) return color_img;
        auto pass = rg.add_pass("raster gs points");
		auto pipeline_desc = rhi::RasterPipelineDesc()
			.with_render_pass(gs_point_render_pass)
			.with_cull_mode(rhi::CullMode::NONE)
			.with_vetex_attribute(false)
			// .with_blend_mode(rhi::BlendMode::OneOne)
			.with_primitive_type(rhi::PrimitiveTopType::TriangleStrip);
		std::vector<std::pair<std::string, std::string>> defines;
#if SPLAT_EDIT
		defines.push_back({ "SPLAT_EDIT", "1" });
#endif
		auto pipeline = pass.register_raster_pipeline(
			{
			   rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Vertex).with_shader_source({"/shaders/gaussian/gs_point_vs.hlsl","main",defines}),
			//    rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Geometry).with_shader_source({"/shaders/gaussian/gs_point_gs.hlsl"}),
			   rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Pixel).with_shader_source({"/shaders/gaussian/gs_point_ps.hlsl","main",defines})
			},
			std::move(pipeline_desc)
		);
		auto depth_ref = pass.raster(depth_img, rhi::AccessType::DepthAttachmentWriteStencilReadOnly);
		auto color_ref = pass.raster(color_img, rhi::AccessType::ColorAttachmentWrite);

		{
			const auto num_gaussians = splat_data.model->get_num_gaussians();
			
			struct GaussianConstants {
				glm::mat4 transform;
				float point_size;
				u32 surface_width;
                u32 surface_height;
                uint buf_id;
				glm::vec4 locked_color;
    			glm::vec4 select_color;
			}gs_constants;
            gs_constants.transform = glm::transpose(splat_data.transform.get_world_matrix());
			gs_constants.point_size = glm::clamp(g_render_settings.gs_point_size,0.0f,100.0f) / 2.0f;
            gs_constants.surface_width = color_img.desc.extent[0];
            gs_constants.surface_height = color_img.desc.extent[1];
			gs_constants.buf_id = splat_data.buf_id;
			gs_constants.locked_color = splat_data.locked_color;
			gs_constants.select_color = splat_data.select_color;

			pass.render([this, color_img = std::move(color_ref),
				depth = std::move(depth_ref),
				num_gaussians, gs_constants,
				pipeline_raster = std::move(pipeline)](rg::RenderPassApi& api) mutable {
					auto [width, height, _] = color_img.desc.extent;

					auto dynamic_offset = api.dynamic_constants()
						->push(gs_constants);

					api.begin_render_pass(
						*gs_point_render_pass,
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
					device->draw_instanced(api.cb, 4, num_gaussians,0,0);
					api.end_render_pass();
				});
		}
		pass.rg->record_pass(std::move(pass.pass));
		return color_img;
    }

	auto GaussianRenderPass::render_color_points(
        rg::RenderGraph& rg,
		rg::Handle<rhi::GpuTexture>& color_img,
		rg::Handle<rhi::GpuTexture>& depth_img,
		const SplatRenderData& splat_data) -> rg::Handle<rhi::GpuTexture>
    {
        auto pass = rg.add_pass("raster gscolor points");
		auto pipeline_desc = rhi::RasterPipelineDesc()
			.with_render_pass(gs_point_render_pass)
			.with_cull_mode(rhi::CullMode::NONE)
			.with_vetex_attribute(false)
			.with_primitive_type(rhi::PrimitiveTopType::TriangleStrip);
		std::vector<std::pair<std::string, std::string>> scaffold_defines;
#if SPLAT_EDIT
		scaffold_defines.push_back({ "SPLAT_EDIT", "1" });
#endif
		auto pipeline = pass.register_raster_pipeline(
			{
			   rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Vertex).with_shader_source({"/shaders/gaussian/gs_color_point_vs.hlsl","main",scaffold_defines}),
			   rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Pixel).with_shader_source({"/shaders/gaussian/gs_point_ps.hlsl","main",scaffold_defines})
			},
			std::move(pipeline_desc)
		);
		auto depth_ref = pass.raster(depth_img, rhi::AccessType::DepthAttachmentWriteStencilReadOnly);
		auto color_ref = pass.raster(color_img, rhi::AccessType::ColorAttachmentWrite);

		{
			const auto num_gaussians = splat_data.model->get_num_gaussians();
			struct GaussianConstants {
				glm::mat4 transform;
				float point_size;
				u32 surface_width;
                u32 surface_height;
                uint buf_id;
			}gs_constants;
            gs_constants.transform = glm::transpose(splat_data.transform.get_world_matrix());
			gs_constants.point_size = glm::clamp(splat_data.model->splat_size,1.0f,100.0f) / 2.0f;
            gs_constants.surface_width = color_img.desc.extent[0];
            gs_constants.surface_height = color_img.desc.extent[1];
			gs_constants.buf_id = splat_data.buf_id;

			pass.render([this, color_img = std::move(color_ref),
				depth = std::move(depth_ref),
				num_gaussians,gs_constants,
				pipeline_raster = std::move(pipeline)](rg::RenderPassApi& api) mutable {
					auto [width, height, _] = color_img.desc.extent;

					auto dynamic_offset = api.dynamic_constants()
						->push(gs_constants);

					api.begin_render_pass(
						*gs_point_render_pass,
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
					device->draw_instanced(api.cb, 4, num_gaussians,0,0);
					api.end_render_pass();
				});
		}
		pass.rg->record_pass(std::move(pass.pass));
		return color_img;
    }

	auto GaussianRenderPass::render_outline(
		rg::RenderGraph& rg,
		rg::Handle<rhi::GpuTexture>& color_img,
		rg::Handle<rhi::GpuTexture>& depth_img,
		rg::Handle<rhi::GpuTexture>& outline_tex,
		const SplatRenderData& splat_data) -> rg::Handle<rhi::GpuTexture>
	{
		const auto splat_edit_mode = g_render_settings.splat_edit_render_mode == 1 ? 1 : 0;
		struct GaussianConstants {
				glm::vec4 color;
				u32 width;
				u32 height;
				f32 alphaCutoff;
				u32 pad;
			}gs_constants;
            gs_constants.color = splat_data.select_color;
			gs_constants.width = color_img.desc.extent[0];
			gs_constants.height = color_img.desc.extent[1];
			gs_constants.alphaCutoff = splat_edit_mode == 1 ? 0.0f : 0.4f;
		rg::RenderPass::new_compute(
			rg.add_pass("splat_outline"), "/shaders/gaussian/gsplat_outline.hlsl")
			.constants(gs_constants)
			.read(outline_tex)
			.write(color_img)
			.dispatch({ color_img.desc.extent[0],color_img.desc.extent[1],1});
		return color_img;
	}

	auto GaussianRenderPass::render_splats(rg::TemporalGraph& rg,
			rg::Handle<rhi::GpuTexture>& color_img,
			rg::Handle<rhi::GpuTexture>& depth_img) -> GSplatRenderOutput
	{
		auto& gs_cmd = renderer->gs_command_queue;

		auto width = depth_img.desc.extent[0];
		auto height = depth_img.desc.extent[1];
		// auto splat_depth_tex = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(PixelFormat::R32G32B32A32_Float, depth_img.desc.extent_2d()),"splat_depth");
		// auto splat_normal_tex = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(PixelFormat::R8G8B8A8_SNorm, depth_img.desc.extent_2d()),"splat_normal");
		// rg::clear_color(rg, splat_depth_tex, { 0.0f,0.0f,0.0f,0.0f });
		// rg::clear_color(rg, splat_normal_tex, { 0.0f,0.0f,0.0f,0.0f });
		if (gs_cmd.size() <= 0) return { color_img };
		const auto outline_enabled = enableOutlineVar.get_value<bool>();
		const auto splat_edit_mode = g_render_settings.splat_edit_render_mode;
		for (const auto& cmd : gs_cmd)
		{
			const auto num_gaussians = cmd.model->get_num_gaussians();
			if (num_gaussians <= 0) continue;
			u32 max_gaussians = 20000000;//drs_align(num_gaussians, 4) * max_overdraw;
			auto gs_model = cmd.model;
			auto gs_buf_id = renderer->get_buf_id(gs_model);
			struct GaussianConstants {
				glm::mat4 transform;
				u32 buf_id;
				u32 surface_width;
				u32 surface_height;
				u32 num_gaussians;
				glm::vec4 locked_color;
				glm::vec4 select_color;
				glm::vec4 tintColor;//w: transparency
				glm::vec4 color_offset;//w: splat_size
			}gs_constants;
			gs_constants.transform = glm::transpose(cmd.transform.get_world_matrix());
			gs_constants.buf_id = gs_buf_id;
			gs_constants.surface_width = color_img.desc.extent[0];
			gs_constants.surface_height = color_img.desc.extent[1];
			gs_constants.tintColor = cmd.tintColor;
			gs_constants.locked_color = glm::vec4(cmd.locked_color.xyz,(f32)(splat_edit_mode));
			gs_constants.select_color = outline_enabled ? glm::vec4(0.0f) : cmd.select_color;
			gs_constants.num_gaussians = num_gaussians;
			gs_constants.color_offset = glm::vec4(cmd.color_offset,cmd.model->splat_size);
			auto point_list_key_buffer = rg.import_res(gs_model->points_key_buf, rhi::AccessType::Nothing);
			auto point_list_value_buffer = rg.import_res(gs_model->points_value_buf, rhi::AccessType::Nothing);
			auto num_visible_buffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(sizeof(u32) * 4, 
				rhi::BufferUsageFlags::STORAGE_BUFFER | 
				rhi::BufferUsageFlags::TRANSFER_SRC | 
				rhi::BufferUsageFlags::TRANSFER_DST), "count_buffer");
			rg::clear_buffer(rg,num_visible_buffer,0);
			rg::RenderPass::new_compute(
				rg.add_pass("clear_points"), "/shaders/gaussian/clear_points.hlsl")
				.write(point_list_key_buffer)
				.write(point_list_value_buffer)
				.constants(gs_constants.num_gaussians)
				.dispatch({ (u32)gs_constants.num_gaussians,1,1 });
			std::vector<std::pair<std::string, std::string>> scaffold_defines;
#if SPLAT_EDIT
			scaffold_defines.push_back({ "SPLAT_EDIT", "1" });
#endif
			rg::RenderPass::new_compute(
				rg.add_pass("gsplat_viewz"), "/shaders/gaussian/gsplat_viewz_cs.hlsl",scaffold_defines)
				.write(point_list_key_buffer)
				.write(point_list_value_buffer)
				.write(num_visible_buffer)
				.constants(gs_constants)
				.raw_descriptor_set(1, renderer->binldess_descriptorset())
				.dispatch({ (u32)gs_constants.num_gaussians,1,1 });
			// auto [sorted_key, sorted_value] = gpu_sort_indirect(rg, point_list_key_buffer, point_list_value_buffer,num_visible_buffer);
			auto [sorted_key, sorted_value] = gpu_sort(rg, point_list_key_buffer, point_list_value_buffer, gs_constants.num_gaussians);
			struct GaussianCropConstants {
				u32 surface_width;
				u32 surface_height;
				u32 num_gaussians;
				u32 max_gaussians;
				glm::mat4 transform;

				uint    buf_id;
				int     crop_num;
				glm::uvec2   padding;

				glm::vec4 crop_min[8];
				glm::vec4 crop_max[8];
				uint    crop_type[8];
			}crop_constants;
			crop_constants = {width, height, (u32)gs_constants.num_gaussians, max_gaussians};
			crop_constants.transform = glm::transpose(cmd.transform.get_world_matrix());
			crop_constants.buf_id = gs_buf_id;
			crop_constants.crop_num = cmd.crop_data.size();
			for (auto crop_id = 0; crop_id < cmd.crop_data.size(); crop_id++)
			{
				auto& crop_data = *static_cast<GaussianCrop::GaussianCropData*>(cmd.crop_data[crop_id]);
				auto transform_box = crop_data.bdbox.transformed(crop_data.transform.get_local_matrix());
				auto transform_sphere = crop_data.bdsphere.transformed(crop_data.transform.get_local_matrix());
				if( crop_data.get_crop_type() == GaussianCrop::CropType::Box)
				{
					crop_constants.crop_min[crop_id].xyz = transform_box.min();
					crop_constants.crop_max[crop_id].xyz = transform_box.max();
				}
				else
				{
					crop_constants.crop_min[crop_id].xyz = transform_sphere.get_center();
					crop_constants.crop_max[crop_id].xyz = glm::vec3(transform_sphere.get_radius(), 0, 0);
				}
				crop_constants.crop_type[crop_id] = crop_data.crop_op;
			}
			if(crop_constants.crop_num > 0 )
			{
				rg::RenderPass::new_compute(
					rg.add_pass("gs_crop"), "/shaders/gaussian/gsplat_crop.hlsl")
					.constants(crop_constants)
					.raw_descriptor_set(1, renderer->binldess_descriptorset())
					.dispatch({ (u32)gs_constants.num_gaussians,1,1 });
			}
			//reset gaussian state
			{
				const auto hasCrop = cmd.crop_data.size() > 0;
				static bool preHasCrop = hasCrop;
				if (preHasCrop != hasCrop && !hasCrop ) {
					rg::RenderPass::new_compute(
						rg.add_pass("gs_reset"), "/shaders/gaussian/gsplat_reset.hlsl",scaffold_defines)
						.constants(glm::uvec4(gs_constants.num_gaussians, max_gaussians, gs_buf_id,0))
						.raw_descriptor_set(1, renderer->binldess_descriptorset())
						.dispatch({ (u32)gs_constants.num_gaussians,1,1 });
				}
				preHasCrop = hasCrop;
			}
			if (g_render_settings.gs_vis_type == (int)(GaussianRenderType::Point)) {
				render_color_points(rg, color_img, depth_img, {gs_buf_id, cmd.model, cmd.transform, cmd.select_color, cmd.locked_color});
				continue;
			}

			std::vector<std::pair<std::string, std::string>> defines;
			if (g_render_settings.gs_vis_type == (int)(GaussianRenderType::Depth)) {
				defines = std::vector<std::pair<std::string, std::string>>{
				   {"VISUALIZE_DEPTH", "1"}
				};
			}
			else if (g_render_settings.gs_vis_type == (int)(GaussianRenderType::Normal)) {
				defines = std::vector<std::pair<std::string, std::string>>{
			   {"VISUALIZE_NORMAL", "1"}
				};
			}
			else if (g_render_settings.gs_vis_type == (int)(GaussianRenderType::Rings) ||
					g_render_settings.splat_edit_render_mode == 1){
				defines = std::vector<std::pair<std::string, std::string>>{
			   {"VISUALIZE_RINGS", "1"}
				};
			}
			else if (g_render_settings.gs_vis_type == (int)(GaussianRenderType::Ellipsoids)) {
				defines = std::vector<std::pair<std::string, std::string>>{
			   {"VISUALIZE_ELLIPSOIDS", "1"}
				};
			}
			if( g_render_settings.gs_vis_type == (int)(GaussianRenderType::Splat) && g_render_settings.splat_edit_render_mode != 1)
				defines.push_back({"GSPLAT_AA", cmd.model->antialiased() ? "1" : "0"});
			if(g_render_settings.gs_vis_type == (int)(GaussianRenderType::Splat))
				defines.push_back({"SH_DEGREE", std::to_string(cmd.sh_degree)});
			else
				defines.push_back({"SH_DEGREE", std::to_string(0)});
#if SPLAT_EDIT
			defines.push_back({ "SPLAT_EDIT", "1" });
#endif
			auto pipeline_desc = rhi::RasterPipelineDesc()
				.with_render_pass(gsplat_render_pass)
				.with_cull_mode(rhi::CullMode::NONE)
				.with_vetex_attribute(false)
				.with_depth_test(true)
				.with_depth_write(false)
				.with_blend_mode(rhi::BlendMode::OneOneMinusSrcAlpha)
				.with_primitive_type(rhi::PrimitiveTopType::TriangleStrip);

			auto pass = rg.add_pass("raster gs splats");
			auto pipeline = pass.register_raster_pipeline(
				{
				   rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Vertex).with_shader_source({"/shaders/gaussian/gsplat_vs.hlsl","main",defines}),
				   rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Pixel).with_shader_source({"/shaders/gaussian/gsplat_ps.hlsl","main",defines})
				},
				std::move(pipeline_desc)
			);	
		
			auto indirect_args_buf = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(
				(sizeof(rhi::IndirectDrawArgsInstanced)),
				rhi::BufferUsageFlags::SHADER_DEVICE_ADDRESS | rhi::BufferUsageFlags::INDIRECT_BUFFER),
				"gsplat.indirect_args_buf");
			rg::RenderPass::new_compute(
				rg.add_pass("gsplat draw indirect args"),
				"/shaders/gaussian/prepare_dispatch_args.hlsl"
			)
			.read(num_visible_buffer)
			.write(indirect_args_buf)
			.dispatch({ 1, 1, 1 });
			pass.render([this,
				color_ref = pass.raster(color_img, rhi::AccessType::ColorAttachmentWrite),
				depth_ref = pass.raster(depth_img, rhi::AccessType::DepthAttachmentWriteStencilReadOnly),
				args_buffer_ref = pass.read(indirect_args_buf, rhi::AccessType::IndirectBuffer),
				gs_constants,cmd,
				sorted_buffer_ref = pass.read(sorted_value, rhi::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer),
				pipeline_raster = std::move(pipeline)](rg::RenderPassApi& api) mutable {
					auto [width, height, _] = color_ref.desc.extent;

					auto dynamic_offset = api.dynamic_constants()
						->push(gs_constants);

					api.begin_render_pass(
						*gsplat_render_pass,
						{ width, height },
						{
							std::pair{color_ref,rhi::GpuTextureViewDesc()},
						},
						std::pair{ depth_ref, rhi::GpuTextureViewDesc().with_aspect_mask(rhi::ImageAspectFlags::DEPTH) }
					);

					api.set_default_view_and_scissor({ width,height });
					//write sorted_value buffer
					std::vector<rg::RenderPassBinding> bindings = { 
						rg::RenderPassBinding::DynamicConstants(dynamic_offset), 
						sorted_buffer_ref.bind()
					};

					auto res = rg::RenderPassPipelineBinding<rg::RgRasterPipelineHandle>::from(pipeline_raster)
						.descriptor_set(0, &bindings)
						.raw_descriptor_set(1, renderer->binldess_descriptorset());
					auto bound_raster = api.bind_raster_pipeline(res);
					auto device = api.device();

					// device->draw_instanced(api.cb, 4, gs_constants.num_gaussians,0,0);
					device->draw_instanced_indirect(api.cb, api.resources.buffer(args_buffer_ref), 0);

					api.end_render_pass();
			});
			pass.rg->record_pass(std::move(pass.pass));

			if (splat_edit_mode == 0) // points
			{
				render_points(rg, color_img, depth_img, {gs_buf_id, cmd.model, cmd.transform, cmd.select_color, cmd.locked_color});
			}
			if(outline_enabled)
			{
				auto outline_pipeline_desc = rhi::RasterPipelineDesc()
					.with_render_pass(gsplat_render_pass)
					.with_cull_mode(rhi::CullMode::NONE)
					.with_vetex_attribute(false)
					.with_depth_test(false)
					.with_blend_mode(rhi::BlendMode::OneOneMinusSrcAlpha)
					.with_primitive_type(rhi::PrimitiveTopType::TriangleStrip);
				defines.push_back({ "OUTLINE_PASS", "1" });
				auto outline_tex = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(PixelFormat::R8G8B8A8_UNorm, depth_img.desc.extent_2d()), "splat_outline_tex");
				rg::clear_color(rg, outline_tex, { 0.0f,0.0f,0.0f,0.0f });
				auto outline_pass = rg.add_pass("splat outline");
				auto outline_pipeline = outline_pass.register_raster_pipeline(
					{
					   rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Vertex).with_shader_source({"/shaders/gaussian/gsplat_vs.hlsl","main",defines}),
					   rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Pixel).with_shader_source({"/shaders/gaussian/gsplat_ps.hlsl","main",defines})
					},
					std::move(outline_pipeline_desc)
				);

				outline_pass.render([this,
					color_ref = outline_pass.raster(outline_tex, rhi::AccessType::ColorAttachmentWrite),
					depth_ref = outline_pass.raster(depth_img, rhi::AccessType::DepthAttachmentWriteStencilReadOnly),
					gs_constants, cmd,
					sorted_buffer_ref = outline_pass.read(sorted_value, rhi::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer),
					pipeline_raster = std::move(outline_pipeline)](rg::RenderPassApi& api) mutable {
						auto [width, height, _] = color_ref.desc.extent;

						auto dynamic_offset = api.dynamic_constants()
							->push(gs_constants);

						api.begin_render_pass(
							*gsplat_render_pass,
							{ width, height },
						{
							std::pair{color_ref,rhi::GpuTextureViewDesc()},
						},
						std::pair{ depth_ref, rhi::GpuTextureViewDesc().with_aspect_mask(rhi::ImageAspectFlags::DEPTH) }
						);

						api.set_default_view_and_scissor({ width,height });
						//write sorted_value buffer
						std::vector<rg::RenderPassBinding> bindings = {
							rg::RenderPassBinding::DynamicConstants(dynamic_offset),
							sorted_buffer_ref.bind(),
						};

						auto res = rg::RenderPassPipelineBinding<rg::RgRasterPipelineHandle>::from(pipeline_raster)
							.descriptor_set(0, &bindings)
							.raw_descriptor_set(1, renderer->binldess_descriptorset());
						auto bound_raster = api.bind_raster_pipeline(res);
						auto device = api.device();

						device->draw_instanced(api.cb, 4, gs_constants.num_gaussians, 0, 0);

						api.end_render_pass();
					});
				outline_pass.rg->record_pass(std::move(outline_pass.pass));
				render_outline(rg, color_img, depth_img, outline_tex,{gs_buf_id, cmd.model, cmd.transform, cmd.select_color, cmd.locked_color});
			}
		}
		rg::RenderPass::new_compute(
			rg.add_pass("reset color alpha"),
			"/shaders/clear_alpha.hlsl")
			.write(color_img)
			.constants(glm::ivec4(1,0,0,0))
			.dispatch({ color_img.desc.extent[0],  color_img.desc.extent[1], 1 });
		
		return { color_img };
	}
}
