#pragma once
#include <tuple>
#include <vector>
#include <optional>
#include "backend/drs_rhi/gpu_barrier.h"
#include "vk_utils.h"
using BufferType = VkBuffer;
using ImageType = VkImage;
using ImageSubresourceRangeType = VkImageSubresourceRange;

namespace vk
{

	/// Defines a handful of layout options for images.
	/// Rather than a list of all possible image layouts, this reduced list is
	/// correlated with the access types to map to the correct Vulkan layouts.
	/// `Optimal` is usually preferred.
	enum class ImageLayout
	{
		/// Choose the most optimal layout for each usage. Performs layout transitions as appropriate for the access.
		Optimal,

		/// Layout accessible by all Vulkan access types on a device - no layout transitions except for presentation
		General,

		/// Similar to `General`, but also allows presentation engines to access it - no layout transitions.
		/// Requires `VK_KHR_shared_presentable_image` to be enabled, and this can only be used for shared presentable
		/// images (i.e. single-buffered swap chains).
		GeneralAndPresentation,
	};


	/// Global barriers define a set of accesses on multiple resources at once.
	/// If a buffer or image doesn't require a queue ownership transfer, or an image
	/// doesn't require a layout transition (e.g. you're using one of the
	/// `ImageLayout::General*` layouts) then a global barrier should be preferred.
	///
	/// Simply define the previous and next access types of resources affected.

	struct GlobalBarrier
	{
		std::vector<diverse::rhi::AccessType> previous_accesses;
		std::vector<diverse::rhi::AccessType> next_accesses;
	};

	/// Buffer barriers should only be used when a queue family ownership transfer
	/// is required - prefer global barriers at all other times.
	///
	/// Access types are defined in the same way as for a global memory barrier, but
	/// they only affect the buffer range identified by `buffer`, `offset` and `size`,
	/// rather than all resources.
	///
	/// `src_queue_family_index` and `dst_queue_family_index` will be passed unmodified
	/// into a buffer memory barrier.
	///
	/// A buffer barrier defining a queue ownership transfer needs to be executed
	/// twice - once by a queue in the source queue family, and then once again by a
	/// queue in the destination queue family, with a semaphore guaranteeing
	/// execution order between them.

	struct BufferBarrier
	{
		std::vector<diverse::rhi::AccessType> previous_accesses;
		std::vector<diverse::rhi::AccessType> next_accesses;
		unsigned int src_queue_family_index;
		unsigned int dst_queue_family_index;
		BufferType buffer;
		unsigned int offset;
		unsigned int size;
	};


	struct AccessInfo
	{
		VkPipelineStageFlags stage_mask;
		VkAccessFlags access_mask;
		VkImageLayout image_layout;
	};

