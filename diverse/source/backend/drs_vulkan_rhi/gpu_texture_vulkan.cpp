#include <unordered_map>
#include "gpu_texture_vulkan.h"
#include "gpu_device_vulkan.h"
namespace diverse
{
    namespace rhi
    {
        VkFormat PixelFormatToVkFormat[static_cast<int32>(PixelFormat::MAX)] = {

            VK_FORMAT_UNDEFINED,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_FORMAT_R32G32B32A32_UINT,
            VK_FORMAT_R32G32B32A32_SINT,
            VK_FORMAT_R32G32B32_SFLOAT,
            VK_FORMAT_R32G32B32_SFLOAT,
            VK_FORMAT_R32G32B32_UINT,
            VK_FORMAT_R32G32B32_SINT,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_FORMAT_R16G16B16A16_UNORM,
            VK_FORMAT_R16G16B16A16_UINT,
            VK_FORMAT_R16G16B16A16_SNORM,
            VK_FORMAT_R16G16B16A16_SINT,
            VK_FORMAT_R32G32_SFLOAT,
            VK_FORMAT_R32G32_SFLOAT,
            VK_FORMAT_R32G32_UINT,
            VK_FORMAT_R32G32_SINT,
            VK_FORMAT_UNDEFINED,
            // TODO: R32G8X24_Typeless
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_UNDEFINED,
            // TODO: R32_Float_X8X24_Typeless
            VK_FORMAT_UNDEFINED,
            // TODO: X32_Typeless_G8X24_UInt
            VK_FORMAT_A2R10G10B10_UNORM_PACK32,
            VK_FORMAT_A2R10G10B10_UNORM_PACK32,
            VK_FORMAT_A2R10G10B10_UINT_PACK32,
            VK_FORMAT_B10G11R11_UFLOAT_PACK32,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_FORMAT_R8G8B8A8_UINT,
            VK_FORMAT_R8G8B8A8_SNORM,
            VK_FORMAT_R8G8B8A8_SINT,
            VK_FORMAT_R16G16_SFLOAT,
            VK_FORMAT_R16G16_SFLOAT,
            VK_FORMAT_R16G16_UNORM,
            VK_FORMAT_R16G16_UINT,
            VK_FORMAT_R16G16_SNORM,
            VK_FORMAT_R16G16_SINT,
            VK_FORMAT_R32_SFLOAT,
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_R32_SFLOAT,
            VK_FORMAT_R32_UINT,
            VK_FORMAT_R32_SINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_X8_D24_UNORM_PACK32,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_R8G8_UNORM,
            VK_FORMAT_R8G8_UNORM,
            VK_FORMAT_R8G8_UINT,
            VK_FORMAT_R8G8_SNORM,
            VK_FORMAT_R8G8_SINT,
            VK_FORMAT_R16_SFLOAT,
            VK_FORMAT_R16_SFLOAT,
            VK_FORMAT_D16_UNORM,
            VK_FORMAT_R16_UNORM,
            VK_FORMAT_R16_UINT,
            VK_FORMAT_R16_SNORM,
            VK_FORMAT_R16_SINT,
            VK_FORMAT_R8_UNORM,
            VK_FORMAT_R8_UNORM,
            VK_FORMAT_R8_UINT,
            VK_FORMAT_R8_SNORM,
            VK_FORMAT_R8_SINT,
            VK_FORMAT_UNDEFINED,
            // TODO: A8_UNorm
            VK_FORMAT_UNDEFINED,
            // TODO: R1_UNorm
            VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
            VK_FORMAT_UNDEFINED,
            // TODO: R8G8_B8G8_UNorm
            VK_FORMAT_UNDEFINED,
            // TODO: G8R8_G8B8_UNorm
            VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
            VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
            VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
            VK_FORMAT_BC2_UNORM_BLOCK,
            VK_FORMAT_BC2_UNORM_BLOCK,
            VK_FORMAT_BC2_SRGB_BLOCK,
            VK_FORMAT_BC3_UNORM_BLOCK,
            VK_FORMAT_BC3_UNORM_BLOCK,
            VK_FORMAT_BC3_SRGB_BLOCK,
            VK_FORMAT_BC4_UNORM_BLOCK,
            VK_FORMAT_BC4_UNORM_BLOCK,
            VK_FORMAT_BC4_SNORM_BLOCK,
            VK_FORMAT_BC5_UNORM_BLOCK,
            VK_FORMAT_BC5_UNORM_BLOCK,
            VK_FORMAT_BC5_SNORM_BLOCK,
            VK_FORMAT_B5G6R5_UNORM_PACK16,
            VK_FORMAT_B5G5R5A1_UNORM_PACK16,
            VK_FORMAT_B8G8R8A8_UNORM,
            VK_FORMAT_B8G8R8A8_UNORM,
            VK_FORMAT_UNDEFINED,
            // TODO: R10G10B10_Xr_Bias_A2_UNorm
            VK_FORMAT_B8G8R8A8_UNORM,
            VK_FORMAT_B8G8R8A8_SRGB,
            VK_FORMAT_B8G8R8A8_UNORM,
            VK_FORMAT_B8G8R8A8_SRGB,
            VK_FORMAT_BC6H_UFLOAT_BLOCK,
            VK_FORMAT_BC6H_UFLOAT_BLOCK,
            VK_FORMAT_BC6H_SFLOAT_BLOCK,
            VK_FORMAT_BC7_UNORM_BLOCK,
            VK_FORMAT_BC7_UNORM_BLOCK,
            VK_FORMAT_BC7_SRGB_BLOCK,
        };

