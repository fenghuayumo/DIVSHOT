#include "radiance_cache.h"

namespace diverse
{
	namespace radiance_cache
	{

		const std::array<u32,3> WRC_GRID_DIMS  = {8, 3, 8};
		const u32 WRC_PROBE_DIMS  = 32;
		const std::array<u32,2> WRC_ATLAS_PROBE_COUNT = {16, 16};

		auto RadianceCacheState::see_through(
			rg::TemporalGraph& rg, 
			IracheState& surfel, 
			const rg::Handle<rhi::GpuTexture>& sky_cube,
			rhi::DescriptorSet* bindless_descriptor_set,
			const rg::Handle<rhi::GpuRayTracingAcceleration>& tlas, 
			rg::Handle<rhi::GpuTexture>& output) -> void
		{
			rg::RenderPass::new_rt(
				rg.add_pass("wrc see through"),
				rhi::ShaderSource{"/shaders/wrc/wrc_see_through.rgen.hlsl"},
				{
					rhi::ShaderSource{"/shaders/rt/gbuffer.rmiss.hlsl"},
					rhi::ShaderSource{"/shaders/rt/shadow.rmiss.hlsl"}
				},
				{rhi::ShaderSource{"/shaders/rt/gbuffer.rchit.hlsl"}}
				)
				.bind(*this)
				.read(sky_cube)
				.bind(surfel)
				.write(output)
				.raw_descriptor_set(1, bindless_descriptor_set)
				.trace_rays(tlas, output.desc.extent);
		}

		auto radiance_cache_trace(
			rg::TemporalGraph& rg, 
			IracheState& surfel, 
			const rg::Handle<rhi::GpuTexture>& sky_cube, 
			rhi::DescriptorSet* bindless_descriptor_set,
			const rg::Handle<rhi::GpuRayTracingAcceleration>& tlas)
			-> RadianceCacheState
		{
			const auto total_probe_count = WRC_GRID_DIMS[0] * WRC_GRID_DIMS[1] * WRC_GRID_DIMS[2];
			const auto total_pixel_count = total_probe_count * WRC_PROBE_DIMS * WRC_PROBE_DIMS;

			auto radiance_atlas = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(
				PixelFormat::R32G32B32A32_Float,
				{
					(WRC_ATLAS_PROBE_COUNT[0] * WRC_PROBE_DIMS),
					(WRC_ATLAS_PROBE_COUNT[1] * WRC_PROBE_DIMS),
				}),
				"radiance_atlas"
				);

			rg::RenderPass::new_rt(
				rg.add_pass("wrc trace"),
				rhi::ShaderSource{"/shaders/wrc/trace_wrc.rgen.hlsl"},
				{
					rhi::ShaderSource{"/shaders/rt/gbuffer.rmiss.hlsl"},
					rhi::ShaderSource{"/shaders/rt/shadow.rmiss.hlsl"}
				},
				{rhi::ShaderSource{"/shaders/rt/gbuffer.rchit.hlsl"}}
				)
				.read(sky_cube)
				.bind(surfel)
				.write(radiance_atlas)
				.raw_descriptor_set(1, bindless_descriptor_set)
				.trace_rays(tlas, {total_pixel_count, 1, 1});
			return RadianceCacheState{ radiance_atlas };
		}

	}
}

