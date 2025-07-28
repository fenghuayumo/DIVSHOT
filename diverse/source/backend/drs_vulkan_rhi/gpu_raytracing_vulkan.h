#pragma once
#include "backend/drs_rhi/gpu_raytracing.h"
#include "gpu_pipeline_vulkan.h"
namespace diverse
{
    namespace rhi
    {

        struct RayTracingShaderTable
        {
            std::shared_ptr<rhi::GpuBuffer>  raygen_shader_binding_table_buffer;
            VkStridedDeviceAddressRegionKHR raygen_shader_binding_table;
            std::shared_ptr<rhi::GpuBuffer>  miss_shader_binding_table_buffer;
            VkStridedDeviceAddressRegionKHR miss_shader_binding_table;
            std::shared_ptr<rhi::GpuBuffer>  hit_shader_binding_table_buffer;
            VkStridedDeviceAddressRegionKHR hit_shader_binding_table;
            std::shared_ptr<rhi::GpuBuffer>  callable_shader_binding_table_buffer;
            VkStridedDeviceAddressRegionKHR callable_shader_binding_table;

        };

        struct RayTracingPipelineVulkan :  public PipelineVulkan
        {
            RayTracingPipelineVulkan(
                VkPipelineLayout layout,
                VkPipeline pip,
                std::vector<std::unordered_map<uint32, VkDescriptorType>>&& layout_info,
                std::vector<VkDescriptorPoolSize>&& pool_sizes,
                std::vector<VkDescriptorSetLayout>&& set_layouts,
                VkPipelineBindPoint bd_point,
                RayTracingShaderTable   stb);

            RayTracingShaderTable   shader_table;
        };

        struct GpuRayTracingAccelerationVulkan : public GpuRayTracingAcceleration
        {
            VkAccelerationStructureKHR  acc = VK_NULL_HANDLE;

            GpuRayTracingAccelerationVulkan(VkAccelerationStructureKHR accel,const std::shared_ptr<GpuBuffer>& buffer)
            : GpuRayTracingAcceleration(buffer), acc(accel)
            {}

            auto as_device_address(const GpuDevice* device) -> u64 override;
        };
    }
}