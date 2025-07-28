#include "gpu_barrier_vulkan.h"
#include "gpu_device_vulkan.h"
#include "vk_barrier_utils.h"
#include "gpu_texture_vulkan.h"
namespace diverse
{
	namespace rhi
	{
		auto image_layout_2_vk(ImageLayout layout) -> VkImageLayout
		{
			//return (VkImageLayout)(layout);
			switch (layout)
			{
			case diverse::rhi::IMAGE_LAYOUT_UNDEFINED:
				return VK_IMAGE_LAYOUT_UNDEFINED;
			case diverse::rhi::IMAGE_LAYOUT_GENERAL:
				return VK_IMAGE_LAYOUT_GENERAL;
			case diverse::rhi::IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			case diverse::rhi::IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			case diverse::rhi::IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
				return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			case diverse::rhi::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			case diverse::rhi::IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			case diverse::rhi::IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			case diverse::rhi::IMAGE_LAYOUT_PREINITIALIZED:
				return VK_IMAGE_LAYOUT_PREINITIALIZED;
			case diverse::rhi::IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
				return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
			case diverse::rhi::IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
				return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
			case diverse::rhi::IMAGE_LAYOUT_PRESENT_SRC_KHR:
				return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			default:
				return VK_IMAGE_LAYOUT_GENERAL;
			}
		}
		auto get_access_flag(VkImageLayout layout) -> VkAccessFlags
		{
			VkAccessFlags flags = 0;
			switch (layout)
			{
			case VK_IMAGE_LAYOUT_UNDEFINED:
				flags |= VK_ACCESS_SHADER_READ_BIT;
				flags |= VK_ACCESS_SHADER_WRITE_BIT;
				flags |= VK_ACCESS_TRANSFER_READ_BIT;
				flags |= VK_ACCESS_TRANSFER_WRITE_BIT;
				flags |= VK_ACCESS_MEMORY_READ_BIT;
				flags |= VK_ACCESS_MEMORY_WRITE_BIT;
				flags |= VK_ACCESS_HOST_WRITE_BIT;
				flags |= VK_ACCESS_HOST_READ_BIT;
				break;
			case VK_IMAGE_LAYOUT_GENERAL:
				flags |= VK_ACCESS_SHADER_READ_BIT;
				flags |= VK_ACCESS_SHADER_WRITE_BIT;
				flags |= VK_ACCESS_TRANSFER_READ_BIT;
				flags |= VK_ACCESS_TRANSFER_WRITE_BIT;
				flags |= VK_ACCESS_MEMORY_READ_BIT;
				flags |= VK_ACCESS_MEMORY_WRITE_BIT;
				break;
			case VK_IMAGE_LAYOUT_PREINITIALIZED:
				// Image is preinitialized
				// Only valid as initial layout for linear images, preserves memory contents
				// Make sure host writes have been finished
				return VK_ACCESS_HOST_WRITE_BIT;
			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				// Image is a color attachment
				// Make sure any writes to the color buffer have been finished
				return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				// Image is a depth/stencil attachment
				// Make sure any writes to the depth/stencil buffer have been finished
				return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				// Image is a transfer source
				// Make sure any reads from the image have been finished
				return VK_ACCESS_TRANSFER_READ_BIT;
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				// Image is a transfer destination
				// Make sure any writes to the image have been finished
				return VK_ACCESS_TRANSFER_WRITE_BIT;
			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				// Image is read by a shader
				// Make sure any shader reads from the image have been finished
				return VK_ACCESS_SHADER_READ_BIT;

			}
			return flags;
		}


		auto record_image_barrier(GpuDeviceVulkan& device, VkCommandBuffer cb, ImageBarrier barrier) -> void
		{
			record_image_barrier(
				device, cb, barrier, device.universe_queue.family.index, device.universe_queue.family.index
			);
		}

		auto record_image_barrier(GpuDeviceVulkan& device, VkCommandBuffer cb, ImageBarrier barrier,uint src_fam_index,uint dst_fam_index) -> void
		{
			VkImageSubresourceRange	range = {};
			range.aspectMask = image_aspect_flag_2_vk(barrier.aspect_mask);
			range.baseMipLevel = 0;
			range.baseArrayLayer = 0;
			range.levelCount = VK_REMAINING_MIP_LEVELS;
			range.layerCount = VK_REMAINING_ARRAY_LAYERS;

			auto image = static_cast<rhi::GpuTextureVulkan*>(barrier.image);
			vk::pipeline_barrier(
				device.device,
				cb,
				std::optional<vk::GlobalBarrier>(),
				{},
				{
					vk::ImageBarrier{
						{barrier.prev_access},
						{barrier.next_access},
						vk::ImageLayout::Optimal,
						vk::ImageLayout::Optimal,
						barrier.discard,
						src_fam_index,
						dst_fam_index,
						image->image,
						range
					}
				}
			);
		}
	}
}