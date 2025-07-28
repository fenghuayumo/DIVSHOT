#pragma once

#include "utility/hash_utils.h"
#include "pixel_format.h"
#include <spdlog/fmt/ostr.h>
#include <optional>
#define MAX_DYNAMIC_CONSTANTS_BYTES_PER_DISPATCH  16384
#define DYNAMIC_CONSTANTS_ALIGNMENT  256
#define DYNAMIC_CONSTANTS_SIZE_BYTES 1024 * 1024 * 16
#define DYNAMIC_CONSTANTS_BUFFER_COUNT 2
#define MAX_DYNAMIC_CONSTANTS_STORAGE_BUFFER_BYTES  1024 * 1024

namespace diverse
{
    namespace rhi
    {

		/// Defines all potential resource usages
		enum  AccessType
		{
			/// No access. Useful primarily for initialization
			Nothing,

			/// Command buffer read operation as defined by `NVX_device_generated_commands`
			CommandBufferReadNVX,

			/// Read as an indirect buffer for drawing or dispatch
			IndirectBuffer,

			/// Read as an index buffer for drawing
			IndexBuffer,

			/// Read as a vertex buffer for drawing
			VertexBuffer,

			/// Read as a uniform buffer in a vertex shader
			VertexShaderReadUniformBuffer,

			/// Read as a sampled image/uniform texel buffer in a vertex shader
			VertexShaderReadSampledImageOrUniformTexelBuffer,

			/// Read as any other resource in a vertex shader
			VertexShaderReadOther,

			/// Read as a uniform buffer in a tessellation control shader
			TessellationControlShaderReadUniformBuffer,

			/// Read as a sampled image/uniform texel buffer in a tessellation control shader
			TessellationControlShaderReadSampledImageOrUniformTexelBuffer,

			/// Read as any other resource in a tessellation control shader
			TessellationControlShaderReadOther,

			/// Read as a uniform buffer in a tessellation evaluation shader
			TessellationEvaluationShaderReadUniformBuffer,

			/// Read as a sampled image/uniform texel buffer in a tessellation evaluation shader
			TessellationEvaluationShaderReadSampledImageOrUniformTexelBuffer,

			/// Read as any other resource in a tessellation evaluation shader
			TessellationEvaluationShaderReadOther,

			/// Read as a uniform buffer in a geometry shader
			GeometryShaderReadUniformBuffer,

			/// Read as a sampled image/uniform texel buffer in a geometry shader
			GeometryShaderReadSampledImageOrUniformTexelBuffer,

			/// Read as any other resource in a geometry shader
			GeometryShaderReadOther,

			/// Read as a uniform buffer in a fragment shader
			FragmentShaderReadUniformBuffer,

			/// Read as a sampled image/uniform texel buffer in a fragment shader
			FragmentShaderReadSampledImageOrUniformTexelBuffer,

			/// Read as an input attachment with a color format in a fragment shader
			FragmentShaderReadColorInputAttachment,

			/// Read as an input attachment with a depth/stencil format in a fragment shader
			FragmentShaderReadDepthStencilInputAttachment,

			/// Read as any other resource in a fragment shader
			FragmentShaderReadOther,

			/// Read by blending/logic operations or subpass load operations
			ColorAttachmentRead,

			/// Read by depth/stencil tests or subpass load operations
			DepthStencilAttachmentRead,

			/// Read as a uniform buffer in a compute shader
			ComputeShaderReadUniformBuffer,

			/// Read as a sampled image/uniform texel buffer in a compute shader
			ComputeShaderReadSampledImageOrUniformTexelBuffer,

			/// Read as any other resource in a compute shader
			ComputeShaderReadOther,

			/// Read as a uniform buffer in any shader
			AnyShaderReadUniformBuffer,

			/// Read as a uniform buffer in any shader, or a vertex buffer
			AnyShaderReadUniformBufferOrVertexBuffer,

			/// Read as a sampled image in any shader
			AnyShaderReadSampledImageOrUniformTexelBuffer,

			/// Read as any other resource (excluding attachments) in any shader
			AnyShaderReadOther,

