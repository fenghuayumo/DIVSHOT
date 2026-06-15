#ifdef DS_RENDER_API_VULKAN
#include "../drs_vulkan_rhi/gpu_device_vulkan.h"
#endif
#include <mutex>
#include <utility>

namespace diverse
{
    namespace
    {
        std::mutex g_device_mutex;
    }

    rhi::GpuDevice*  g_device;

    RenderAPI   g_render_api;
    namespace rhi
    { 
#ifdef DS_RENDER_API_METAL
   extern auto create_metal_device(u32 device_index)->rhi::GpuDevice*;
#endif
        auto create_device(u32 device_index, RenderAPI api) ->GpuDevice*
        {
            {
                std::lock_guard<std::mutex> lock(g_device_mutex);
                g_render_api = api;
                if (g_device)
                    return g_device;
            }

            GpuDevice* created_device = nullptr;
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
                created_device = new GpuDeviceVulkan(device_index);
            }break;
#endif
#ifdef DS_RENDER_API_METAL
            case RenderAPI::METAL:
            {
                created_device = create_metal_device(device_index);
            }
#endif
            default:
                break;
            }
            std::lock_guard<std::mutex> lock(g_device_mutex);
            if (!g_device)
                g_device = created_device;
            else
                delete created_device;
            return g_device;
        }
    }
    void set_render_api(RenderAPI api)
    {
        std::lock_guard<std::mutex> lock(g_device_mutex);
        g_render_api = api;
    }

    RenderAPI get_render_api()
    {
        std::lock_guard<std::mutex> lock(g_device_mutex);
        return g_render_api;
    }

    auto get_global_device()->rhi::GpuDevice*
    {
        std::lock_guard<std::mutex> lock(g_device_mutex);
        if(g_device) return g_device;
        return  nullptr;
    }

    auto destroy_device()->void
    {
        rhi::GpuDevice* device = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_device_mutex);
            device = std::exchange(g_device, nullptr);
        }
        delete device;
    }
}
