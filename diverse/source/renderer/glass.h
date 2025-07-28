#pragma once
#include "drs_rg/simple_pass.h"
#include "drs_rg/temporal.h"
namespace diverse
{
    class GlassPass
    {
    public:

        auto render(rg::RenderGraph& rg)->rg::Handle<rhi::GpuTexture>;
    };
}