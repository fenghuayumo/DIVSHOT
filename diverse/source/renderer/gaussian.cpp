#include "gaussian.h"
#include "defered_renderer.h"
#include "drs_rg/image_op.h"
#include "drs_rg/buffer_op.h"
#include "scene/components/global_transform_component.h"
#include "scene/component/gaussian_component.h"
#include "scene/component/gaussian_crop.h"
#include "scene/camera/camera.h"
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
		auto device = renderer->get_device();
		rhi::RenderPassDesc desc = {
		{
			rhi::RenderPassAttachmentDesc::create(PixelFormat::R8G8B8A8_UNorm).load_input(),
		},
			rhi::RenderPassAttachmentDesc::create(PixelFormat::D32_Float).load_input()
		};
		gs_point_render_pass = device->create_render_pass(desc);

	 	desc = {
		{
            rhi::RenderPassAttachmentDesc::create(PixelFormat::R8G8B8A8_UNorm).load_input(),
		},
			rhi::RenderPassAttachmentDesc::create(PixelFormat::D32_Float).load_input()
		};
		gsplat_render_pass = device->create_render_pass(desc);

		// GUT mesh shader render pass
		desc = {
		{
			rhi::RenderPassAttachmentDesc::create(PixelFormat::R8G8B8A8_UNorm).load_input(),
		},
			rhi::RenderPassAttachmentDesc::create(PixelFormat::D32_Float).load_input()
		};
		gut_mesh_render_pass = device->create_render_pass(desc);
	}

	GaussianRenderPass::GaussianRenderPass()
	{
	}

	auto GaussianRenderPass::render(rg::TemporalGraph& rg,
		rg::Handle<rhi::GpuTexture>& color_img,
		rg::Handle<rhi::GpuTexture>& depth_img) -> GSplatRenderOutput
	{
		const auto& render_settings = renderer->get_frame_render_settings();
		if (render_settings.splat_render_method == SplatRenderMethod::GUT)
		{
			return render_gut_mesh_shader(rg, color_img, depth_img);
		}
		return render_splats(rg, color_img, depth_img);
	}

    auto GaussianRenderPass::render_points(rg::TemporalGraph& rg,
			rg::Handle<rhi::GpuTexture>& color_img,
			rg::Handle<rhi::GpuTexture>& depth_img,
			const SplatRenderData& splat_data) -> rg::Handle<rhi::GpuTexture>
    {
		const auto& render_settings = renderer->get_frame_render_settings();
		if(render_settings.gs_point_size <= 0 ) return color_img;
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
		gs_constants.point_size = glm::clamp(render_settings.gs_point_size,0.0f,100.0f) / 2.0f;
		gs_constants.surface_width = color_img.desc.extent[0];
		gs_constants.surface_height = color_img.desc.extent[1];
		gs_constants.buf_id = splat_data.buf_id;
		gs_constants.locked_color = splat_data.locked_color;
		gs_constants.select_color = splat_data.select_color;

		rg::RenderPass::new_raster(
			rg.add_pass("raster gs points"),
            std::move(pipeline_desc),
            rhi::ShaderSource{"/shaders/gaussian/gs_point_vs.hlsl","main",defines},
            rhi::ShaderSource{"/shaders/gaussian/gs_point_ps.hlsl","main",defines}
        )
        .raster_depth(depth_img)
        .raster_color(color_img)
		.constants(gs_constants)
		.raw_descriptor_set(1, renderer->binldess_descriptorset())
		.draw_instanced(*gs_point_render_pass, 4, num_gaussians);
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
		const auto num_gaussians = splat_data.model->get_num_gaussians();
		struct GaussianConstants {
			glm::mat4 transform;
			float point_size;
			u32 surface_width;
			u32 surface_height;
			uint buf_id;
		}gs_constants;
		gs_constants.transform = glm::transpose(splat_data.transform.get_world_matrix());
		gs_constants.point_size = glm::clamp(splat_data.model->splat_size, 1.0f, 100.0f) / 2.0f;
		gs_constants.surface_width = color_img.desc.extent[0];
		gs_constants.surface_height = color_img.desc.extent[1];
		gs_constants.buf_id = splat_data.buf_id;

		rg::RenderPass::new_raster(
			rg.add_pass("raster gs points"),
			std::move(pipeline_desc),
			rhi::ShaderSource{ "/shaders/gaussian/gs_color_point_vs.hlsl","main",scaffold_defines },
			rhi::ShaderSource{ "/shaders/gaussian/gs_point_ps.hlsl","main",scaffold_defines }
		)
		.raster_depth(depth_img)
		.raster_color(color_img)
		.constants(gs_constants)
		.raw_descriptor_set(1, renderer->binldess_descriptorset())
		.draw_instanced(*gs_point_render_pass, 4, num_gaussians);
		return color_img;
    }

	auto GaussianRenderPass::render_outline(
		rg::RenderGraph& rg,
		rg::Handle<rhi::GpuTexture>& color_img,
		rg::Handle<rhi::GpuTexture>& depth_img,
		rg::Handle<rhi::GpuTexture>& outline_tex,
			const SplatRenderData& splat_data) -> rg::Handle<rhi::GpuTexture>
	{
		const auto& render_settings = renderer->get_frame_render_settings();
		const auto splat_edit_mode = render_settings.splat_edit_render_mode == 1 ? 1 : 0;
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
		const auto& render_settings = renderer->get_frame_render_settings();

		auto width = depth_img.desc.extent[0];
		auto height = depth_img.desc.extent[1];
		// auto splat_depth_tex = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(PixelFormat::R32G32B32A32_Float, depth_img.desc.extent_2d()),"splat_depth");
		// auto splat_normal_tex = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(PixelFormat::R8G8B8A8_SNorm, depth_img.desc.extent_2d()),"splat_normal");
		// rg::clear_color(rg, splat_depth_tex, { 0.0f,0.0f,0.0f,0.0f });
		// rg::clear_color(rg, splat_normal_tex, { 0.0f,0.0f,0.0f,0.0f });
		if (gs_cmd.size() <= 0) return { color_img };
		const auto outline_enabled = enableOutlineVar.get_value<bool>();
		const auto splat_edit_mode = render_settings.splat_edit_render_mode;
		for (const auto& cmd : gs_cmd)
		{
			const auto num_gaussians = cmd.model->get_num_gaussians();
			if (num_gaussians <= 0) continue;
			u32 max_gaussians = 20000000;//drs_align(num_gaussians, 4) * max_overdraw;
			auto* gs_model = cmd.model.get();
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
			gs_constants.transform = glm::transpose(cmd.transform.world_matrix);
			gs_constants.buf_id = gs_buf_id;
			gs_constants.surface_width = color_img.desc.extent[0];
			gs_constants.surface_height = color_img.desc.extent[1];
			gs_constants.tintColor = cmd.tintColor;
			gs_constants.locked_color = glm::vec4(cmd.locked_color.xyz,(f32)(splat_edit_mode));
			gs_constants.select_color = outline_enabled ? glm::vec4(0.0f) : cmd.select_color;
			gs_constants.num_gaussians = num_gaussians;
			gs_constants.color_offset = glm::vec4(cmd.color_offset,cmd.model->splat_size);
			auto point_list_key_buffer = rg.import_res(gs_model->get_points_key_buf(), rhi::AccessType::Nothing);
			auto point_list_value_buffer = rg.import_res(gs_model->get_points_value_buf(), rhi::AccessType::Nothing);
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
			// Use indirect version for dynamic element count from num_visible_buffer
			auto [sorted_key, sorted_value] = gpu_sort_indirect(rg, point_list_key_buffer, point_list_value_buffer, num_visible_buffer);
			// Alternative: use direct version if count is known at CPU side
			// auto [sorted_key, sorted_value] = gpu_sort(rg, point_list_key_buffer, point_list_value_buffer, gs_constants.num_gaussians);
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
			crop_constants.transform = glm::transpose(cmd.transform.world_matrix);
			crop_constants.buf_id = gs_buf_id;
			crop_constants.crop_num = cmd.crop_data.size();
			for (auto crop_id = 0; crop_id < cmd.crop_data.size(); crop_id++)
			{
				auto& crop_data = cmd.crop_data[crop_id];
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
			if (render_settings.gs_vis_type == (int)(GaussianRenderType::Point)) {
				// TODO: Migrate SplatRenderData to use GlobalTransform instead of maths::Transform
				// render_color_points(rg, color_img, depth_img, {gs_buf_id, gs_model, cmd.transform, cmd.select_color, cmd.locked_color});
				continue;
			}

			std::vector<std::pair<std::string, std::string>> defines;
			if (render_settings.gs_vis_type == (int)(GaussianRenderType::Depth)) {
				defines = std::vector<std::pair<std::string, std::string>>{
				   {"VISUALIZE_DEPTH", "1"}
				};
			}
			else if (render_settings.gs_vis_type == (int)(GaussianRenderType::Normal)) {
				defines = std::vector<std::pair<std::string, std::string>>{
			   {"VISUALIZE_NORMAL", "1"}
				};
			}
			else if (render_settings.gs_vis_type == (int)(GaussianRenderType::Rings) ||
					render_settings.splat_edit_render_mode == 1){
				defines = std::vector<std::pair<std::string, std::string>>{
			   {"VISUALIZE_RINGS", "1"}
				};
			}
			else if (render_settings.gs_vis_type == (int)(GaussianRenderType::Ellipsoids)) {
				defines = std::vector<std::pair<std::string, std::string>>{
			   {"VISUALIZE_ELLIPSOIDS", "1"}
				};
			}
			if( render_settings.gs_vis_type == (int)(GaussianRenderType::Splat) && render_settings.splat_edit_render_mode != 1)
				defines.push_back({"GSPLAT_AA", cmd.mip_antialiased ? "1" : "0"});
			if(render_settings.gs_vis_type == (int)(GaussianRenderType::Splat))
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
				.with_blend_mode(rhi::BlendMode::SrcAlphaOneMinusSrcAlpha)
				.with_primitive_type(rhi::PrimitiveTopType::TriangleStrip);


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
			
			rg::RenderPass::new_raster(
				rg.add_pass("raster gs splats"),
				std::move(pipeline_desc),
				rhi::ShaderSource{ "/shaders/gaussian/gsplat_vs.hlsl","main",defines },
				rhi::ShaderSource{ "/shaders/gaussian/gsplat_ps.hlsl","main",defines }
			)
			.raster_color(color_img)
			.raster_depth(depth_img)
			.constants(gs_constants)
			.read(sorted_value)
			.raw_descriptor_set(1, renderer->binldess_descriptorset())
			.draw_indirect_instanced(*gsplat_render_pass, indirect_args_buf, 0);

			if (splat_edit_mode == 0) // points
			{
				// TODO: Migrate SplatRenderData to use GlobalTransform instead of maths::Transform
				// render_points(rg, color_img, depth_img, {gs_buf_id, gs_model, cmd.transform, cmd.select_color, cmd.locked_color});
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
				
				rg::RenderPass::new_raster(
					rg.add_pass("raster gs splats"),
					std::move(pipeline_desc),
					rhi::ShaderSource{ "/shaders/gaussian/gsplat_vs.hlsl","main",defines },
					rhi::ShaderSource{ "/shaders/gaussian/gsplat_ps.hlsl","main",defines }
				)
				.raster_color(outline_tex)
				.raster_depth(depth_img)
				.constants(gs_constants)
				.read(sorted_value)
				.raw_descriptor_set(1, renderer->binldess_descriptorset())
				.draw_indirect_instanced(*gsplat_render_pass, indirect_args_buf, 0);
				// TODO: Migrate SplatRenderData to use GlobalTransform instead of maths::Transform
				// render_outline(rg, color_img, depth_img, outline_tex,{gs_buf_id, gs_model, cmd.transform, cmd.select_color, cmd.locked_color});
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

	auto GaussianRenderPass::render_gut_mesh_shader(rg::TemporalGraph& rg,
		rg::Handle<rhi::GpuTexture>& color_img,
		rg::Handle<rhi::GpuTexture>& depth_img) -> GSplatRenderOutput
	{
		auto& gs_cmd = renderer->gs_command_queue;
		
		auto width = depth_img.desc.extent[0];
		auto height = depth_img.desc.extent[1];
		
		if (gs_cmd.size() <= 0) return { color_img };
		
		for (const auto& cmd : gs_cmd)
		{
			const auto num_gaussians = cmd.model->get_num_gaussians();
			if (num_gaussians <= 0) continue;
			
			auto* gs_model = cmd.model.get();
			auto gs_buf_id = renderer->get_buf_id(gs_model);
			
			// GUT constants for mesh shader
			struct GutMeshConstants {
				glm::mat4 transform;
				u32 buf_id;
				u32 surface_width;
				u32 surface_height;
				u32 num_gaussians;
				glm::vec4 locked_color;
				glm::vec4 select_color;
				glm::vec4 tintColor;
				glm::vec4 color_offset;
			} gut_constants;
			
			gut_constants.transform = glm::transpose(cmd.transform.world_matrix);
			gut_constants.buf_id = gs_buf_id;
			gut_constants.surface_width = color_img.desc.extent[0];
			gut_constants.surface_height = color_img.desc.extent[1];
			gut_constants.num_gaussians = num_gaussians;
			const auto& render_settings = renderer->get_frame_render_settings();
			gut_constants.locked_color = glm::vec4(cmd.locked_color.xyz, (f32)(render_settings.splat_edit_render_mode));
			gut_constants.select_color = cmd.select_color;
			gut_constants.tintColor = cmd.tintColor;
			gut_constants.color_offset = glm::vec4(cmd.color_offset, cmd.model->splat_size);
			
			// Create sorting buffers
			auto point_list_key_buffer = rg.import_res(gs_model->get_points_key_buf(), rhi::AccessType::Nothing);
			auto point_list_value_buffer = rg.import_res(gs_model->get_points_value_buf(), rhi::AccessType::Nothing);
			auto num_visible_buffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(sizeof(u32) * 4, 
				rhi::BufferUsageFlags::STORAGE_BUFFER | 
				rhi::BufferUsageFlags::TRANSFER_SRC | 
				rhi::BufferUsageFlags::TRANSFER_DST), "count_buffer");
			
			rg::clear_buffer(rg, num_visible_buffer, 0);
			
			// Clear points
			rg::RenderPass::new_compute(
				rg.add_pass("clear_points_gut"), "/shaders/gaussian/clear_points.hlsl")
				.write(point_list_key_buffer)
				.write(point_list_value_buffer)
				.constants(gut_constants.num_gaussians)
				.dispatch({ (u32)gut_constants.num_gaussians, 1, 1 });
			
			std::vector<std::pair<std::string, std::string>> defines;
#if SPLAT_EDIT
			defines.push_back({ "SPLAT_EDIT", "1" });
#endif
			defines.push_back({ "SH_DEGREE", std::to_string(cmd.sh_degree) });
			defines.push_back({ "GSPLAT_AA", cmd.mip_antialiased ? "1" : "0" });
			
			// View-z sorting pass
			rg::RenderPass::new_compute(
				rg.add_pass("gsplat_viewz_gut"), "/shaders/gaussian/gsplat_viewz_cs.hlsl", defines)
				.write(point_list_key_buffer)
				.write(point_list_value_buffer)
				.write(num_visible_buffer)
				.constants(gut_constants)
				.raw_descriptor_set(1, renderer->binldess_descriptorset())
				.dispatch({ (u32)gut_constants.num_gaussians, 1, 1 });
			
			// GPU sort
			auto [sorted_key, sorted_value] = gpu_sort_indirect(rg, point_list_key_buffer, point_list_value_buffer, num_visible_buffer);
			
			// Prepare indirect args for mesh shader
			auto indirect_args_buf = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(
				sizeof(rhi::IndirectDrawMeshTasksArgs) + sizeof(u32) * 4,
				rhi::BufferUsageFlags::SHADER_DEVICE_ADDRESS | rhi::BufferUsageFlags::INDIRECT_BUFFER | rhi::BufferUsageFlags::STORAGE_BUFFER),
				"gut_mesh.indirect_args_buf");
			
			// Prepare dispatch args for mesh shader (workgroup count)
			struct PrepareGutArgsConstants {
				u32 workgroup_size;
				u32 padding[3];
			} prepare_args_constants;
			prepare_args_constants.workgroup_size = 32; // MESH_SHADER_WORKGROUP_SIZE
			
			rg::RenderPass::new_compute(
				rg.add_pass("prepare_gut_mesh_args"),
				"/shaders/gaussian/prepare_gut_mesh_args.hlsl")
				.read(num_visible_buffer)
				.write(indirect_args_buf)
				.constants(prepare_args_constants)
				.dispatch({ 1, 1, 1 });
			
			// Mesh shader pipeline descriptor
			auto pipeline_desc = rhi::MeshShaderPipelineDesc()
				.with_render_pass(gut_mesh_render_pass)
				.with_cull_mode(rhi::CullMode::NONE)
				.with_depth_test(true)
				.with_depth_write(false)
				.with_blend_mode(rhi::BlendMode::SrcAlphaOneMinusSrcAlpha)
				.with_push_constants(sizeof(GutMeshConstants));
			
			// Render using mesh shader pipeline
			rg::RenderPass::new_mesh_shader(
				rg.add_pass("gut_mesh_shader_splats"),
				std::move(pipeline_desc),
				rhi::ShaderSource{ "/shaders/gaussian/gsplat_gut.mesh.hlsl", "main", defines },
				rhi::ShaderSource{ "/shaders/gaussian/gsplat_gut.frag.hlsl", "main", defines }
			)
			.raster_color(color_img)
			.raster_depth(depth_img)
			.constants(gut_constants)
			.read(sorted_value)
			.read(indirect_args_buf)
			.raw_descriptor_set(1, renderer->binldess_descriptorset())
			.draw_mesh_tasks_indirect(*gut_mesh_render_pass, indirect_args_buf, 0, 1, sizeof(rhi::IndirectDrawMeshTasksArgs));
		}
		
		// Reset alpha channel
		rg::RenderPass::new_compute(
			rg.add_pass("reset color alpha gut"),
			"/shaders/clear_alpha.hlsl")
			.write(color_img)
			.constants(glm::ivec4(1, 0, 0, 0))
			.dispatch({ color_img.desc.extent[0], color_img.desc.extent[1], 1 });
		
		return { color_img };
	}

}
