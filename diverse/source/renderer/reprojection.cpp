#include "renderer/drs_rg/simple_pass.h"
#include "gbuffer_depth.h"

namespace diverse
{
    auto calculate_reprojection_map(
        rg::TemporalGraph& rg,
        const GbufferDepth& gbuffer_depth,
        const rg::Handle<rhi::GpuTexture>& velocity_img) -> rg::Handle<rhi::GpuTexture>
    {
        auto desc = gbuffer_depth.geometric_normal.desc;
        auto output_tex = rg.create<rhi::GpuTexture>(
            desc
            .with_format(PixelFormat::R16G16B16A16_SNorm),
            "reprojection_map"
            );
        auto prev_depth = rg.get_or_create_temporal(
                           "reprojection.prev_depth",
                             desc
                             .with_format(PixelFormat::R32_Float)
                             .with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::STORAGE));

        rg::RenderPass::new_compute(
            rg.add_pass("reprojection map"),
            "/shaders/calculate_reprojection_map.hlsl"
            )
            .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
            .read(gbuffer_depth.geometric_normal)
            .read(prev_depth)
            .read(velocity_img)
            .write(output_tex)
            .constants(output_tex.desc.extent_inv_extent_2d())
            .dispatch(output_tex.desc.extent);

        rg::RenderPass::new_compute(
            rg.add_pass("copy depth"),
            "/shaders/copy_depth_to_r.hlsl"
            )
            .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
            .write(prev_depth)
            .dispatch(prev_depth.desc.extent);


        return output_tex;
    }
}