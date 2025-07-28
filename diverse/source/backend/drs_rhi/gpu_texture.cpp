#include "gpu_texture.h"

auto diverse::rhi::image_access_type_to_usage_flags(AccessType access_type) -> TextureUsageFlags
{
	switch ( access_type)
	{
	case diverse::rhi::VertexShaderReadSampledImageOrUniformTexelBuffer:
	case diverse::rhi::VertexShaderReadOther:
	case diverse::rhi::TessellationControlShaderReadSampledImageOrUniformTexelBuffer:
	case diverse::rhi::TessellationControlShaderReadOther:
	case diverse::rhi::TessellationEvaluationShaderReadSampledImageOrUniformTexelBuffer:
	case diverse::rhi::TessellationEvaluationShaderReadOther:
	case diverse::rhi::GeometryShaderReadSampledImageOrUniformTexelBuffer:
	case diverse::rhi::GeometryShaderReadOther:
	case diverse::rhi::ComputeShaderReadSampledImageOrUniformTexelBuffer:
	case diverse::rhi::ComputeShaderReadOther:
	case diverse::rhi::AnyShaderReadSampledImageOrUniformTexelBuffer:
	case diverse::rhi::AnyShaderReadOther:
	case diverse::rhi::FragmentShaderReadSampledImageOrUniformTexelBuffer:
		return TextureUsageFlags::SAMPLED;

	case diverse::rhi::VertexShaderReadUniformBuffer:
	case diverse::rhi::TessellationControlShaderReadUniformBuffer:
	case diverse::rhi::TessellationEvaluationShaderReadUniformBuffer:
	case diverse::rhi::GeometryShaderReadUniformBuffer:
	case diverse::rhi::FragmentShaderReadUniformBuffer:
		return TextureUsageFlags::SAMPLED;
	case diverse::rhi::TransferRead:
		return TextureUsageFlags::TRANSFER_SRC;
	case diverse::rhi::VertexShaderWrite:
	case diverse::rhi::TessellationControlShaderWrite:
	case diverse::rhi::TessellationEvaluationShaderWrite:
	case diverse::rhi::GeometryShaderWrite:
	case diverse::rhi::FragmentShaderWrite:
	case diverse::rhi::ComputeShaderWrite:
	case diverse::rhi::AnyShaderWrite:
		return TextureUsageFlags::STORAGE;
	case diverse::rhi::TransferWrite:
		return TextureUsageFlags::TRANSFER_DST;
	case diverse::rhi::General:
		return TextureUsageFlags::STORAGE;

	case AccessType::ColorAttachmentReadWrite:
	case AccessType::ColorAttachmentWrite:
	case AccessType::ColorAttachmentRead:
		return TextureUsageFlags::COLOR_ATTACHMENT;
	case AccessType::DepthAttachmentWriteStencilReadOnly:
	case AccessType::DepthStencilAttachmentWrite:
	case AccessType::DepthStencilAttachmentRead:
		return TextureUsageFlags::DEPTH_STENCIL_ATTACHMENT;

	default:
		return TextureUsageFlags::SAMPLED;
	}
}