			/// Read as the source of a transfer operation
			TransferRead,

			/// Read on the host
			HostRead,

			/// Read by the presentation engine (i.e. `vkQueuePresentKHR`)
			Present,

			/// Command buffer write operation as defined by `NVX_device_generated_commands`
			CommandBufferWriteNVX,

			/// Written as any resource in a vertex shader
			VertexShaderWrite,

			/// Written as any resource in a tessellation control shader
			TessellationControlShaderWrite,

			/// Written as any resource in a tessellation evaluation shader
			TessellationEvaluationShaderWrite,

			/// Written as any resource in a geometry shader
			GeometryShaderWrite,

			/// Written as any resource in a fragment shader
			FragmentShaderWrite,

			/// Written as a color attachment during rendering, or via a subpass store op
			ColorAttachmentWrite,

			/// Written as a depth/stencil attachment during rendering, or via a subpass store op
			DepthStencilAttachmentWrite,

			/// Written as a depth aspect of a depth/stencil attachment during rendering, whilst the
			/// stencil aspect is read-only. Requires `VK_KHR_maintenance2` to be enabled.
			DepthAttachmentWriteStencilReadOnly,

			/// Written as a stencil aspect of a depth/stencil attachment during rendering, whilst the
			/// depth aspect is read-only. Requires `VK_KHR_maintenance2` to be enabled.
			StencilAttachmentWriteDepthReadOnly,

			/// Written as any resource in a compute shader
			ComputeShaderWrite,

			/// Written as any resource in any shader
			AnyShaderWrite,

			/// Written as the destination of a transfer operation
			TransferWrite,

			/// Written on the host
			HostWrite,

			/// Read or written as a color attachment during rendering
			ColorAttachmentReadWrite,

			/// Covers any access - useful for debug, generally avoid for performance reasons
			General,
		};

        struct GpuResource
        {
			virtual ~GpuResource();
			enum class Type
			{
				Buffer,
				Texture,
				RayTracingAcceleration,
				Unkown
			}ty = Type::Unkown;

			constexpr bool is_texture() const { return ty == Type::Texture; }
			constexpr bool is_buffer() const { return ty == Type::Buffer; }
			constexpr bool is_acceleration_structure() const { return ty == Type::RayTracingAcceleration; }
			bool is_signaled() { return release_flag[0] && release_flag[1]; }
			//2 device frame flag
			bool release_flag[2] = {false};
        };
    }
}

//template<>
//struct fmt::formatter<diverse::rhi::AccessType> : fmt::formatter<std::string>
//{
//	
//};


