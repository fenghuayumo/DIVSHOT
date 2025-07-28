#include "drs_rg/simple_pass.h"
#include "gbuffer_depth.h"

namespace diverse
{
	auto motion_blur(rg::RenderGraph& rg,
		const rg::Handle<rhi::GpuTexture>& input,
		const rg::Handle<rhi::GpuTexture>& depth,
		const rg::Handle<rhi::GpuTexture>& reprojection_map) -> rg::Handle<rhi::GpuTexture>
	{
		const u32 VELOCITY_TILE_SIZE = 16;
		auto velocity_reduced_x = rg.create<rhi::GpuTexture>(
			reprojection_map.get_desc().div_up_extent({VELOCITY_TILE_SIZE, 1, 1}).with_format(PixelFormat::R16G16_Float),
			"motion_lur.velocity_reduced_x"
		);
		rg::RenderPass::new_compute(
		rg.add_pass("velocity reduce x"),
		"/shaders/motion_blur/velocity_reduce_x.hlsl"
			)
			.read(reprojection_map)
			.write( velocity_reduced_x)
			.dispatch(velocity_reduced_x.desc.extent);
		
		auto velocity_reduced_y = rg.create<rhi::GpuTexture>(
			velocity_reduced_x.get_desc().div_up_extent({ 1, VELOCITY_TILE_SIZE, 1 }), 
			"motion_lur.velocity_reduced_y"
		);

		rg::RenderPass::new_compute(
			rg.add_pass("velocity reduce y"),
			"/shaders/motion_blur/velocity_reduce_y.hlsl"
			)
			.read(velocity_reduced_x)
			.write(velocity_reduced_y)
			.dispatch(velocity_reduced_x.desc.extent);
		
		auto velocity_dilated = rg.create<rhi::GpuTexture>(velocity_reduced_y.desc,"motion_lur.velocity_dilated");
		rg::RenderPass::new_compute(
			rg.add_pass("velocity dilate"),
			"/shaders/motion_blur/velocity_dilate.hlsl"
			)
			.read(velocity_reduced_y)
			.write(velocity_dilated)
			.dispatch(velocity_dilated.desc.extent);

		auto output = rg.create<rhi::GpuTexture>(input.desc, "motion_lur.output");
		auto motion_blur_scale = 1.0f;
		rg::RenderPass::new_compute(
			rg.add_pass("velocity dilate"),
			"/shaders/motion_blur/motion_blur.hlsl"
			)
			.read(input)
			.read(reprojection_map)
			.read(velocity_dilated)
			.read_aspect(depth, rhi::ImageAspectFlags::DEPTH)
			.write(output)
			.constants(
				std::tuple{
					depth.desc.extent_inv_extent_2d(),
					output.desc.extent_inv_extent_2d(),
					motion_blur_scale
				}
			)
			.dispatch(output.desc.extent);
		return output;
	}
}