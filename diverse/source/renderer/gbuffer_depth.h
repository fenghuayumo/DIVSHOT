#pragma once
#include <renderer/drs_rg/temporal.h>

namespace diverse
{
    struct GbufferDepth
    {

        rg::Handle<rhi::GpuTexture> geometric_normal;
        rg::Handle<rhi::GpuTexture> gbuffer;
        rg::Handle<rhi::GpuTexture> depth;


        GbufferDepth(rg::Handle<rhi::GpuTexture>&& normal,
                    rg::Handle<rhi::GpuTexture>&& gbuffer,
                    rg::Handle<rhi::GpuTexture>&& depth);

        auto get_half_view_normal(rg::RenderGraph& rg) ->rg::Handle<rhi::GpuTexture>;
        auto get_half_depth(rg::RenderGraph& rg)  ->rg::Handle<rhi::GpuTexture>;

    private:
        std::optional<rg::Handle<rhi::GpuTexture>> half_view_normal;
        std::optional<rg::Handle<rhi::GpuTexture>> half_depth;
    };

    struct PingPongTemporalResource
    {
        rg::TemporalResourceKey output_tex;
        rg::TemporalResourceKey history_tex;

        PingPongTemporalResource(const std::string& name = "");

        auto get_output_and_history(
            rg::TemporalGraph& rg,
            const rhi::GpuTextureDesc& desc)->std::pair<rg::Handle<rhi::GpuTexture>, rg::Handle<rhi::GpuTexture>>;
    };
}