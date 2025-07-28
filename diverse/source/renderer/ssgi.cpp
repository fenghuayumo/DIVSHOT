#include "ssgi.h"

namespace diverse
{
    auto SsgiRenderer::filter_ssgi(rg::TemporalGraph& rg, 
                rg::Handle<rhi::GpuTexture>& input,
                GbufferDepth& gbuffer_depth,
                const rg::Handle<rhi::GpuTexture>& reprojection_map,
                PingPongTemporalResource& temporal_tex) -> rg::Handle<rhi::GpuTexture>
    {
        auto gbuffer_desc = gbuffer_depth.gbuffer.desc;
        auto half_view_normal_tex = gbuffer_depth.get_half_view_normal(rg);
        auto half_depth_tex = gbuffer_depth.get_half_depth(rg);

        auto spatially_filtered_tex = rg.create<rhi::GpuTexture>(
            gbuffer_desc
            .half_res()
            .with_format(PixelFormat::R16_Float),
            "ssgi_spatial_filter"
        );

        rg::RenderPass::new_compute(
            rg.add_pass("ssgi spatial"),
            "/shaders/ssgi/spatial_filter.hlsl"
            )
            .read(input)
            .read(half_depth_tex)
            .read(half_view_normal_tex)
            .write(spatially_filtered_tex)
            .dispatch(spatially_filtered_tex.desc.extent);
        auto upsampled_tex = upsample_ssgi(rg,spatially_filtered_tex,gbuffer_depth.depth, gbuffer_depth.gbuffer);

        auto [history_output_tex, history_tex] = temporal_tex
            .get_output_and_history(rg, temporal_tex_desc(gbuffer_depth.gbuffer.get_desc().extent_2d()));

        auto filtered_output_tex = rg.create<rhi::GpuTexture>(gbuffer_depth.gbuffer.get_desc().with_format(PixelFormat::R8_UNorm), "ssgi_temporal_filter");
        rg::RenderPass::new_compute(
            rg.add_pass("ssgi temporal"),
            "/shaders/ssgi/temporal_filter.hlsl"
            )
            .read(upsampled_tex)
            .read(history_tex)
            .read(reprojection_map)
            .write(filtered_output_tex)
            .write(history_output_tex)
            .constants(history_output_tex.desc.extent_inv_extent_2d())
            .dispatch(history_output_tex.desc.extent);
        
        return filtered_output_tex;
    }

    auto SsgiRenderer::upsample_ssgi(rg::TemporalGraph& rg,
                    rg::Handle<rhi::GpuTexture>& ssgi,
                    const rg::Handle<rhi::GpuTexture>& depth,
                    const rg::Handle<rhi::GpuTexture>& gbuffer) -> rg::Handle<rhi::GpuTexture>
    {
        auto desc = gbuffer.desc;
        auto output_tex = rg.create<rhi::GpuTexture>(desc.with_format(PixelFormat::R16_Float),"ssgi_upsampled");


        rg::RenderPass::new_compute(
            rg.add_pass("ssgi upsample"),
            "/shaders/ssgi/upsample.hlsl"
            )
            .read(ssgi)
            .read_aspect(depth, rhi::ImageAspectFlags::DEPTH)
            .read(gbuffer)
            .write(output_tex)
            .dispatch(output_tex.desc.extent);
        return output_tex;
    }

    SsgiRenderer::SsgiRenderer()
    {
        ssgi_tex = PingPongTemporalResource("ssgi");
    }

    auto SsgiRenderer::render(rg::TemporalGraph& rg,
            GbufferDepth& gbuffer_depth,
            const rg::Handle<rhi::GpuTexture>& reprojection_map,
            const rg::Handle<rhi::GpuTexture>& prev_radiance,
            rhi::DescriptorSet*	bindless_descriptor_set) -> rg::Handle<rhi::GpuTexture>
    {
        auto gbuffer_desc = gbuffer_depth.gbuffer.desc;
        auto half_view_normal_tex = gbuffer_depth.get_half_view_normal(rg);
        auto half_depth_tex = gbuffer_depth.get_half_depth(rg);

        auto ssgi_tex = rg.create<rhi::GpuTexture>(gbuffer_desc
                .with_usage(rhi::TextureUsageFlags(0))
                .half_res()
                .with_format(PixelFormat::R16_Float),
                "ssgi_half"
                );

        rg::RenderPass::new_compute(rg.add_pass("ssgi"), "/shaders/ssgi/ssgi.hlsl")
            .read(gbuffer_depth.gbuffer)
            .read(half_depth_tex)
            .read(half_view_normal_tex)
            .read(prev_radiance)
            .read(reprojection_map)
            .write(ssgi_tex)
            .constants(std::tuple{
                gbuffer_desc.extent_inv_extent_2d(),
                ssgi_tex.desc.extent_inv_extent_2d(),
                })
            .raw_descriptor_set(1, bindless_descriptor_set)
            .dispatch(ssgi_tex.desc.extent);

        return filter_ssgi(
            rg,
            ssgi_tex, 
            gbuffer_depth,
            reprojection_map,
            this->ssgi_tex);
    }
}