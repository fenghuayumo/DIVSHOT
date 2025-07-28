#include "taa.h"
#include "drs_rg/simple_pass.h"
#include "drs_rg/image_op.h"
#include "utility/cmd_variable.h"
namespace diverse
{
	auto TaaRenderer::render(rg::TemporalGraph& rg, const rg::Handle<rhi::GpuTexture>& input_tex, const rg::Handle<rhi::GpuTexture>& reprojection_map, const rg::Handle<rhi::GpuTexture>& depth_tex, const std::array<u32, 2>& output_extent) -> TaaOutput
	{
		auto [temporal_output_tex, history_tex] = temporal_tex.get_output_and_history(rg, temporal_tex_desc(output_extent));

		auto [temporal_velocity_output_tex, velocity_history_tex] = temporal_velocity_tex.get_output_and_history(rg,
					rhi::GpuTextureDesc::new_2d(PixelFormat::R16G16_Float, output_extent)
					.with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::STORAGE));
		
		auto reprojected_history_img = rg.create<rhi::GpuTexture>(temporal_tex_desc(output_extent),"taa.reprojected_history");
		auto closest_velocity_img = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(PixelFormat::R16G16_Float, output_extent), "taa.closest_velocity_img");

		
		rg::RenderPass::new_compute(rg.add_pass("reproject taa"), "/shaders/taa/reproject_history.hlsl")
		.read(history_tex)
		.read(reprojection_map)
		.read_aspect(depth_tex, rhi::ImageAspectFlags::DEPTH)
		.write(reprojected_history_img)
		.write(closest_velocity_img)
		.constants(std::tuple{
			input_tex.desc.extent_inv_extent_2d(),
			reprojected_history_img.desc.extent_inv_extent_2d()
		})
		.dispatch(reprojected_history_img.desc.extent);

		auto [smooth_var_output_tex, smooth_var_history_tex] = temporal_smooth_var_tex.get_output_and_history(
																rg,
																rhi::GpuTextureDesc::new_2d(PixelFormat::R16G16B16A16_Float, output_extent)
																.with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::STORAGE)
																);
		auto filtered_input_img = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(
			PixelFormat::R16G16B16A16_Float,
			input_tex.desc.extent_2d()),
			"taa.filtered_input_img"
			);

		auto filtered_input_deviation_img = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(
			PixelFormat::R16G16B16A16_Float,
			input_tex.desc.extent_2d()),
			"taa.filtered_input_deviation_img"
			);

		rg::RenderPass::new_compute(
			rg.add_pass("taa filter input"),
			"/shaders/taa/filter_input.hlsl"
		 )
		.read(input_tex)
		.read_aspect(depth_tex, rhi::ImageAspectFlags::DEPTH)
		.write(filtered_input_img)
		.write(filtered_input_deviation_img)
		.dispatch(filtered_input_img.desc.extent);

		auto filtered_history_img = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(
			PixelFormat::R16G16B16A16_Float,
			filtered_input_img.desc.extent_2d()));
		rg::RenderPass::new_compute(
			rg.add_pass("taa filter history"),
			"/shaders/taa/filter_history.hlsl"
			)
		.read(reprojected_history_img)
		.write(filtered_history_img)
		.constants(std::tuple{
			reprojected_history_img.desc.extent_inv_extent_2d(),
			input_tex.desc.extent_inv_extent_2d()
			})
		.dispatch(filtered_history_img.desc.extent);

		auto input_prob_img = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(
			PixelFormat::R16_Float,
			input_tex.desc.extent_2d()
			),
			"taa.input_prob_img"
			);

		rg::RenderPass::new_compute(
			rg.add_pass("taa input prob"),
			"/shaders/taa/input_prob.hlsl"
			)
		.read(input_tex)
		.read(filtered_input_img)
		.read(filtered_input_deviation_img)
		.read(reprojected_history_img)
		.read(filtered_history_img)
		.read(reprojection_map)
		.read_aspect(depth_tex, rhi::ImageAspectFlags::DEPTH)
		.read(smooth_var_history_tex)
		.read(velocity_history_tex)
		.write(input_prob_img)
		.constants(input_tex.desc.extent_inv_extent_2d())
		.dispatch(input_prob_img.desc.extent);

		auto prob_filtered1_img = rg.create<rhi::GpuTexture>(input_prob_img.desc,"taa.prob_filtered1_img");
		rg::RenderPass::new_compute(
			rg.add_pass("taa prob filter"),
			"/shaders/taa/filter_prob.hlsl"
			)
			.read(input_prob_img)
			.write(prob_filtered1_img)
			.dispatch(prob_filtered1_img.desc.extent);

		auto prob_filtered2_img = rg.create<rhi::GpuTexture>(input_prob_img.desc,"taa.prob_filtered2_img");
		rg::RenderPass::new_compute(
			rg.add_pass("taa prob filter2"),
			"/shaders/taa/filter_prob2.hlsl"
			)
			.read(prob_filtered1_img)
			.write(prob_filtered2_img)
			.dispatch(prob_filtered1_img.desc.extent);

		input_prob_img = prob_filtered2_img;

		auto this_frame_output_img = rg.create<rhi::GpuTexture>(temporal_tex_desc(output_extent),"taa.frame_output");
		rg::RenderPass::new_compute(rg.add_pass("taa"), "/shaders/taa/taa.hlsl")
			.read(input_tex)
			.read(reprojected_history_img)
			.read(reprojection_map)
			.read(closest_velocity_img)
			.read(velocity_history_tex)
			.read_aspect(depth_tex, rhi::ImageAspectFlags::DEPTH)
			.read(smooth_var_history_tex)
			.read(input_prob_img)
			.write(temporal_output_tex)
			.write(this_frame_output_img)
			.write(smooth_var_output_tex)
			.write(temporal_velocity_output_tex)
			.constants(std::tuple{
				input_tex.desc.extent_inv_extent_2d(),
				temporal_output_tex.desc.extent_inv_extent_2d()
				})
			.dispatch(temporal_output_tex.desc.extent);

		return TaaOutput{
			temporal_output_tex,
			this_frame_output_img
		};
	}

	auto TaaRenderer::accumulate_blend(rg::TemporalGraph& rg,
		const rg::Handle<rhi::GpuTexture>& input) -> rg::Handle<rhi::GpuTexture>
    {
		auto v = CmadVariableMgr::get().find("r.accumulate.spp");
		auto spp = v ? v->get_value<int>() : 1;
        auto [temporal_output_tex, history_tex] = accumulate_tex.get_output_and_history(rg, temporal_tex_desc(input.desc.extent_2d()));
		static int accumulate_num = 0;
		if ((++accumulate_num % spp) == 0)
		{
			rg::clear_color(rg,temporal_output_tex,{0,0,0,0});
			rg::clear_color(rg, history_tex,{0,0,0,0});
			accumulate_num = 0;
		}
		rg::RenderPass::new_compute(rg.add_pass("taa.accumulate"), "/shaders/taa/accumulate_blend.hlsl")
			.read(input)
			.read(history_tex)
			.write(temporal_output_tex)
			.dispatch(temporal_output_tex.desc.extent);
		return temporal_output_tex;
    }
}