#include "gpu_raytracing_vulkan.h"
#include "gpu_device_vulkan.h"
namespace diverse
{
    namespace rhi
    {

        RayTracingPipelineVulkan::RayTracingPipelineVulkan(VkPipelineLayout layout,
            VkPipeline pip,
            std::vector<std::unordered_map<uint32, VkDescriptorType>>&& layout_info,
            std::vector<VkDescriptorPoolSize>&& pool_sizes,
            std::vector<VkDescriptorSetLayout>&& set_layouts,
            VkPipelineBindPoint bd_point,
            RayTracingShaderTable stb)
            : PipelineVulkan(layout, pip, std::move(layout_info), std::move(pool_sizes), std::move(set_layouts), bd_point)
            , shader_table(stb)
        {
        }
        auto GpuRayTracingAccelerationVulkan::as_device_address(const GpuDevice* device) -> u64 
        {
            auto vk_device = static_cast<const GpuDeviceVulkan*>(device)->device;
            VkAccelerationStructureDeviceAddressInfoKHR device_info = {};
            device_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
            device_info.accelerationStructure = acc;
            auto blas_address = vkGetAccelerationStructureDeviceAddressKHR(vk_device, &device_info);

            return blas_address;
        }
    }
}