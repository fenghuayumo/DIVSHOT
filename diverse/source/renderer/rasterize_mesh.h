#pragma once
#include "backend/drs_rhi/gpu_texture.h"
#include "drs_rg/simple_pass.h"
#include "gbuffer_depth.h"
#include <glm/vec4.hpp>
#include <optional>
namespace diverse
{
    class RasterizeMesh
    {
    public:
        RasterizeMesh(struct DeferedRenderer* renderer);
        ~RasterizeMesh();

        auto raster_gbuffer(rg::RenderGraph& rg, 
            GbufferDepth& gbuffer_depth,
            rg::Handle<rhi::GpuTexture>& velocity_img)->void;
        
        auto raster_depthprepass(rg::RenderGraph& rg, 
            rg::Handle<rhi::GpuTexture>& color_img,
            rg::Handle<rhi::GpuTexture>& depth_img)->rg::Handle<rhi::GpuTexture>;

        auto raster_wire_frame(rg::RenderGraph& rg,
            rg::Handle<rhi::GpuTexture>& color_img,
            rg::Handle<rhi::GpuTexture>& depth_img)->void;
    private:
        struct DeferedRenderer* renderer;
        
		std::shared_ptr<rhi::RenderPass> raster_render_pass;
        std::shared_ptr<rhi::RenderPass> wire_render_pass;
    };
}