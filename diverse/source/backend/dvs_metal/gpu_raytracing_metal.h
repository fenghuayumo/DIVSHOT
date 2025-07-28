#pragma once

#include "gpu_pipeline_metal.h"
#include "gpu_raytracing.h"
namespace diverse
{
    namespace rhi
    {
        struct RayTracingPipelineMetal:  public GpuPipelineMetal
        {
            
        };

        struct GpuRayTracingAccelerationMetal : public GpuRayTracingAcceleration
        {
       
            // GpuRayTracingAccelerationVulkan(VkAccelerationStructureKHR accel,const std::shared_ptr<GpuBuffer>& buffer)
            // : GpuRayTracingAcceleration(buffer), acc(accel)
            // {}

            // auto as_device_address(const GpuDevice* device) -> u64 override;

        };
    }
}
