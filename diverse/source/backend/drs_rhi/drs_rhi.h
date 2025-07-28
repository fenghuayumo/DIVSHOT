#pragma once
#include "core/base_type.h"
#include <memory>
namespace diverse
{
    enum class RenderAPI : u32
    {
        OPENGL = 0,
        VULKAN,
        DIRECT3D, // Unsupported
        METAL,    // Unsupported
        NONE,     // Unsupported
    };

    void        set_render_api(RenderAPI    api);
    RenderAPI   get_render_api();
    
    namespace  rhi
    {
        struct GpuDevice;
        auto create_device(u32 device_index, RenderAPI api) ->GpuDevice*;
    }

    auto get_global_device()->rhi::GpuDevice*;
    auto destroy_device()->void;
    extern rhi::GpuDevice* g_device;
}
