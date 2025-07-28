#pragma once
#include "drs_rg/simple_pass.h"
#include "gbuffer_depth.h"
#include "irache.h"

namespace diverse
{
    namespace radiance_cache
    {
        struct RadianceCacheState;
    }
    struct RestirCandidates
    {
        rg::Handle<rhi::GpuTexture> candidate_radiance_tex;
        rg::Handle<rhi::GpuTexture> candidate_normal_tex;
        rg::Handle<rhi::GpuTexture> candidate_hit_tex;
    };

    struct ReprjoectedRestirGi
    {
        rg::Handle<rhi::GpuTexture> reprojected_history_tex;
        rg::Handle<rhi::GpuTexture> temporal_output_tex;
    };

    struct RestirGiOutput
    {
        rg::Handle<rhi::GpuTexture> screen_irradiance_tex;
        RestirCandidates candidates;
    };

    struct RestirGiRenderer
    {
    public:   
        RestirGiRenderer();

        auto temporal_tex_desc(const std::array<u32,2>& extent) -> rhi::GpuTextureDesc
        {
            return rhi::GpuTextureDesc::new_2d(PixelFormat::R16G16B16A16_Float, extent)
                .with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::STORAGE);
        }

        auto temporal(rg::TemporalGraph& rg,
            const rg::Handle<rhi::GpuTexture>& input_clor,
            const GbufferDepth& gbuffer_depth,
            const rg::Handle<rhi::GpuTexture>& reprojection_map,
            const rg::Handle<rhi::GpuTexture>& reprojected_history_tex,
            const rg::Handle<rhi::GpuTexture>& rt_history_invalidity_tex,
            rg::Handle<rhi::GpuTexture>& temporal_output_tex) -> rg::Handle<rhi::GpuTexture>;

        auto spatial(rg::TemporalGraph& rg,
            const rg::Handle<rhi::GpuTexture>& input_clor,
            const GbufferDepth& gbuffer_depth,
            const rg::Handle<rhi::GpuTexture>& ssao_tex,
            rhi::DescriptorSet*  bindless_descriptor_set)->rg::Handle<rhi::GpuTexture>;

        auto reproject(rg::TemporalGraph& rg, 
             const rg::Handle<rhi::GpuTexture>& reprojection_map)-> ReprjoectedRestirGi;

        auto render(rg::TemporalGraph& rg,
            ReprjoectedRestirGi&& reprojected,
            GbufferDepth& gbuffer_depth,
            const rg::Handle<rhi::GpuTexture>& reprojection_map,
            const rg::Handle<rhi::GpuTexture>& sky_cube,
            rhi::DescriptorSet*  bindless_descriptor_set,
            IracheState& surfel,
            const radiance_cache::RadianceCacheState& rc_state,
            rg::Handle<rhi::GpuRayTracingAcceleration>& tals,
            rg::Handle<rhi::GpuTexture>& ssao_tex
            )-> RestirGiOutput;
    protected:
        PingPongTemporalResource temporal_radiance_tex = PingPongTemporalResource("restirgi.radiance");
        PingPongTemporalResource temporal_ray_orig_tex = PingPongTemporalResource("restirgi.ray_orig");
        PingPongTemporalResource temporal_ray_tex = PingPongTemporalResource("restirgi.ray");
        PingPongTemporalResource temporal_reservoir_tex = PingPongTemporalResource("restirgi.reservoir");
        PingPongTemporalResource temporal_candidate_tex = PingPongTemporalResource("restirgi.candidate");
        PingPongTemporalResource temporal_invalidity_tex = PingPongTemporalResource("restirgi.invalidity");
        PingPongTemporalResource temporal2_tex = PingPongTemporalResource("restirgi.temporal2");
        PingPongTemporalResource temporal2_variance_tex = PingPongTemporalResource("restirgi.temporal2_var");
        PingPongTemporalResource temporal_hit_normal_tex = PingPongTemporalResource("restirgi.hit_normal");
    public:
        u32 spatial_reuse_pass_count = 2;
        bool use_raytraced_reservoir_visibility = true;
        bool use_trace_eval = false;
    };
}