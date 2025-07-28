#pragma once

#include "backend/drs_rhi/gpu_texture.h"
#include "drs_rg/temporal.h"
#include "drs_rg/simple_pass.h"
#include <glm/vec4.hpp>
#include <optional>
namespace diverse
{
    namespace sky
    { 
        auto render_sky_cube(rg::RenderGraph& rg,const glm::vec4& color = glm::vec4(1,1,1,1),int cubeResolution = 512)->rg::Handle<rhi::GpuTexture>;
        auto build_sky_env_map(rg::RenderGraph& rg,
            const rg::Handle<rhi::GpuTexture>& input,
            int envmap_width = 2048)->std::tuple<rg::Handle<rhi::GpuTexture>,rg::Handle<rhi::GpuTexture>>;
            
        auto convolve_cube(rg::RenderGraph& rg,
            const rg::Handle<rhi::GpuTexture>& input)->rg::Handle<rhi::GpuTexture>;
        auto render_sky(rg::RenderGraph& rg,
            const rg::Handle<rhi::GpuTexture>& convolved_sky_cube,
            rg::Handle<rhi::GpuTexture>& color_img,
            rhi::DescriptorSet* bindless_descriptor_set) -> rg::Handle<rhi::GpuTexture>;
    }
    struct IblRenderParameter
    {
        f32 theta = 0.0f;
        f32 phi = 0.0f;
    };
    struct IblRenderer
    {
        auto render(rg::TemporalGraph& rg,const std::shared_ptr<rhi::GpuTexture>&  texture,const IblRenderParameter& param) -> std::optional<rg::Handle<rhi::GpuTexture>>;
    };
}