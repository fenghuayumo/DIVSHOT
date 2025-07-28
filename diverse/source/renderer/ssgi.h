#include "drs_rg/simple_pass.h"
#include "gbuffer_depth.h"
namespace diverse
{
	struct SsgiRenderer
	{
	protected:
		PingPongTemporalResource	ssgi_tex = PingPongTemporalResource("ssgi");

		auto filter_ssgi(rg::TemporalGraph& rg, 
					rg::Handle<rhi::GpuTexture>& input,
					GbufferDepth& gbuffer_depth,
					const rg::Handle<rhi::GpuTexture>& reprojection_map,
					PingPongTemporalResource& temporal_tex) -> rg::Handle<rhi::GpuTexture>;

		auto upsample_ssgi(rg::TemporalGraph& rg,
						rg::Handle<rhi::GpuTexture>& ssgi,
						const rg::Handle<rhi::GpuTexture>& depth,
						const rg::Handle<rhi::GpuTexture>& gbuffer) -> rg::Handle<rhi::GpuTexture>;

		auto temporal_tex_desc(const std::array<u32,2>& extent) -> rhi::GpuTextureDesc
		{
			return rhi::GpuTextureDesc::new_2d(PixelFormat::R16_Float, extent)
					.with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::STORAGE);
		}
	public:
		SsgiRenderer();
		auto render(rg::TemporalGraph& rg,
				GbufferDepth& gbuffer_depth,
				const rg::Handle<rhi::GpuTexture>& reprojection_map,
				const rg::Handle<rhi::GpuTexture>& prev_radiance,
				rhi::DescriptorSet*	bindless_descriptor_set) -> rg::Handle<rhi::GpuTexture>;
	};
}