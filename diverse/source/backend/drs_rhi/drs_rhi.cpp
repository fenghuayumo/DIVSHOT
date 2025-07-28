#ifdef DS_RENDER_API_VULKAN
#include "../drs_vulkan_rhi/gpu_device_vulkan.h"
#endif

namespace diverse
{
    rhi::GpuDevice*  g_device;

    RenderAPI   g_render_api;
    namespace rhi
    { 
#ifdef DS_RENDER_API_METAL
   extern auto create_metal_device(u32 device_index)->rhi::GpuDevice*;
#endif
        auto create_device(u32 device_index, RenderAPI api) ->GpuDevice*
        {
            set_render_api(api);
            switch (api)
            {
#ifdef DS_RENDER_API_OPENGL
            case RenderAPI::OPENGL:
            {

            }break;
#endif
#ifdef DS_RENDER_API_VULKAN
            case RenderAPI::VULKAN:
            {
                g_device = new GpuDeviceVulkan(device_index);
            }break;
#endif
#ifdef DS_RENDER_API_METAL
            case RenderAPI::METAL:
            {
                g_device = create_metal_device(device_index);
            }
#endif
            default:
                break;
            }
            return g_device;
        }
    }
    void set_render_api(RenderAPI api)
    {
        g_render_api = api;
    }

    RenderAPI get_render_api()
    {
        return g_render_api;
    }

    auto get_global_device()->rhi::GpuDevice*
    {
        if(g_device) return g_device;
        return  nullptr;
    }

    auto destroy_device()->void
    {
        if(g_device)
        {
            delete g_device;
            g_device = nullptr;
        }
    }
}
