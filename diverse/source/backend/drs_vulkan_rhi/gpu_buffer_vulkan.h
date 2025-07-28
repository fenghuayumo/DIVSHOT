#pragma once
#include "backend/drs_rhi/gpu_buffer.h"
#include "vk_utils.h"
namespace diverse
{
	namespace rhi
	{
		struct GpuDeviceVulkan;
		struct GpuBufferViewVulkan : GpuBufferView
		{
			GpuBufferViewVulkan(VkBufferView view) :buf_view(view) {}
			~GpuBufferViewVulkan();
			VkBufferView	buf_view = VK_NULL_HANDLE;
		};
		struct GpuBufferVulkan : public GpuBuffer
		{
			GpuBufferVulkan(){ handle = 0; allocation = nullptr;}
			GpuBufferVulkan(VkBuffer h,const GpuBufferDesc& desc,VmaAllocation  alloc, VmaAllocationInfo info);
			~GpuBufferVulkan();
            VkBuffer	handle = VK_NULL_HANDLE;
			VmaAllocation allocation{};
			VmaAllocationInfo allocate_info = {};
			std::unordered_map<GpuBufferViewDesc, std::shared_ptr<GpuBufferView>>	views;
            uint64 device_address(const struct GpuDevice* device) override;
			auto view(const struct GpuDevice* device, const GpuBufferViewDesc& view_desc) -> std::shared_ptr<GpuBufferView> override;
			auto map(const struct GpuDevice* device) -> u8* override;
			auto unmap(const GpuDevice* device) -> void override;
		};
	}
}
