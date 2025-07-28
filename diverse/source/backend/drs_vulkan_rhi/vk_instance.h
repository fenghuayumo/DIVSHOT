#pragma once
#include "vk_utils.h"
namespace diverse
{
    namespace rhi
    {
        struct DeviceBuilder 
        {
            bool							graphics_debugging = true;
        };

        struct GpuInstanceVulkan
        {
            GpuInstanceVulkan(const DeviceBuilder& builder);
            GpuInstanceVulkan() = default;
            GpuInstanceVulkan(VkInstance inst) : instance(inst){}
            VkInstance	instance = {};
            //VkDebugUtilsMessengerEXT debugUtilsMessenger;    

            static auto create(DeviceBuilder& builder) -> GpuInstanceVulkan;
        };
    }
}
