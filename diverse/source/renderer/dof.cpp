#include "drs_rg/simple_pass.h"

namespace diverse
{
	
	auto dof(rg::RenderGraph& rg,
			const rg::Handle<rhi::GpuTexture>& input,
		const rg::Handle<rhi::GpuTexture>& depth) -> rg::Handle<rhi::GpuTexture>
	{
		auto coc = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(PixelFormat::R16_Float, input.desc.extent_2d()));

		auto coc_tiles = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(PixelFormat::R16_Float, coc.desc.div_up_extent({8,8,1}).extent_2d()));

		rg::RenderPass::new_compute(rg.add_pass("coc"), "/shaders/dof/coc.hlsl")
				.read_aspect(depth, rhi::ImageAspectFlags::DEPTH)
				.write(coc)
				.write(coc_tiles)
				.dispatch(coc.desc.extent);
		
		auto dof = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(
			PixelFormat::R16G16B16A16_Float,
			input.desc.extent_2d()
		));

		rg::RenderPass::new_compute(rg.add_pass("dof gather"), "/shaders/dof/gather.hlsl")
			.read_aspect(depth, rhi::ImageAspectFlags::DEPTH)
			.read(input)
			.read(coc)
			.read(coc_tiles)
			.write(dof)
			.constants(dof.desc.extent_inv_extent_2d())
			.dispatch(dof.desc.extent);

		return dof;
	}
}