	auto inline get_access_info(diverse::rhi::AccessType access_type) -> AccessInfo
	{
		switch (access_type)
		{
		case	diverse::rhi::AccessType::Nothing:  return AccessInfo{
				0,
				0,
				VK_IMAGE_LAYOUT_UNDEFINED,
		};
		case diverse::rhi::AccessType::CommandBufferReadNVX:  return AccessInfo{
				VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV,
				VkAccessFlagBits::VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_NV,
				VK_IMAGE_LAYOUT_UNDEFINED
		};
		case	diverse::rhi::AccessType::IndirectBuffer:  return AccessInfo{
				VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
				VkAccessFlagBits::VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
		};
		case	diverse::rhi::AccessType::IndexBuffer:  return AccessInfo{
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
				VkAccessFlagBits::VK_ACCESS_INDEX_READ_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
		};
		case	diverse::rhi::AccessType::VertexBuffer:  return AccessInfo{
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
				VkAccessFlagBits::VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
		};
		case	diverse::rhi::AccessType::VertexShaderReadUniformBuffer:  return AccessInfo{
				VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
				VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
		};
		case	diverse::rhi::AccessType::VertexShaderReadSampledImageOrUniformTexelBuffer:  return AccessInfo{
				VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
				VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};
		case	diverse::rhi::AccessType::VertexShaderReadOther:  return AccessInfo{
				VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
				VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_GENERAL,
		};
		case	diverse::rhi::AccessType::TessellationControlShaderReadUniformBuffer:  return AccessInfo{
				VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
				VkAccessFlagBits::VK_ACCESS_UNIFORM_READ_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
		};
		case	diverse::rhi::AccessType::TessellationControlShaderReadSampledImageOrUniformTexelBuffer:  return AccessInfo{
				VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
				VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		case	diverse::rhi::AccessType::TessellationControlShaderReadOther:  return AccessInfo{
				VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
				VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_GENERAL
		};
		case	diverse::rhi::AccessType::TessellationEvaluationShaderReadUniformBuffer:  return AccessInfo{
				VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
				VkAccessFlagBits::VK_ACCESS_UNIFORM_READ_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
		};
		case	diverse::rhi::AccessType::TessellationEvaluationShaderReadSampledImageOrUniformTexelBuffer:  return {
				AccessInfo {
					VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
					VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				}
		};
		case	diverse::rhi::AccessType::TessellationEvaluationShaderReadOther:  return AccessInfo{
				VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
				VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_GENERAL,
		};
		case	diverse::rhi::AccessType::GeometryShaderReadUniformBuffer:  return AccessInfo{
				VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT,
				VkAccessFlagBits::VK_ACCESS_UNIFORM_READ_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
		};
		case	diverse::rhi::AccessType::GeometryShaderReadSampledImageOrUniformTexelBuffer:  return AccessInfo{
				VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT,
				VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		case	diverse::rhi::AccessType::GeometryShaderReadOther:  return AccessInfo{
				VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT,
				VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_GENERAL,
		};
		case	diverse::rhi::AccessType::FragmentShaderReadUniformBuffer:  return AccessInfo{
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				VkAccessFlagBits::VK_ACCESS_UNIFORM_READ_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
		};
		case	diverse::rhi::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer:  return AccessInfo{
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		case	diverse::rhi::AccessType::FragmentShaderReadColorInputAttachment:  return AccessInfo{
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		case	diverse::rhi::AccessType::FragmentShaderReadDepthStencilInputAttachment:  return AccessInfo{
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		};
		case	diverse::rhi::AccessType::FragmentShaderReadOther:  return AccessInfo{
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_GENERAL,
		};
		case	diverse::rhi::AccessType::ColorAttachmentRead:  return AccessInfo{
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};
		case	diverse::rhi::AccessType::DepthStencilAttachmentRead:  return AccessInfo{
				VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
					| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		};
		case	diverse::rhi::AccessType::ComputeShaderReadUniformBuffer:  return AccessInfo{
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VkAccessFlagBits::VK_ACCESS_UNIFORM_READ_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
		};
		case	diverse::rhi::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer:  return AccessInfo{
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		case	diverse::rhi::AccessType::ComputeShaderReadOther:  return AccessInfo{
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_GENERAL,
		};
		case	diverse::rhi::AccessType::AnyShaderReadUniformBuffer:  return AccessInfo{
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				VkAccessFlagBits::VK_ACCESS_UNIFORM_READ_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
		};
		case	diverse::rhi::AccessType::AnyShaderReadUniformBufferOrVertexBuffer:  return AccessInfo{
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				VK_ACCESS_UNIFORM_READ_BIT
					| VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
		};
		case	diverse::rhi::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer:  return AccessInfo{
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		case diverse::rhi::AccessType::AnyShaderReadOther:  return AccessInfo{
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_GENERAL,
		};
		case diverse::rhi::AccessType::TransferRead: return AccessInfo{
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_ACCESS_TRANSFER_READ_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		};
		case diverse::rhi::AccessType::HostRead: return AccessInfo{
		VK_PIPELINE_STAGE_HOST_BIT,
		VK_ACCESS_HOST_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL,
		};
		case diverse::rhi::AccessType::Present: return AccessInfo{
		0,
		0,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		};
		case diverse::rhi::AccessType::CommandBufferWriteNVX: return AccessInfo{
			VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV,
			VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV,
			VK_IMAGE_LAYOUT_UNDEFINED,
		};
		case diverse::rhi::AccessType::VertexShaderWrite: return AccessInfo{
		VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
		VK_ACCESS_SHADER_WRITE_BIT,
		VK_IMAGE_LAYOUT_GENERAL,
		};

		case diverse::rhi::AccessType::TessellationControlShaderWrite: return AccessInfo{
		VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
		VK_ACCESS_SHADER_WRITE_BIT,
		VK_IMAGE_LAYOUT_GENERAL,
		};

		case diverse::rhi::AccessType::TessellationEvaluationShaderWrite: return AccessInfo{
		VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
		VK_ACCESS_SHADER_WRITE_BIT,
		VK_IMAGE_LAYOUT_GENERAL,
		};

		case diverse::rhi::AccessType::GeometryShaderWrite: return AccessInfo{
		VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT,
		VK_ACCESS_SHADER_WRITE_BIT,
		VK_IMAGE_LAYOUT_GENERAL,
		};
		case diverse::rhi::AccessType::FragmentShaderWrite: return AccessInfo{
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_ACCESS_SHADER_WRITE_BIT,
		VK_IMAGE_LAYOUT_GENERAL,
		};
		case diverse::rhi::AccessType::ColorAttachmentWrite: return AccessInfo{
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};

		case diverse::rhi::AccessType::DepthStencilAttachmentWrite: return AccessInfo{
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
			| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		};
		case diverse::rhi::AccessType::DepthAttachmentWriteStencilReadOnly:  return AccessInfo{
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
			| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
			| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
		VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL,
		};

		case diverse::rhi::AccessType::StencilAttachmentWriteDepthReadOnly:  return AccessInfo{
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
			| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
			| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
		VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL,
		};

		case diverse::rhi::AccessType::ComputeShaderWrite:  return AccessInfo{
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_ACCESS_SHADER_WRITE_BIT,
		VK_IMAGE_LAYOUT_GENERAL,
		};

		case diverse::rhi::AccessType::AnyShaderWrite: return AccessInfo{
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_IMAGE_LAYOUT_GENERAL,
		};

		case diverse::rhi::AccessType::TransferWrite: return AccessInfo{
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		};

		case diverse::rhi::AccessType::HostWrite: return AccessInfo{
		VK_PIPELINE_STAGE_HOST_BIT,
		VK_ACCESS_HOST_WRITE_BIT,
		VK_IMAGE_LAYOUT_GENERAL,
		};

		case diverse::rhi::AccessType::ColorAttachmentReadWrite: return AccessInfo{
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
				| VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};
		case	diverse::rhi::AccessType::General:  return AccessInfo{
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
				VK_IMAGE_LAYOUT_GENERAL,
		};
		default:
			return AccessInfo{
				0,
				0,
				VK_IMAGE_LAYOUT_UNDEFINED,
			};
		}
		return AccessInfo{
				0,
				0,
				VK_IMAGE_LAYOUT_UNDEFINED,
		};
	}

	inline auto is_write_access(diverse::rhi::AccessType access_type) -> bool
	{
		switch (access_type)
		{
		case	diverse::rhi::AccessType::CommandBufferWriteNVX: return true;
		case	diverse::rhi::AccessType::VertexShaderWrite: return true;
		case	diverse::rhi::AccessType::TessellationControlShaderWrite: return true;
		case	diverse::rhi::AccessType::TessellationEvaluationShaderWrite: return true;
		case	diverse::rhi::AccessType::GeometryShaderWrite: return true;
		case	diverse::rhi::AccessType::FragmentShaderWrite: return true;
		case	diverse::rhi::AccessType::ColorAttachmentWrite: return true;
		case	diverse::rhi::AccessType::DepthStencilAttachmentWrite: return true;
		case	diverse::rhi::AccessType::DepthAttachmentWriteStencilReadOnly: return true;
		case	diverse::rhi::AccessType::StencilAttachmentWriteDepthReadOnly: return true;
		case	diverse::rhi::AccessType::ComputeShaderWrite:  return true;
		case	diverse::rhi::AccessType::AnyShaderWrite:  return true;
		case	diverse::rhi::AccessType::TransferWrite:  return true;
		case	diverse::rhi::AccessType::HostWrite:  return true;
		case	diverse::rhi::AccessType::ColorAttachmentReadWrite: return true;
		case	diverse::rhi::AccessType::General:  return true;
		default:  return false;
		}

		return false;
	}
	/// Image barriers should only be used when a queue family ownership transfer
	/// or an image layout transition is required - prefer global barriers at all
	/// other times.
	///
	/// In general it is better to use image barriers with `ImageLayout::Optimal`
	/// than it is to use global barriers with images using either of the
	/// `ImageLayout::General*` layouts.
	///
	/// Access types are defined in the same way as for a global memory barrier, but
	/// they only affect the image subresource range identified by `image` and
	/// `range`, rather than all resources.
	///
	/// `src_queue_family_index`, `dst_queue_family_index`, `image`, and `range` will
	/// be passed unmodified into an image memory barrier.
	///
	/// An image barrier defining a queue ownership transfer needs to be executed
	/// twice - once by a queue in the source queue family, and then once again by a
	/// queue in the destination queue family, with a semaphore guaranteeing
	/// execution order between them.
	///
	/// If `discard_contents` is set to true, the contents of the image become
	/// undefined after the barrier is executed, which can result in a performance
	/// boost over attempting to preserve the contents. This is particularly useful
	/// for transient images where the contents are going to be immediately overwritten.
	/// A good example of when to use this is when an application re-uses a presented
	/// image after acquiring the next swap chain image.

	struct ImageBarrier
	{
		std::vector<diverse::rhi::AccessType> previous_accesses;
		std::vector<diverse::rhi::AccessType> next_accesses;
		ImageLayout previous_layout;
		ImageLayout next_layout;
		bool discard_contents;
		unsigned int src_queue_family_index;
		unsigned int dst_queue_family_index;
		ImageType image;
		ImageSubresourceRangeType range;
	};

	/// Mapping function that translates a global barrier into a set of source and
	/// destination pipeline stages, and a memory barrier, that can be used with
	/// Vulkan synchronization methods.
	inline std::tuple<VkPipelineStageFlags, VkPipelineStageFlags, VkMemoryBarrier> get_memory_barrier(GlobalBarrier barrier)
	{
		auto src_stages = 0;
		auto dst_stages = 0;

		VkMemoryBarrier memory_barrier = {};
		memory_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		for (auto previous_access : barrier.previous_accesses)
		{
			auto previous_info = get_access_info(previous_access);

			src_stages |= previous_info.stage_mask;

			// Add appropriate availability operations - for writes only.
			if (is_write_access(previous_access))
			{
				memory_barrier.srcAccessMask |= previous_info.access_mask;
			}
		}

		for (auto next_access : barrier.next_accesses)
		{
			auto next_info = get_access_info(next_access);

			dst_stages |= next_info.stage_mask;

			// Add visibility operations as necessary.
			// If the src access mask, this is a WAR hazard (or for some reason a "RAR"),
			// so the dst access mask can be safely zeroed as these don't need visibility.
			if (memory_barrier.srcAccessMask != 0)
			{
				memory_barrier.dstAccessMask |= next_info.access_mask;
			}
		}

		// Ensure that the stage masks are valid if no stages were determined
		if (!src_stages)
		{
			src_stages = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		}

		if (!dst_stages)
		{
			dst_stages = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		}

		return std::make_tuple(src_stages, dst_stages, memory_barrier);
	}

	/// Mapping function that translates a buffer barrier into a set of source and
	/// destination pipeline stages, and a buffer memory barrier, that can be used
	/// with Vulkan synchronization methods.
	inline auto  get_buffer_memory_barrier(BufferBarrier barrier) -> std::tuple<VkPipelineStageFlags,VkPipelineStageFlags,VkBufferMemoryBarrier>
	{
		auto src_stages = 0;
		auto dst_stages = 0;

		VkBufferMemoryBarrier buffer_barrier = {};
		buffer_barrier.srcQueueFamilyIndex = barrier.src_queue_family_index;
		buffer_barrier.dstQueueFamilyIndex = barrier.dst_queue_family_index;
		buffer_barrier.offset = barrier.offset;
		buffer_barrier.buffer = barrier.buffer;
		buffer_barrier.size = barrier.size;
		buffer_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;

		for (auto previous_access : barrier.previous_accesses)
		{
			auto previous_info = get_access_info(previous_access);

			src_stages |= previous_info.stage_mask;

			// Add appropriate availability operations - for writes only.
			if (is_write_access(previous_access)) {
				buffer_barrier.srcAccessMask |= previous_info.access_mask;
			}
		}

		for (auto next_access : barrier.next_accesses)
		{
			auto next_info = get_access_info(next_access);

			dst_stages |= next_info.stage_mask;

			// Add visibility operations as necessary.
			// If the src access mask, this is a WAR hazard (or for some reason a "RAR"),
			// so the dst access mask can be safely zeroed as these don't need visibility.
			if (buffer_barrier.srcAccessMask != 0)
				buffer_barrier.dstAccessMask |= next_info.access_mask;
		}

		// Ensure that the stage masks are valid if no stages were determined
		if (!src_stages)
		{
			src_stages = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		}

		if (!dst_stages)
			dst_stages = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

		return std::make_tuple(src_stages, dst_stages, buffer_barrier);
	}

	/// Mapping function that translates an image barrier into a set of source and
	/// destination pipeline stages, and an image memory barrier, that can be used
	/// with Vulkan synchronization methods.
	auto get_image_memory_barrier(ImageBarrier barrier) -> std::tuple<
		VkPipelineStageFlags,
		VkPipelineStageFlags,
		VkImageMemoryBarrier>;

	void pipeline_barrier(VkDevice device, 
		VkCommandBuffer command_buffer,
		std::optional<GlobalBarrier> global_barrier,
		const std::vector<BufferBarrier>& buffer_barriers,
		const std::vector<ImageBarrier>& image_barriers);
}



//DECLARE_ENUM_OPERATORS(vk::AccessType);

