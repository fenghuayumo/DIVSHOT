#pragma once
#include <renderer/drs_rg/simple_pass.h>

namespace diverse
{
    auto  extract_half_res_gbuffer_view_normal_rgba8(rg::RenderGraph& rg,
        const rg::Handle<rhi::GpuTexture>& gbuffer)->rg::Handle<rhi::GpuTexture>
    {
        auto desc = gbuffer.desc;
        auto output_tex = rg.create<rhi::GpuTexture>(
             desc
            .half_res()
            .with_format(PixelFormat::R8G8B8A8_SNorm),
            "half_view_normal"
            );
        rg::RenderPass::new_compute(
            rg.add_pass("extract view normal/2"),
            "/shaders/extract_half_res_gbuffer_view_normal_rgba8.hlsl"
            )
            .read(gbuffer)
            .write(output_tex)
            .dispatch(output_tex.desc.extent);
        return output_tex;
    }

    auto  extract_half_res_depth(rg::RenderGraph& rg,
        const rg::Handle<rhi::GpuTexture>& depth) -> rg::Handle<rhi::GpuTexture>
    {
        auto desc = depth.desc;
        auto output_tex = rg.create<rhi::GpuTexture>(
            desc
            .half_res()
            .with_format(PixelFormat::R32_Float)
            .with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::STORAGE),
            "half_depth"
            );
        rg::RenderPass::new_compute(
            rg.add_pass("extract half depth"),
            "/shaders/extract_half_res_depth.hlsl"
            )
            .read_aspect(depth, rhi::ImageAspectFlags::DEPTH)
            .write(output_tex)
            .dispatch(output_tex.desc.extent);
        return output_tex;
    }
}