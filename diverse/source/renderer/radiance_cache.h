#pragma once
#include "drs_rg/simple_pass.h"
#include "drs_rg/temporal.h"
#include "irache.h"

namespace diverse
{
    namespace radiance_cache
    { 
        struct RadianceCacheState
        {

            template<typename RgPipelineHandle>
            auto bind(rg::SimpleRenderPass<RgPipelineHandle>& pass) const -> rg::SimpleRenderPass<RgPipelineHandle>&
            {
                return pass.read(radiance_atlas);
            }
            template<typename RgPipelineHandle>
            auto bind(rg::SimpleRenderPass<RgPipelineHandle>& pass) -> rg::SimpleRenderPass<RgPipelineHandle>&
            {
                return pass.read(radiance_atlas);
            }
            auto see_through(
                rg::TemporalGraph& rg,
                IracheState& surfel,
                const rg::Handle<rhi::GpuTexture>& sky_cube,
                rhi::DescriptorSet* bindless_descriptor,
                const rg::Handle<rhi::GpuRayTracingAcceleration>& tlas,
                rg::Handle<rhi::GpuTexture>& output
            )->void;

            rg::Handle<rhi::GpuTexture> radiance_atlas;
        };

        auto radiance_cache_trace(
             rg::TemporalGraph& rg,
             IracheState& surfel,
             const rg::Handle<rhi::GpuTexture>& sky_cube,
             rhi::DescriptorSet* bindless_descriptor,
             const rg::Handle<rhi::GpuRayTracingAcceleration>& tlas
            )-> RadianceCacheState;

        inline auto allocate_dummy_ouput(rg::TemporalGraph& rg) -> RadianceCacheState {
            return RadianceCacheState{
                rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(PixelFormat::R8_UNorm,{1,1}), "radiance_altas_dummy")
            };
        }
    }
}