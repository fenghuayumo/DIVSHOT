#pragma once

#include "gpu_texture.h"
#include "gpu_buffer.h"
#include <optional>
namespace diverse
{
    namespace rhi
    {
        struct ImageBarrier
        {
            //uint64      image;
            rhi::GpuTexture* image;
			AccessType prev_access;
			AccessType next_access;
            ImageAspectFlags aspect_mask;
            bool       discard = false;

            auto with_discard(bool discard) -> ImageBarrier
            {
                this->discard = discard;
                return *this;
            }
        };

        struct BufferBarrier
        {
            rhi::GpuBuffer* buffer; 
            AccessType prev_access;
			AccessType next_access;
            u32 offset;
            u32 size;
        };
        //struct AcessInfo
        //{

        //};
        //struct GpuBarrier
        //{
        //    
        //};
        auto image_aspect_mask_from_format(PixelFormat format)-> ImageAspectFlags;
        auto image_aspect_mask_from_access_type_and_format(rhi::AccessType access_type, PixelFormat format)->std::optional<ImageAspectFlags>;

    }
}