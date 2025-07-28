#pragma once
#include "vk_utils.h"
#include "vk_instance.h"
namespace diverse
{
    namespace rhi
    {
        struct SurfaceVulkan
        {
            VkSurfaceKHR	surface;
            const GpuInstanceVulkan& instance;
            //VkInstance      instance;
            explicit SurfaceVulkan(const GpuInstanceVulkan& instance, void* window_hanlde);

            ~SurfaceVulkan();
        };
    }
}