		auto texture_type_2_vk_view_type(TextureType image_type) -> VkImageViewType
		{
			switch (image_type)
			{
			case TextureType::Tex1d:
				return VkImageViewType::VK_IMAGE_VIEW_TYPE_1D;
			case TextureType::Tex1dArray:
				return VkImageViewType::VK_IMAGE_VIEW_TYPE_1D_ARRAY;
			case TextureType::Tex2d:
				return VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
			case TextureType::Tex2dArray:
				return VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			case TextureType::Tex3d:
				return VkImageViewType::VK_IMAGE_VIEW_TYPE_3D;
			case TextureType::CubeArray:
				return VkImageViewType::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
            case TextureType::Cube:
                return VkImageViewType::VK_IMAGE_VIEW_TYPE_CUBE;
			default:
				return VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
			}
			return VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
		}
		auto pixel_format_2_vk(PixelFormat pxiel_format) -> VkFormat
		{
            return PixelFormatToVkFormat[(int32)pxiel_format];
		}
		auto texture_usage_2_vk(TextureUsageFlags flags) -> VkImageUsageFlags
		{
        /*    switch (flags)
            {
            case diverse::rhi::TextureUsageFlags::TRANSFER_SRC:
               return VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            case diverse::rhi::TextureUsageFlags::TRANSFER_DST:
                return VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            case diverse::rhi::TextureUsageFlags::SAMPLED:
                return VK_IMAGE_USAGE_SAMPLED_BIT;
            case diverse::rhi::TextureUsageFlags::STORAGE:
                return VK_IMAGE_USAGE_STORAGE_BIT;
            case diverse::rhi::TextureUsageFlags::COLOR_ATTACHMENT:
                return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            case diverse::rhi::TextureUsageFlags::DEPTH_STENCIL_ATTACHMENT:
                return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            case diverse::rhi::TextureUsageFlags::TRANSIENT_ATTACHMENT:
                return VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
            case diverse::rhi::TextureUsageFlags::INPUT_ATTACHMENT:
                return VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
            default:
                break;
            }*/
            return (VkImageUsageFlags)(flags);
		}
		auto texture_create_flag_2_vk(TextureCreateFlags flags) -> VkImageCreateFlags
		{
 /*           switch (flags)
            {
            case diverse::rhi::TextureCreateFlags::SPARSE_BINDING:
                return VK_IMAGE_CREATE_SPARSE_BINDING_BIT;
            case diverse::rhi::TextureCreateFlags::SPARSE_RESIDENCY:
                return VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;
            case diverse::rhi::TextureCreateFlags::SPARSE_ALIASED:
                return VK_IMAGE_CREATE_SPARSE_ALIASED_BIT;
            case diverse::rhi::TextureCreateFlags::MUTABLE_FORMAT:
                return VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
            case diverse::rhi::TextureCreateFlags::CUBE_COMPATIBLE:
                return VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            default:
                break;
            }*/
            return (VkImageCreateFlags)(flags);
		}
		auto image_aspect_flag_2_vk(ImageAspectFlags flags) -> VkImageAspectFlags
		{
            //switch (flags)
            //{
            //case diverse::rhi::ImageAspectFlags::COLOR:
            //    return VK_IMAGE_ASPECT_COLOR_BIT;
            //case diverse::rhi::ImageAspectFlags::DEPTH:
            //    return VK_IMAGE_ASPECT_DEPTH_BIT;
            //case diverse::rhi::ImageAspectFlags::STENCIL:
            //    return VK_IMAGE_ASPECT_STENCIL_BIT;
            //case diverse::rhi::ImageAspectFlags::METADATA:
            //    return VK_IMAGE_ASPECT_METADATA_BIT;
            //default:
            //    break;
            //}
            return (VkImageAspectFlags)(flags);
		}
		auto get_image_create_info(const GpuTextureDesc& desc, bool initial_data) -> VkImageCreateInfo
		{
			VkImageType img_type;
			VkExtent3D	img_extent;
			uint32 img_layers = 1;
			switch (desc.image_type)
			{
			case TextureType::Tex1d:
				img_type = VK_IMAGE_TYPE_1D;
				img_extent = { desc.extent[0], 1,1 };
				img_layers = 1;
				break;
			case TextureType::Tex1dArray:
				img_type = VK_IMAGE_TYPE_1D;
				img_extent = { desc.extent[0], 1,1 };
				img_layers = desc.array_elements;
				break;
			case TextureType::Tex2d:
				img_type = VK_IMAGE_TYPE_2D;
				img_extent = { desc.extent[0], desc.extent[1],1 };
				img_layers = 1;
				break;
			case TextureType::Tex2dArray:
				img_type = VK_IMAGE_TYPE_2D;
				img_extent = { desc.extent[0], desc.extent[1],1 };
				img_layers = desc.array_elements;
				break;
			case TextureType::Tex3d:
				img_type = VK_IMAGE_TYPE_3D;
				img_extent = { desc.extent[0], desc.extent[1],desc.extent[2] };
				img_layers = 1;
				break;
			case TextureType::Cube:
				img_type = VK_IMAGE_TYPE_2D;
				img_extent = { desc.extent[0], desc.extent[1],1 };
				img_layers = 6;
				break;
			case TextureType::CubeArray:
				img_type = VK_IMAGE_TYPE_2D;
				img_extent = { desc.extent[0], desc.extent[1],1 };
				img_layers = 6 * desc.array_elements;
				break;
			default:
				img_type = VK_IMAGE_TYPE_2D;
				img_extent = { desc.extent[0], desc.extent[1],1 };
				img_layers = 1;
				break;
			}
			auto img_usage = desc.usage;
			if (initial_data)
				img_usage |= TextureUsageFlags::TRANSFER_DST;
			VkImageCreateInfo image_create_info = {};
			image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image_create_info.imageType = img_type;
			image_create_info.extent = img_extent;
			image_create_info.mipLevels = desc.mip_levels;
			image_create_info.format = pixel_format_2_vk(desc.format);
			image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;//desc.tiling;
			image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			image_create_info.usage = texture_usage_2_vk(img_usage);
			image_create_info.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
			image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			image_create_info.arrayLayers = img_layers;
			image_create_info.flags = texture_create_flag_2_vk(desc.flags);
			image_create_info.initialLayout = initial_data ? VK_IMAGE_LAYOUT_PREINITIALIZED : VK_IMAGE_LAYOUT_UNDEFINED;
			return image_create_info;
		}

