#include "gpu_barrier.h"

namespace diverse
{
    namespace rhi
    {
        auto image_aspect_mask_from_format(PixelFormat format)-> ImageAspectFlags
        {
            switch (format)
            {
            case PixelFormat::D16_UNorm:
            case PixelFormat::D32_Float:
                return ImageAspectFlags::DEPTH;
            case PixelFormat::D24_UNorm_S8_UInt:
            case PixelFormat::D32_Float_S8X24_UInt:
                return ImageAspectFlags::DEPTH | ImageAspectFlags::STENCIL;
            default:
                return ImageAspectFlags::COLOR;
            }
            return ImageAspectFlags::COLOR;
        }

         auto image_aspect_mask_from_access_type_and_format(rhi::AccessType access_type, PixelFormat format)->std::optional<ImageAspectFlags>
         {
             switch ( access_type)
             {
             default:   
                 break;
             }
             return image_aspect_mask_from_format(format);
         }
    }
}