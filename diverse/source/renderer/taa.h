#pragma once
#include "gbuffer_depth.h"
#include <glm/vec2.hpp>
namespace diverse
{
    struct TaaOutput
    {
        rg::Handle<rhi::GpuTexture> temporal_out;
        rg::Handle<rhi::GpuTexture> this_frame_out;
    };

    struct TaaRenderer
    {
    public:
        //TaaRenderer();
        auto render(rg::TemporalGraph& rg,
                    const rg::Handle<rhi::GpuTexture>& input_tex,
                    const rg::Handle<rhi::GpuTexture>& reprojection_map,
                    const rg::Handle<rhi::GpuTexture>& depth_tex,
                    const std::array<u32,2>& output_extent)->TaaOutput;

        auto accumulate_blend(
            rg::TemporalGraph& rg,
            const rg::Handle<rhi::GpuTexture>& inoutput) -> rg::Handle<rhi::GpuTexture>;

        auto temporal_tex_desc(const std::array<u32, 2>& extent) -> rhi::GpuTextureDesc 
        {
           return rhi::GpuTextureDesc::new_2d(PixelFormat::R16G16B16A16_Float, extent)
                .with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::STORAGE |
                            rhi::TextureUsageFlags::TRANSFER_DST | rhi::TextureUsageFlags::TRANSFER_SRC);
        }

        glm::vec2 current_supersample_offset = glm::vec2(0);
    protected:
        PingPongTemporalResource    temporal_tex = PingPongTemporalResource("taa");
        PingPongTemporalResource    temporal_velocity_tex = PingPongTemporalResource("taa_velocity");
        PingPongTemporalResource    temporal_smooth_var_tex = PingPongTemporalResource("taa_smooth_var");
        PingPongTemporalResource    accumulate_tex = PingPongTemporalResource("taa_accumulate");
    };

}