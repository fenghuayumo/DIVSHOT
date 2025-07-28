#pragma once
#include "backend/drs_rhi/gpu_barrier.h"
#include "vk_utils.h"

namespace diverse
{
    namespace rhi
    {
        //struct GpuBarrierVulkan : public GpuBarrier
        //{

        //};

        auto image_layout_2_vk(ImageLayout layout)-> VkImageLayout;
        auto get_access_flag(VkImageLayout layout) -> VkAccessFlags;
        auto record_image_barrier(struct GpuDeviceVulkan& device, VkCommandBuffer	cb, ImageBarrier barrier) -> void;
        auto record_image_barrier(struct GpuDeviceVulkan& device, VkCommandBuffer	cb, ImageBarrier barrier, uint src_fam_index, uint dst_fam_index) -> void;
    }
}