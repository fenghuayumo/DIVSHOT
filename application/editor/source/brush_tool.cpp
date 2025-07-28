#include "brush_tool.h"
#include <backend/drs_rhi/gpu_device.h>
#include <opencv2/opencv.hpp>
#include <utility/thread_pool.h>
namespace diverse
{
    // BrushTool::BrushTool()
    // {
    //     brush_img_buffer = std::make_shared<rhi::GpuBuffer>();
    //     brush_image = std::make_shared<rhi::GpuTexture>();
    // }
    // BrushTool::~BrushTool()
    // {
    // }

    void BrushTool::update_brush_buffer()
    {
        if (brush_img_buffer)
        {
            auto device = get_global_device();
			auto dst = brush_img_buffer->map(device);
            memcpy(dst, image_mask->data, brush_img_buffer->desc.size);
            brush_img_buffer->unmap(device);

            device->copy_image(brush_img_buffer.get(), brush_image.get(), nullptr);
		}
    }

    void BrushTool::create_brush_buffer(u32 width,u32 height)
    {
        u64 buffer_size = width * height * sizeof(u32);
        if ( !brush_img_buffer || brush_img_buffer->desc.size < buffer_size)
        {
            //init image_mask size with width,height
            image_mask = std::make_shared<cv::Mat>(height, width, CV_8UC4, cv::Scalar(0));
            
            brush_img_buffer = g_device->create_buffer(rhi::GpuBufferDesc::new_cpu_to_gpu(buffer_size,
                                                         rhi::BufferUsageFlags::TRANSFER_DST | 
                                                         rhi::BufferUsageFlags::STORAGE_BUFFER | 
                                                         rhi::BufferUsageFlags::TRANSFER_SRC), 
                                                         "image_mask", (u8*)image_mask->data);
           std::vector<rhi::ImageSubData> subData = {rhi::ImageSubData{ (u8*)image_mask->data , (u32)buffer_size, (u32)(width * sizeof(u32)), (u32)buffer_size}};
           brush_image = g_device->create_texture(rhi::GpuTextureDesc::new_2d(
                                                  PixelFormat::R8G8B8A8_UNorm,{width,height})
                                                  .with_usage(rhi::TextureUsageFlags::SAMPLED | 
                                                  rhi::TextureUsageFlags::TRANSFER_DST |
                                                  rhi::TextureUsageFlags::TRANSFER_SRC),
                                                  subData,
                                                  "brushmask");
        }
    }

    auto BrushTool::create_pick_buffer(u32 width, u32 height) ->  std::shared_ptr<rhi::GpuBuffer>
    {
        u64 buffer_size = width * height * sizeof(u32);
        if (!brush_pick_buffer || brush_pick_buffer->desc.size < buffer_size)
        {
            std::vector<int> data(width * height, 0);
            brush_pick_buffer = g_device->create_buffer(rhi::GpuBufferDesc::new_cpu_to_gpu(buffer_size,
                rhi::BufferUsageFlags::TRANSFER_DST |
                rhi::BufferUsageFlags::STORAGE_BUFFER |
                rhi::BufferUsageFlags::TRANSFER_SRC),
                "pick_buffer", (u8*)data.data());
        }
        return brush_pick_buffer;
    }

    void BrushTool::reset_brush()
    {
        points.clear();
        image_mask->setTo(cv::Scalar(0, 0, 0, 0));
        if(brush_pick_buffer)
        {
            auto device = get_global_device();
            auto data = brush_pick_buffer->map(device);
            memset(data, 0, brush_pick_buffer->desc.size);
            brush_pick_buffer->unmap(device);
        }
    }
}