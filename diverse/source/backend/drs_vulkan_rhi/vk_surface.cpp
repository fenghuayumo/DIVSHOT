#include "core/ds_log.h"
#include "vk_surface.h"
#include "engine/window.h"
namespace diverse
{
    extern VkSurfaceKHR    create_platform_surface(VkInstance vkInstance, Window* window);
    
    namespace rhi
    {
        SurfaceVulkan::SurfaceVulkan(const GpuInstanceVulkan& ints, void* window)
            : instance(ints)
        {
            surface = create_platform_surface(instance.instance, (Window*)window);
        }

        SurfaceVulkan::~SurfaceVulkan()
        {
            if (surface != nullptr)
            {
                vkDestroySurfaceKHR(instance.instance, surface, nullptr);
                surface = nullptr;
            }
        }
    }
}
