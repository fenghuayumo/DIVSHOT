#pragma once
#include "image_op.h"
#include "brush_tool.h"
#include <utility/thread_pool.h>
#include <utility/singleton.h>

namespace diverse
{
    namespace rhi
    {
        struct GpuTexture;
    }

    class ImagePaint : public ThreadSafeSingleton<ImagePaint>
    {
    public:
        void set_paint_texture(const std::shared_ptr<rhi::GpuTexture>& tex) 
        {
            paint_texture = tex; 
        }
        auto get_paint_texture()->std::shared_ptr<rhi::GpuTexture>& {return paint_texture;}
    public:
        auto image_edit_type()->ImageEditOpType& {return edit_type;}
        auto brush()->BrushTool& {return brush_tool;}
    protected:
        std::shared_ptr<rhi::GpuTexture>    paint_texture;
        ImageEditOpType edit_type = ImageEditOpType::None;
        BrushTool brush_tool;
    };

}