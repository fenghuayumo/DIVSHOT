#pragma once
#include "drs_rg/simple_pass.h"
#include "drs_rg/temporal.h"
#include "gbuffer_depth.h"

namespace diverse
{
	struct ShadowDenoiser
	{
	protected:
		PingPongTemporalResource	accum = PingPongTemporalResource("shadow_denoise_accum");
		PingPongTemporalResource	moments = PingPongTemporalResource("shadow_denoise_moments");
		//ShadowDenoiser();

		auto filter_spatial(
			rg::TemporalGraph& rg,
			u32 step_size,
			const rg::Handle<rhi::GpuTexture>& input_image,
			rg::Handle<rhi::GpuTexture>& output_image,
			const rg::Handle<rhi::GpuTexture>& metadata_image,
			const GbufferDepth& gbuffer_depth,
			const std::array<u32, 2> bitpacked_shadow_mask_extent
		);
	public:
		auto render(rg::TemporalGraph& rg,
			GbufferDepth& gbuffer_depth,
			const rg::Handle<rhi::GpuTexture>& shadow_mask,
			const rg::Handle<rhi::GpuTexture>& reprojection_map) -> rg::Handle<rhi::GpuTexture>;
	};
}