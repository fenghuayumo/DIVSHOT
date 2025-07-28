#include <string>
#include "vk_barrier_utils.h"

namespace vk
{
    auto get_image_memory_barrier(ImageBarrier barrier) -> std::tuple<
            VkPipelineStageFlags,
            VkPipelineStageFlags,
            VkImageMemoryBarrier>
    {
		auto src_stages = 0;
		auto dst_stages = 0;

		VkImageMemoryBarrier image_barrier = {};
		image_barrier.srcQueueFamilyIndex = barrier.src_queue_family_index;
		image_barrier.dstQueueFamilyIndex = barrier.dst_queue_family_index;
		image_barrier.image = barrier.image;
		image_barrier.subresourceRange = barrier.range;
		image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;


		for (auto previous_access : barrier.previous_accesses)
		{
			auto previous_info = get_access_info(previous_access);

			src_stages |= previous_info.stage_mask;

			// Add appropriate availability operations - for writes only.
			if (is_write_access(previous_access))
				image_barrier.srcAccessMask |= previous_info.access_mask;

			if (barrier.discard_contents)
				image_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			else
			{
				VkImageLayout	layout;

				switch (barrier.previous_layout)
				{
				case	ImageLayout::General:
				{
					layout = previous_access == diverse::rhi::AccessType::Present ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_GENERAL;
				}
				break;
				case ImageLayout::Optimal:
				{
					layout = previous_info.image_layout;
				}
				break;
				case ImageLayout::GeneralAndPresentation:
				default:
					layout = VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR;
					break;
				}

				image_barrier.oldLayout = layout;
			}
		}

		for (auto next_access : barrier.next_accesses)
		{
			auto next_info = get_access_info(next_access);

			dst_stages |= next_info.stage_mask;

			// Add visibility operations as necessary.
			// If the src access mask, this is a WAR hazard (or for some reason a "RAR"),
			// so the dst access mask can be safely zeroed as these don't need visibility.
			if (image_barrier.srcAccessMask != 0)
			{
				image_barrier.dstAccessMask |= next_info.access_mask;
			}

			VkImageLayout layout;
			switch (barrier.next_layout)
			{

			case	ImageLayout::General:
			{
				layout = next_access == diverse::rhi::AccessType::Present ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_GENERAL;
			}
			break;
			case ImageLayout::Optimal:
			{
				layout = next_info.image_layout;
			}
			break;
			case ImageLayout::GeneralAndPresentation:
			default:
				layout = VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR;
				break;
			}
			image_barrier.newLayout = layout;
		}

		// Ensure that the stage masks are valid if no stages were determined
		if (!src_stages)
		{
			src_stages = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		}

		if (!dst_stages)
			dst_stages = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

		return std::make_tuple(src_stages, dst_stages, image_barrier);
	}


    void pipeline_barrier(VkDevice device, 
		VkCommandBuffer command_buffer,
		std::optional<GlobalBarrier> global_barrier,
		const std::vector<BufferBarrier>& buffer_barriers,
		const std::vector<ImageBarrier>& image_barriers)
    {
		VkFlags src_stage_mask = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkFlags dst_stage_mask = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

		// TODO: Optimize out the Vec heap allocations
		std::vector<VkMemoryBarrier> vk_memory_barriers;
		std::vector<VkBufferMemoryBarrier> vk_buffer_barriers;

		std::vector<VkImageMemoryBarrier> vk_image_barriers;

		// Global memory barrier
		if (global_barrier.has_value()) 
		{
			auto [src_mask, dst_mask, barrier] = get_memory_barrier(global_barrier.value());
			src_stage_mask |= src_mask;
			dst_stage_mask |= dst_mask;
			vk_memory_barriers.push_back(barrier);
		}

		// Buffer memory barriers
		for(auto buffer_barrier : buffer_barriers)
		{
			auto [src_mask, dst_mask, barrier] = get_buffer_memory_barrier(buffer_barrier);
			src_stage_mask |= src_mask;
			dst_stage_mask |= dst_mask;
			vk_buffer_barriers.push_back(barrier);
		}

		// Image memory barriers
		for(auto image_barrier : image_barriers)
		{
			auto [src_mask, dst_mask, barrier] = get_image_memory_barrier(image_barrier);
			src_stage_mask |= src_mask;
			dst_stage_mask |= dst_mask;
			vk_image_barriers.push_back(barrier);
		}

		vkCmdPipelineBarrier(command_buffer,
			src_stage_mask,
			dst_stage_mask,
			0,
			vk_memory_barriers.size(),
			vk_memory_barriers.data(),
			vk_buffer_barriers.size(),
			vk_buffer_barriers.data(),
			vk_image_barriers.size(),
			vk_image_barriers.data());
	}
}