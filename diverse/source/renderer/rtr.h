#pragma once
#include "drs_rg/simple_pass.h"
#include "gbuffer_depth.h"
#include "irache.h"
#include "restir_gi.h"
namespace diverse
{
    namespace radiance_cache
    {
        struct RadianceCacheState;
    }
    struct TracedRtr
    {
        rg::Handle<rhi::GpuTexture> resolved_tex;
        rg::Handle<rhi::GpuTexture> temporal_output_tex;    
        rg::Handle<rhi::GpuTexture> history_tex;    
        rg::Handle<rhi::GpuTexture> ray_len_tex;    
        rg::Handle<rhi::GpuTexture> refl_restir_invalidity_tex;    

        auto filter_temporal(rg::TemporalGraph& rg,
                const GbufferDepth& gbuffer_depth,
                const rg::Handle<rhi::GpuTexture>& reprojection_map)->rg::Handle<rhi::GpuTexture>;
    };

    struct RtrRenderer
    {
    protected:
        PingPongTemporalResource   temporal_tex;
        PingPongTemporalResource   ray_len_tex;     
        
        PingPongTemporalResource   temporal_irradiance_tex;
        PingPongTemporalResource   temporal_ray_orig_tex;     
        PingPongTemporalResource   temporal_ray_tex;
        PingPongTemporalResource   temporal_reservoir_tex;     
        PingPongTemporalResource   temporal_rng_tex;
        PingPongTemporalResource   temporal_hit_normal_tex;         


        std::shared_ptr<rhi::GpuBuffer> ranking_tile_buf;
        std::shared_ptr<rhi::GpuBuffer> scambling_tile_buf;
        std::shared_ptr<rhi::GpuBuffer> sobol_buf;
     public:
        bool reuse_rtdgi_rays;


        RtrRenderer(struct rhi::GpuDevice* device);

        auto temporal_tex_desc(const std::array<u32,2>& extent)->rhi::GpuTextureDesc
        {
            return rhi::GpuTextureDesc::new_2d(PixelFormat::R16G16B16A16_Float, extent)
                .with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::STORAGE);
        }
        auto trace(rg::TemporalGraph& rg, 
            GbufferDepth& gbuffer_depth,
            const rg::Handle<rhi::GpuTexture>& reprojection_map,
            const rg::Handle<rhi::GpuTexture>& sky_cube,
            rhi::DescriptorSet* bindelss_descriptor_set,
            rg::Handle<rhi::GpuRayTracingAcceleration>& tlas,
            const rg::Handle<rhi::GpuTexture>& rtdgi_irradiance,
            RestirCandidates& restirgi_candidates,
            IracheState& ircache,
            const radiance_cache::RadianceCacheState& rc_state)->TracedRtr;

        auto create_dummy_output(rg::TemporalGraph& rg,
                    const GbufferDepth& gbuffer_depth)->TracedRtr;
    };

}