template<>
struct fmt::formatter<diverse::PixelFormat> : fmt::formatter<std::string>
{
	auto format(diverse::PixelFormat v, format_context& ctx) const -> decltype(ctx.out())
	{
		std::string str;
		switch (v)
		{
		case diverse::PixelFormat::Unknown:
			str = "PixelFormat::Unknown";
			break;
		case diverse::PixelFormat::R32G32B32A32_Typeless:
			str = "PixelFormat::R32G32B32A32_Typeless";
			break;
		case diverse::PixelFormat::R32G32B32A32_Float:
			str = "PixelFormat::R32G32B32A32_Float";
			break;
		case diverse::PixelFormat::R32G32B32A32_UInt:
			str = "PixelFormat::R32G32B32A32_UInt";
			break;
		case diverse::PixelFormat::R32G32B32A32_SInt:
			str = "PixelFormat::R32G32B32A32_SInt";
			break;
		case diverse::PixelFormat::R32G32B32_Typeless:
			str = "PixelFormat::R32G32B32_Typeless";
			break;
		case diverse::PixelFormat::R32G32B32_Float:
			str = "PixelFormat::R32G32B32_Float";
			break;
		case diverse::PixelFormat::R32G32B32_UInt:
			str = "PixelFormat::R32G32B32_UInt";
			break;
		case diverse::PixelFormat::R32G32B32_SInt:
			str = "PixelFormat::R32G32B32_SInt";
			break;
		case diverse::PixelFormat::R16G16B16A16_Typeless:
			str = "PixelFormat::R16G16B16A16_Typeless";
			break;
		case diverse::PixelFormat::R16G16B16A16_Float:
			str = "PixelFormat::R16G16B16A16_Float";
			break;
		case diverse::PixelFormat::R16G16B16A16_UNorm:
			str = "PixelFormat::R16G16B16A16_UNorm";
			break;
		case diverse::PixelFormat::R16G16B16A16_UInt:
			str = "PixelFormat::R16G16B16A16_UInt";
			break;
		case diverse::PixelFormat::R16G16B16A16_SNorm:
			str = "PixelFormat::R16G16B16A16_SNorm";
			break;
		case diverse::PixelFormat::R16G16B16A16_SInt:
			str = "PixelFormat::R16G16B16A16_SInt";
			break;
		case diverse::PixelFormat::R32G32_Typeless:
			str = "PixelFormat::R32G32_Typeless";
			break;
		case diverse::PixelFormat::R32G32_Float:
			str = "PixelFormat::R32G32_Float";
			break;
		case diverse::PixelFormat::R32G32_UInt:
			str = "PixelFormat::R32G32_UInt";
			break;
		case diverse::PixelFormat::R32G32_SInt:
			str = "PixelFormat::R32G32_SInt";
			break;
		case diverse::PixelFormat::R32G8X24_Typeless:
			str = "PixelFormat::R32G8X24_Typeless";
			break;
		case diverse::PixelFormat::D32_Float_S8X24_UInt:
			str = "PixelFormat::D32_Float_S8X24_UInt";
			break;
		case diverse::PixelFormat::R32_Float_X8X24_Typeless:
			str = "PixelFormat::R32_Float_X8X24_Typeless";
			break;
		case diverse::PixelFormat::X32_Typeless_G8X24_UInt:
			str = "PixelFormat::X32_Typeless_G8X24_UInt";
			break;
		case diverse::PixelFormat::R10G10B10A2_Typeless:
			str = "PixelFormat::R10G10B10A2_Typeless";
			break;
		case diverse::PixelFormat::R10G10B10A2_UNorm:
			str = "PixelFormat::R10G10B10A2_UNorm";
			break;
		case diverse::PixelFormat::R10G10B10A2_UInt:
			str = "PixelFormat::R10G10B10A2_UInt";
			break;
		case diverse::PixelFormat::R11G11B10_Float:
			str = "PixelFormat::R11G11B10_Float";
			break;
		case diverse::PixelFormat::R8G8B8A8_Typeless:
			str = "PixelFormat::R8G8B8A8_Typeless";
			break;
		case diverse::PixelFormat::R8G8B8A8_UNorm:
			str = "PixelFormat::R8G8B8A8_UNorm";
			break;
		case diverse::PixelFormat::R8G8B8A8_UNorm_sRGB:
			str = "PixelFormat::R8G8B8A8_UNorm_sRGB";
			break;
		case diverse::PixelFormat::R8G8B8A8_UInt:
			str = "PixelFormat::R8G8B8A8_UInt";
			break;
		case diverse::PixelFormat::R8G8B8A8_SNorm:
			str = "PixelFormat::R8G8B8A8_SNorm";
			break;
		case diverse::PixelFormat::R8G8B8A8_SInt:
			str = "PixelFormat::R8G8B8A8_SInt";
			break;
		case diverse::PixelFormat::R16G16_Typeless:
			str = "PixelFormat::R16G16_Typeless";
			break;
		case diverse::PixelFormat::R16G16_Float:
			str = "PixelFormat::R16G16_Float";
			break;
		case diverse::PixelFormat::R16G16_UNorm:
			str = "PixelFormat::R16G16_UNorm";
			break;
		case diverse::PixelFormat::R16G16_UInt:
			str = "PixelFormat::R16G16_UInt";
			break;
		case diverse::PixelFormat::R16G16_SNorm:
			str = "PixelFormat::R16G16_SNorm";
			break;
		case diverse::PixelFormat::R16G16_SInt:
			str = "PixelFormat::R16G16_SInt";
			break;
		case diverse::PixelFormat::R32_Typeless:
			str = "PixelFormat::R32_Typeless";
			break;
		case diverse::PixelFormat::D32_Float:
			str = "PixelFormat::D32_Float";
			break;
		case diverse::PixelFormat::R32_Float:
			str = "PixelFormat::R32_Float";
			break;
		case diverse::PixelFormat::R32_UInt:
			str = "PixelFormat::R32_UInt";
			break;
		case diverse::PixelFormat::R32_SInt:
			str = "PixelFormat::R32_SInt";
			break;
		case diverse::PixelFormat::R24G8_Typeless:
			str = "PixelFormat::R24G8_Typeless";
			break;
		case diverse::PixelFormat::D24_UNorm_S8_UInt:
			str = "PixelFormat::D24_UNorm_S8_UInt";
			break;
		case diverse::PixelFormat::R24_UNorm_X8_Typeless:
			str = "PixelFormat::R24_UNorm_X8_Typeless";
			break;
		case diverse::PixelFormat::X24_Typeless_G8_UInt:
			str = "PixelFormat::X24_Typeless_G8_UInt";
			break;
		case diverse::PixelFormat::R8G8_Typeless:
			str = "PixelFormat::R8G8_Typeless";
			break;
		case diverse::PixelFormat::R8G8_UNorm:
			str = "PixelFormat::R8G8_UNorm";
			break;
		case diverse::PixelFormat::R8G8_UInt:
			str = "PixelFormat::R8G8_UInt";
			break;
		case diverse::PixelFormat::R8G8_SNorm:
			str = "PixelFormat::R8G8_SNorm";
			break;
		case diverse::PixelFormat::R8G8_SInt:
			str = "PixelFormat::R8G8_SInt";
			break;
		case diverse::PixelFormat::R16_Typeless:
			str = "PixelFormat::R16_Typeless";
			break;
		case diverse::PixelFormat::R16_Float:
			str = "PixelFormat::R16_Float";
			break;
		case diverse::PixelFormat::D16_UNorm:
			str = "PixelFormat::D16_UNorm";
			break;
		case diverse::PixelFormat::R16_UNorm:
			str = "PixelFormat::R16_UNorm";
			break;
		case diverse::PixelFormat::R16_UInt:
			str = "PixelFormat::R16_UInt";
			break;
		case diverse::PixelFormat::R16_SNorm:
			str = "PixelFormat::R16_SNorm";
			break;
		case diverse::PixelFormat::R16_SInt:
			str = "PixelFormat::R16_SInt";
			break;
		case diverse::PixelFormat::R8_Typeless:
			str = "PixelFormat::R8_Typeless";
			break;
		case diverse::PixelFormat::R8_UNorm:
			str = "PixelFormat::R8_UNorm";
			break;
		case diverse::PixelFormat::R8_UInt:
			str = "PixelFormat::R8_UInt";
			break;
		case diverse::PixelFormat::R8_SNorm:
			str = "PixelFormat::R8_SNorm";
			break;
		case diverse::PixelFormat::R8_SInt:
			str = "PixelFormat::R8_SInt";
			break;
		case diverse::PixelFormat::A8_UNorm:
			str = "PixelFormat::A8_UNorm";
			break;
		case diverse::PixelFormat::R1_UNorm:
			str = "PixelFormat::R1_UNorm";
			break;
		case diverse::PixelFormat::R9G9B9E5_SharedExp:
			str = "PixelFormat::R9G9B9E5_SharedExp";
			break;
		case diverse::PixelFormat::R8G8_B8G8_UNorm:
			str = "PixelFormat::R8G8_B8G8_UNorm";
			break;
		case diverse::PixelFormat::G8R8_G8B8_UNorm:
			str = "PixelFormat::G8R8_G8B8_UNorm";
			break;
		case diverse::PixelFormat::BC1_Typeless:
			str = "PixelFormat::BC1_Typeless";
			break;
		case diverse::PixelFormat::BC1_UNorm:
			str = "PixelFormat::BC1_UNorm";
			break;
		case diverse::PixelFormat::BC1_UNorm_sRGB:
			str = "PixelFormat::BC1_UNorm_sRGB";
			break;
		case diverse::PixelFormat::BC2_Typeless:
			str = "PixelFormat::BC2_Typeless";
			break;
		case diverse::PixelFormat::BC2_UNorm:
			str = "PixelFormat::BC2_UNorm";
			break;
		case diverse::PixelFormat::BC2_UNorm_sRGB:
			str = "PixelFormat::BC2_UNorm_sRGB";
			break;
		case diverse::PixelFormat::BC3_Typeless:
			str = "PixelFormat::BC3_Typeless";
			break;
		case diverse::PixelFormat::BC3_UNorm:
			str = "PixelFormat::BC3_UNorm";
			break;
		case diverse::PixelFormat::BC3_UNorm_sRGB:
			str = "PixelFormat::BC3_UNorm_sRGB";
			break;
		case diverse::PixelFormat::BC4_Typeless:
			str = "PixelFormat::BC4_Typeless";
			break;
		case diverse::PixelFormat::BC4_UNorm:
			str = "PixelFormat::BC4_UNorm";
			break;
		case diverse::PixelFormat::BC4_SNorm:
			str = "PixelFormat::BC4_SNorm";
			break;
		case diverse::PixelFormat::BC5_Typeless:
			str = "PixelFormat::BC5_Typeless";
			break;
		case diverse::PixelFormat::BC5_UNorm:
			str = "PixelFormat::BC5_UNorm";
			break;
		case diverse::PixelFormat::BC5_SNorm:
			str = "PixelFormat::BC5_SNorm";
			break;
		case diverse::PixelFormat::B5G6R5_UNorm:
			str = "PixelFormat::B5G6R5_UNorm";
			break;
		case diverse::PixelFormat::B5G5R5A1_UNorm:
			str = "PixelFormat::B5G5R5A1_UNorm";
			break;
		case diverse::PixelFormat::B8G8R8A8_UNorm:
			str = "PixelFormat::B8G8R8A8_UNorm";
			break;
		case diverse::PixelFormat::B8G8R8X8_UNorm:
			str = "PixelFormat::B8G8R8X8_UNorm";
			break;
		case diverse::PixelFormat::R10G10B10_Xr_Bias_A2_UNorm:
			str = "PixelFormat::R10G10B10_Xr_Bias_A2_UNorm";
			break;
		case diverse::PixelFormat::B8G8R8A8_Typeless:
			str = "PixelFormat::B8G8R8A8_Typeless";
			break;
		case diverse::PixelFormat::B8G8R8A8_UNorm_sRGB:
			str = "PixelFormat::B8G8R8A8_UNorm_sRGB";
			break;
		case diverse::PixelFormat::B8G8R8X8_Typeless:
			str = "PixelFormat::B8G8R8X8_Typeless";
			break;
		case diverse::PixelFormat::B8G8R8X8_UNorm_sRGB:
			str = "PixelFormat::B8G8R8X8_UNorm_sRGB";
			break;
		case diverse::PixelFormat::BC6H_Typeless:
			str = "PixelFormat::BC6H_Typeless";
			break;
		case diverse::PixelFormat::BC6H_Uf16:
			str = "PixelFormat::BC6H_Uf16";
			break;
		case diverse::PixelFormat::BC6H_Sf16:
			str = "PixelFormat::BC6H_Sf16";
			break;
		case diverse::PixelFormat::BC7_Typeless:
			str = "PixelFormat::BC7_Typeless";
			break;
		case diverse::PixelFormat::BC7_UNorm:
			str = "PixelFormat::BC7_UNorm";
			break;
		case diverse::PixelFormat::BC7_UNorm_sRGB:
			str = "PixelFormat::BC7_UNorm_sRGB";
			break;
		case diverse::PixelFormat::MAX:
			break;
		default:
			break;
		}

		return fmt::format_to(ctx.out(), "{}", str);
	}
};