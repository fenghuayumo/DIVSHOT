#ifdef DS_RENDER_API_METAL
#include "gpu_texture_metal.h"
#include "gpu_device_metal.h"
namespace diverse
{
    namespace rhi
    {
        MTLPixelFormat metal_pixel_formats[] = {
            
        };

        GpuTextureViewMetal::GpuTextureViewMetal(const GpuTextureViewDesc& view_desc, const GpuTextureDesc& desc, id<MTLTexture> tex)
            :id_tex(tex)
        {

        }
        GpuTextureViewMetal::~GpuTextureViewMetal()
        {

        }

        auto GpuTextureMetal::view(const struct GpuDevice* device, const GpuTextureViewDesc& view_desc) -> std::shared_ptr<GpuTextureView>
        {
            auto metal_device = dynamic_cast<const GpuDeviceMetal*>(device);
            auto it = views.find(view_desc);
            if (it == views.end())
            {
                auto img_view = std::make_shared<GpuTextureViewMetal>(view_desc, desc, id_tex);//metal_device->create_image_view(view_desc, desc, image);
                views[view_desc] = std::move(img_view);
            }
            return views[view_desc];
        }
        auto texel_address_mode_2_metal(const rhi::SamplerAddressMode& mode)->MTLSamplerAddressMode
        {
            switch (mode)
            {
            case SamplerAddressMode::REPEAT:
                return MTLSamplerAddressModeRepeat;
            case SamplerAddressMode::MIRRORED_REPEAT:
                return MTLSamplerAddressModeMirrorRepeat;
            case SamplerAddressMode::CLAMP_TO_EDGE:
                return MTLSamplerAddressModeClampToEdge;
            case SamplerAddressMode::CLAMP_TO_BORDER:
                return MTLSamplerAddressModeClampToZero;
            case SamplerAddressMode::MIRROR_CLAMP_TO_EDGE:
                return MTLSamplerAddressModeMirrorClampToEdge;
            default:
                break;
            }
            return MTLSamplerAddressModeRepeat;
        }
        auto texel_filter_mode_2_metal(const TexelFilter& filter_mode) -> MTLSamplerMinMagFilter
        {
            switch (filter_mode)
            {
            case TexelFilter::NEAREST:
                return MTLSamplerMinMagFilterNearest;
            case TexelFilter::LINEAR:
                return MTLSamplerMinMagFilterLinear;
            default:
                break;
            }
            return MTLSamplerMinMagFilterNearest;
        }
    
        auto texture_type_to_metal(const rhi::TextureType& tex_type)->MTLTextureType
        {
            switch (tex_type)
            {
            case rhi::TextureType::Tex1d:
                return MTLTextureType1D;
            case rhi::TextureType::Tex2d:
                return MTLTextureType2D;
            case rhi::TextureType::Tex3d:
                return MTLTextureType3D;
            case rhi::TextureType::Cube:
                return MTLTextureTypeCube;
            case rhi::TextureType::Tex1dArray:
                return MTLTextureType1DArray;
            case rhi::TextureType::Tex2dArray:
                return MTLTextureType2DArray;
            case rhi::TextureType::CubeArray:
                return MTLTextureTypeCubeArray;
            default:
                break;
            }
            return MTLTextureType2D;
        }
    
        auto texture_usage_2_metal(TextureUsageFlags flags) -> MTLTextureUsage
        {
            MTLTextureUsage usage = MTLTextureUsageUnknown;

            if( flags & TextureUsageFlags::SAMPLED)
            {
                usage |= MTLTextureUsageShaderRead;
            }
            if( flags & TextureUsageFlags::STORAGE)
            {
                usage |= MTLTextureUsageShaderWrite;
            }
            if( flags & TextureUsageFlags::COLOR_ATTACHMENT)
            {
                usage |= MTLTextureUsageRenderTarget;
            }
            if( flags & TextureUsageFlags::DEPTH_STENCIL_ATTACHMENT)
            {
                usage |= MTLTextureUsageRenderTarget;
            }
            return usage;
        }