        auto GpuTextureVulkan::view(const GpuDevice* device, const GpuTextureViewDesc& view_desc) -> std::shared_ptr<GpuTextureView>
        {
            auto vk_device = dynamic_cast<const GpuDeviceVulkan*>(device);
            auto it = views.find(view_desc);
            if (it == views.end())
            {
                auto img_view = vk_device->create_image_view(view_desc, desc, image);
                views[view_desc] = std::move(img_view);
            }
            return views[view_desc];
        }
        auto GpuTextureVulkan::view_desc(const GpuTextureViewDesc& view_desc) -> VkImageViewCreateInfo
        {
            return GpuTextureVulkan::view_desc(desc, view_desc);
        }
        
        auto GpuTextureVulkan::view_desc(const GpuTextureDesc& desc, const GpuTextureViewDesc& view_desc) -> VkImageViewCreateInfo
        {
            VkImageViewCreateInfo	image_view_create_info = {};
            image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            image_view_create_info.format = pixel_format_2_vk(desc.format);
            image_view_create_info.components = VkComponentMapping{VkComponentSwizzle::VK_COMPONENT_SWIZZLE_R,VkComponentSwizzle::VK_COMPONENT_SWIZZLE_G ,VkComponentSwizzle::VK_COMPONENT_SWIZZLE_B,
            VkComponentSwizzle::VK_COMPONENT_SWIZZLE_A};
            image_view_create_info.viewType = texture_type_2_vk_view_type(view_desc.view_type.value_or(desc.image_type));
            image_view_create_info.subresourceRange = VkImageSubresourceRange{
                image_aspect_flag_2_vk(view_desc.aspect_mask),
                view_desc.base_mip_level,
                view_desc.level_count > 0 ? view_desc.level_count : desc.mip_levels,
                0,
                (desc.image_type == TextureType::Cube || TextureType::CubeArray == desc.image_type) ? 6u : 1u
            };
            return image_view_create_info;
        }

