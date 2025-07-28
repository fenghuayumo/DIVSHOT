#pragma once
#include <maths/transform.h>
#include <maths/bounding_box.h>
#include <maths/bounding_sphere.h>
#include <vector>

namespace cv
{
   class Mat;
}

namespace diverse
{
    namespace rhi
    {
        struct GpuBuffer;
        struct GpuTexture;
    };

    class BrushTool
    {
    public:
        BrushTool() = default;
        ~BrushTool() = default;

    public:
        auto brush_thick_ness()-> uint& { return brush_thickness; }
        auto brush_points()-> std::vector<glm::ivec2>& { return points; }
        auto brush_buffer()-> std::shared_ptr<rhi::GpuBuffer> { return brush_img_buffer; }
        auto brush_texture()-> std::shared_ptr<rhi::GpuTexture> { return brush_image; }
        auto brush_mask()-> std::shared_ptr<cv::Mat> {return image_mask;}
        auto update_brush_buffer() -> void;
        auto create_brush_buffer(u32 width, u32 height) -> void;
        auto create_pick_buffer(u32 width, u32 height) -> std::shared_ptr<rhi::GpuBuffer>;
        auto reset_brush() -> void;

        uint brush_thickness = 60;
    private:
        std::vector<glm::ivec2>  points;

        std::shared_ptr<rhi::GpuBuffer> brush_pick_buffer;
        std::shared_ptr<rhi::GpuBuffer> brush_img_buffer;
        std::shared_ptr<rhi::GpuTexture>  brush_image;  
        std::shared_ptr<cv::Mat>    image_mask;
    };
}