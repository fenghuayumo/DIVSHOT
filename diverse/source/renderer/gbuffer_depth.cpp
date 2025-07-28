#include "gbuffer_depth.h"
#include <format>

namespace diverse
{
    extern auto  extract_half_res_gbuffer_view_normal_rgba8(rg::RenderGraph& rg,
        const rg::Handle<rhi::GpuTexture>& gbuffer) -> rg::Handle<rhi::GpuTexture>;

    extern auto  extract_half_res_depth(rg::RenderGraph& rg,
        const rg::Handle<rhi::GpuTexture>& depth) -> rg::Handle<rhi::GpuTexture>;

    GbufferDepth::GbufferDepth(rg::Handle<rhi::GpuTexture>&& normal,
                rg::Handle<rhi::GpuTexture>&& gbuff,
                rg::Handle<rhi::GpuTexture>&& depth_tex)
            : geometric_normal(normal),
            gbuffer(gbuff),
            depth(depth_tex)
    {
    }

     auto GbufferDepth::get_half_view_normal(rg::RenderGraph& rg) ->rg::Handle<rhi::GpuTexture>
     {
         if( !half_view_normal )
         {
             half_view_normal = extract_half_res_gbuffer_view_normal_rgba8(rg, gbuffer);

         }
         return half_view_normal.value();
     }
     auto GbufferDepth::get_half_depth(rg::RenderGraph& rg) ->rg::Handle<rhi::GpuTexture>
     {
         if(!half_depth)
         {
             half_depth = extract_half_res_depth(rg, depth);
         }
         return half_depth.value();
     }

    PingPongTemporalResource::PingPongTemporalResource(const std::string& name)
    {
        output_tex = std::format("{}:0", name);
        history_tex = std::format("{}:1", name);
    }

    auto PingPongTemporalResource::get_output_and_history(
        rg::TemporalGraph& rg,
        const rhi::GpuTextureDesc& desc
    )->std::pair<rg::Handle<rhi::GpuTexture>, rg::Handle<rhi::GpuTexture>>
    {
        auto output = rg.get_or_create_temporal(output_tex, desc);
        auto history = rg.get_or_create_temporal(history_tex, desc);

        std::swap(output_tex, history_tex);

        return { output, history };
    }
}