        auto pixel_format_2_metal(PixelFormat pxiel_format) -> MTLPixelFormat
        {
            switch (pxiel_format)
            {
            case PixelFormat::B8G8R8A8_UNorm:
                  return  MTLPixelFormatBGRA8Unorm;
            case PixelFormat::R8G8B8A8_UNorm:
                return MTLPixelFormatRGBA8Unorm;
            case PixelFormat::R8G8B8A8_UNorm_sRGB:
                return MTLPixelFormatRGBA8Unorm_sRGB;
            case PixelFormat::R8G8B8A8_UInt:
                return MTLPixelFormatRGBA8Uint;
            case PixelFormat::R8G8B8A8_SInt:
                return MTLPixelFormatRGBA8Sint;
                case PixelFormat::R32G32B32A32_Float:
                case PixelFormat::R32G32B32A32_Typeless:
                return MTLPixelFormatRGBA32Float;
                case PixelFormat::R32G32B32A32_UInt:    
                return MTLPixelFormatRGBA32Uint;
                case PixelFormat::R32G32B32A32_SInt:
                return MTLPixelFormatRGBA32Sint;
                case PixelFormat::R32G32B32_Float:
                return MTLPixelFormatRGBA32Float;
                case PixelFormat::R32G32B32_UInt:
                return MTLPixelFormatRGBA32Uint;
                case PixelFormat::R32G32B32_SInt:
                return MTLPixelFormatRGBA32Sint;
                case PixelFormat::R16G16B16A16_Float:
                return MTLPixelFormatRGBA16Float;
                case PixelFormat::R16G16B16A16_UNorm:
                return MTLPixelFormatRGBA16Unorm;
                case PixelFormat::R16G16B16A16_UInt:
                return MTLPixelFormatRGBA16Uint;
                case PixelFormat::R16G16B16A16_SInt:
                return MTLPixelFormatRGBA16Sint;
                case PixelFormat::R16G16B16A16_SNorm:
                return MTLPixelFormatRGBA16Snorm;
                case PixelFormat::R32G32_Float:
                return MTLPixelFormatRG32Float;
                case PixelFormat::R32G32_UInt:
                return MTLPixelFormatRG32Uint;
                case PixelFormat::R32G32_SInt:
                return MTLPixelFormatRG32Sint;
                case PixelFormat::R10G10B10A2_UNorm:
                return MTLPixelFormatRGB10A2Unorm;
                case PixelFormat::R10G10B10A2_UInt:
                return MTLPixelFormatRGB10A2Uint;
                case PixelFormat::R11G11B10_Float:
                return MTLPixelFormatRGB9E5Float;
                case PixelFormat::R16G16_Float:
                return MTLPixelFormatRG16Float;
                case PixelFormat::R16G16_UNorm:
                return MTLPixelFormatRG16Unorm;
                case PixelFormat::R16G16_UInt:
                return MTLPixelFormatRG16Uint;
                case PixelFormat::R16G16_SInt:
                return MTLPixelFormatRG16Sint;
                case PixelFormat::R16G16_SNorm:
                return MTLPixelFormatRG16Snorm;
            case PixelFormat::R32_Float:
                return MTLPixelFormatR32Float;
                case PixelFormat::R32_UInt:
                return MTLPixelFormatR32Uint;
            case PixelFormat::R32_SInt:
                return MTLPixelFormatR32Sint;
            case PixelFormat::R8G8_UNorm:
                return MTLPixelFormatRG8Unorm;
            case PixelFormat::R8G8_UInt:
                return MTLPixelFormatRG8Uint;
                case PixelFormat::R8G8_SInt:
                return MTLPixelFormatRG8Sint;
                case PixelFormat::R8G8_SNorm:
                return MTLPixelFormatRG8Snorm;
            case PixelFormat::R16_Float:
                return MTLPixelFormatR16Float;
                case PixelFormat::R16_UNorm:
                return MTLPixelFormatR16Unorm;
                case PixelFormat::R16_UInt:
                return MTLPixelFormatR16Uint;
            case PixelFormat::R16_SInt:
                return MTLPixelFormatR16Sint;
                case PixelFormat::R16_SNorm:
                return MTLPixelFormatR16Snorm;
                case PixelFormat::R8_UNorm:
                return MTLPixelFormatR8Unorm;
                case PixelFormat::R8_UInt:
                return MTLPixelFormatR8Uint;
                case PixelFormat::R8_SInt:
                return MTLPixelFormatR8Sint;
            case PixelFormat::R8_SNorm:
                return MTLPixelFormatR8Snorm;
            case PixelFormat::D32_Float:
                return MTLPixelFormatDepth32Float;
                case PixelFormat::D24_UNorm_S8_UInt:
                return MTLPixelFormatDepth24Unorm_Stencil8;
            case PixelFormat::D32_Float_S8X24_UInt:
                return MTLPixelFormatDepth32Float_Stencil8;
                case PixelFormat::BC1_UNorm:
                return MTLPixelFormatBC1_RGBA;
            case PixelFormat::BC1_UNorm_sRGB:
                return MTLPixelFormatBC1_RGBA_sRGB;
            case PixelFormat::BC2_UNorm:
                return MTLPixelFormatBC2_RGBA;
            case PixelFormat::BC2_UNorm_sRGB:
                return MTLPixelFormatBC2_RGBA_sRGB;
            case PixelFormat::BC3_UNorm:
                return MTLPixelFormatBC3_RGBA;
            case PixelFormat::BC3_UNorm_sRGB:
                return MTLPixelFormatBC3_RGBA_sRGB;
            case PixelFormat::BC4_UNorm:
                return MTLPixelFormatBC4_RUnorm;
            case PixelFormat::BC4_SNorm:
                return MTLPixelFormatBC4_RSnorm;
            case PixelFormat::BC5_UNorm:
                return MTLPixelFormatBC5_RGUnorm;
            case PixelFormat::BC5_SNorm:
                return MTLPixelFormatBC5_RGSnorm;
            case PixelFormat::BC6H_Uf16:
                return MTLPixelFormatBC6H_RGBUfloat;
            case PixelFormat::BC6H_Sf16:
                return MTLPixelFormatBC6H_RGBFloat;
            case PixelFormat::BC7_UNorm:
                return MTLPixelFormatBC7_RGBAUnorm;
            default:
                break;
            }
        }
    }
}
#endif