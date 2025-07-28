#pragma once
#include "backend/drs_rhi/gpu_texture.h"
#include "backend/drs_rhi/gpu_sampler.h"
#include "vk_utils.h"

namespace diverse
{
	namespace rhi
	{

		struct GpuTextureViewVulkan : public GpuTextureView
		{
			GpuTextureViewVulkan(VkImageView view):img_view(view){}
			~GpuTextureViewVulkan();
			VkImageView	img_view = VK_NULL_HANDLE;
		};

		struct GpuTextureVulkan : public GpuTexture
		{
			~GpuTextureVulkan();
			VkImage	image = VK_NULL_HANDLE;
			VmaAllocation allocation{};
			std::unordered_map<GpuTextureViewDesc, std::shared_ptr<GpuTextureView>>	views;
			bool b_swapchin = false;
			auto view(const struct GpuDevice* device, const GpuTextureViewDesc& desc) -> std::shared_ptr<GpuTextureView>;
			auto view_desc( const GpuTextureViewDesc& view_desc)->VkImageViewCreateInfo;
			static auto view_desc(const GpuTextureDesc& desc,const GpuTextureViewDesc& view_desc)->VkImageViewCreateInfo;
		};

		auto get_image_create_info(const GpuTextureDesc& desc, bool initial_data) -> VkImageCreateInfo;


		auto texture_type_2_vk_view_type(TextureType image_type) -> VkImageViewType;

		auto pixel_format_2_vk(PixelFormat pxiel_format)->VkFormat;
		auto texture_usage_2_vk(TextureUsageFlags flags)->VkImageUsageFlags;
		auto texture_create_flag_2_vk(TextureCreateFlags flags) -> VkImageCreateFlags;
		auto image_aspect_flag_2_vk(ImageAspectFlags flags)->VkImageAspectFlags;

		auto mipmap_mode_2_vk(const SamplerMipmapMode& mode)-> VkSamplerMipmapMode;
		auto address_mode_2_vk(const SamplerAddressMode& mode) -> VkSamplerAddressMode;
		auto texel_filter_mode_2_vk(const TexelFilter& mode) -> VkFilter;
	}
}