        auto mipmap_mode_2_vk(const SamplerMipmapMode& mode) -> VkSamplerMipmapMode
        {
            switch ( mode )
            {
            case SamplerMipmapMode::LINEAR:
                return VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR;
            case SamplerMipmapMode::NEAREST:
                return VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_NEAREST;
            default:
                break;
            }
            return VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR;
        }
        auto address_mode_2_vk(const SamplerAddressMode& mode) -> VkSamplerAddressMode
        {
            switch ( mode )
            {
            case SamplerAddressMode::CLAMP_TO_BORDER:
                return VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            case SamplerAddressMode::CLAMP_TO_EDGE:
                return VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case SamplerAddressMode::MIRRORED_REPEAT:
                return VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            case SamplerAddressMode::REPEAT:
                return VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT;
            default:
                break;
            }
            return VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        }
        auto texel_filter_mode_2_vk(const TexelFilter& mode) -> VkFilter
        {
            switch ( mode )
            {
            case TexelFilter::NEAREST:
                return VkFilter::VK_FILTER_NEAREST;
            case TexelFilter::LINEAR:
                return VkFilter::VK_FILTER_LINEAR;
            default:
                break;
            }
            return VkFilter::VK_FILTER_NEAREST;
        }

        GpuTextureViewVulkan::~GpuTextureViewVulkan()
        {
            auto device = dynamic_cast<GpuDeviceVulkan*>(get_global_device());
            if (img_view != VK_NULL_HANDLE && device)
            {
                device->defer_release([img_view = this->img_view, device]() mutable {
                    if (img_view != VK_NULL_HANDLE)
                    {
                        vkDestroyImageView(device->device, img_view, nullptr);
                        img_view = VK_NULL_HANDLE;
                    }
                });
            }
        }

        GpuTextureVulkan::~GpuTextureVulkan()
        {
            if( b_swapchin ) return;
            auto device = dynamic_cast<GpuDeviceVulkan*>(get_global_device());
            if(device)
            {
                g_device->defer_release([handle = this->image, allocation = this->allocation, device]() mutable{
                    if(handle != VK_NULL_HANDLE )
                    {
                        vkDestroyImage(device->device, handle, nullptr);
                        //vmaDestroyImage(device->global_allocator,handle, allocation);
                        handle = VK_NULL_HANDLE;
                        if (allocation != nullptr)
                        {
                            vmaFreeMemory(device->global_allocator, allocation);
                        }
                    }
                });
            }
        }
    }
}
