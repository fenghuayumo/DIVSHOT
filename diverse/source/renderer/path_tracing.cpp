#include "drs_rg/temporal.h"
#include "drs_rg/simple_pass.h"
#include <optional>
#include "assets/texture.h"
#include "drs_rg/image_op.h"

namespace diverse
{
    auto render_path_tracing(rg::TemporalGraph& rg,
		rg::Handle<rhi::GpuTexture>& output_image,
		rg::Handle<rhi::GpuTexture>& depth_image,
		rg::Handle<rhi::GpuTexture>& sky_cube,
		rhi::DescriptorSet* bindless_descriptor_set,
		const std::optional<rg::Handle<rhi::GpuRayTracingAcceleration>>& tlas) -> void
	{		
		if(!tlas) return;
		auto pt_depth_img = rg.create<rhi::GpuTexture>(
						output_image.desc
						.with_format(PixelFormat::R32_Float),
						"pt.depth");
		
		rg::RenderPass::new_rt(
			rg.add_pass("reference pt"),
			rhi::ShaderSource{ "/shaders/rt/reference_path_trace.rgen.hlsl" },
			{
				rhi::ShaderSource{ "/shaders/rt/gbuffer.rmiss.hlsl" },
				rhi::ShaderSource{ "/shaders/rt/shadow.rmiss.hlsl" }
			},
			{
				rhi::ShaderSource{ "/shaders/rt/gbuffer.rchit.hlsl" }
			}
		)
		.write(output_image)
		.write(pt_depth_img)
		.read(sky_cube)
		.raw_descriptor_set(1, bindless_descriptor_set)
		.trace_rays(tlas.value(), output_image.desc.extent);

		// rg::copy_img(rg, pt_depth_img, depth_image);
	}
}