u32 diverse::get_pixel_bytes(diverse::PixelFormat format)
{
	switch (format)
	{
	case diverse::PixelFormat::R32G32B32A32_Typeless:
	case diverse::PixelFormat::R32G32B32A32_Float:
	case diverse::PixelFormat::R32G32B32A32_UInt:
	case diverse::PixelFormat::R32G32B32A32_SInt:
		return 4 * 4;
	case diverse::PixelFormat::R32G32B32_Typeless:
	case diverse::PixelFormat::R32G32B32_Float:
	case diverse::PixelFormat::R32G32B32_UInt:
	case diverse::PixelFormat::R32G32B32_SInt:
		return 4 * 3;
	case diverse::PixelFormat::R16G16B16A16_Typeless:
	case diverse::PixelFormat::R16G16B16A16_Float:
	case diverse::PixelFormat::R16G16B16A16_UNorm:
	case diverse::PixelFormat::R16G16B16A16_UInt:
	case diverse::PixelFormat::R16G16B16A16_SNorm:
	case diverse::PixelFormat::R16G16B16A16_SInt:
		return 2 * 4;
	case diverse::PixelFormat::R32G32_Typeless:
	case diverse::PixelFormat::R32G32_Float:
	case diverse::PixelFormat::R32G32_UInt:
	case diverse::PixelFormat::R32G32_SInt:
	case diverse::PixelFormat::R32G8X24_Typeless:
	case diverse::PixelFormat::D32_Float_S8X24_UInt:
	case diverse::PixelFormat::R32_Float_X8X24_Typeless:
	case diverse::PixelFormat::X32_Typeless_G8X24_UInt:
		return 2 * 4;
	case diverse::PixelFormat::R10G10B10A2_Typeless:
	case diverse::PixelFormat::R10G10B10A2_UNorm:
	case diverse::PixelFormat::R10G10B10A2_UInt:
	case diverse::PixelFormat::R11G11B10_Float:
	case diverse::PixelFormat::R8G8B8A8_Typeless:
	case diverse::PixelFormat::R8G8B8A8_UNorm:
	case diverse::PixelFormat::R8G8B8A8_UNorm_sRGB:
	case diverse::PixelFormat::R8G8B8A8_UInt:
	case diverse::PixelFormat::R8G8B8A8_SNorm:
	case diverse::PixelFormat::R8G8B8A8_SInt:
	case diverse::PixelFormat::R16G16_Typeless:
	case diverse::PixelFormat::R16G16_Float:
	case diverse::PixelFormat::R16G16_UNorm:
	case diverse::PixelFormat::R16G16_UInt:
	case diverse::PixelFormat::R16G16_SNorm:
	case diverse::PixelFormat::R16G16_SInt:
	case diverse::PixelFormat::R32_Typeless:
	case diverse::PixelFormat::D32_Float:
	case diverse::PixelFormat::R32_Float:
	case diverse::PixelFormat::R32_UInt:
	case diverse::PixelFormat::R32_SInt:
	case diverse::PixelFormat::R24G8_Typeless:
	case diverse::PixelFormat::D24_UNorm_S8_UInt:
	case diverse::PixelFormat::R24_UNorm_X8_Typeless:
	case diverse::PixelFormat::X24_Typeless_G8_UInt:
		return 4;
	case diverse::PixelFormat::R8G8_Typeless:
	case diverse::PixelFormat::R8G8_UNorm:
	case diverse::PixelFormat::R8G8_UInt:
	case diverse::PixelFormat::R8G8_SNorm:
	case diverse::PixelFormat::R8G8_SInt:
	case diverse::PixelFormat::R16_Typeless:
	case diverse::PixelFormat::R16_Float:
	case diverse::PixelFormat::D16_UNorm:
	case diverse::PixelFormat::R16_UNorm:
	case diverse::PixelFormat::R16_UInt:
	case diverse::PixelFormat::R16_SNorm:
	case diverse::PixelFormat::R16_SInt:
		return 2;
	case diverse::PixelFormat::R8_Typeless:
	case diverse::PixelFormat::R8_UNorm:
	case diverse::PixelFormat::R8_UInt:
	case diverse::PixelFormat::R8_SNorm:
	case diverse::PixelFormat::R8_SInt:
	case diverse::PixelFormat::A8_UNorm:
		return 1;
	case diverse::PixelFormat::B8G8R8A8_UNorm:
	case diverse::PixelFormat::B8G8R8X8_UNorm:
	case diverse::PixelFormat::R10G10B10_Xr_Bias_A2_UNorm:
	case diverse::PixelFormat::B8G8R8A8_Typeless:
	case diverse::PixelFormat::B8G8R8A8_UNorm_sRGB:
	case diverse::PixelFormat::B8G8R8X8_Typeless:
	case diverse::PixelFormat::B8G8R8X8_UNorm_sRGB:
		return 4;
	default:
		break;
	}
	return 4;
}

diverse::rhi::GpuTextureDesc::GpuTextureDesc(PixelFormat fmt, TextureType img_ty, std::array<uint32, 3> extents)
	: format(fmt), image_type(img_ty), extent(extents)
{
}

diverse::rhi::GpuTextureDesc::GpuTextureDesc(TextureType img_ty, TextureUsageFlags usa, TextureCreateFlags flag, PixelFormat fmt, std::array<uint32, 3> extents, uint32 mip, uint32 array_ele)
	: image_type(img_ty), usage(usa), flags(flag), format(fmt), extent(extents), mip_levels(mip), array_elements(array_ele)
{
}
