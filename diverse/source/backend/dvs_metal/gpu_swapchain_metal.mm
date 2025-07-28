#ifdef DS_RENDER_API_METAL
#include "gpu_swapchain_metal.h"
#include "gpu_device_metal.h"
#include "core/os/window.h"
#undef _GLFW_REQUIRE_LOADER
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
namespace diverse
{
    namespace rhi
    {
      
        SwapchainMetal::SwapchainMetal(struct GpuDeviceMetal& dev,SwapchainDesc desc,void* window_handle)
            : device(dev)
        {
            this->desc = desc;
            NSWindow *nswin = glfwGetCocoaWindow(static_cast<GLFWwindow*>(((Window*)window_handle)->get_handle()));
            metalLayer = [CAMetalLayer layer];
            metalLayer.device = device.device;
            metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
            nswin.contentView.layer = metalLayer;
            nswin.contentView.wantsLayer = YES;
            init(desc, window_handle);
        }
        SwapchainMetal::~SwapchainMetal()
        {
        }

        void SwapchainMetal::init(SwapchainDesc desc,void* window)
        {
            next_img = 0;
            metalLayer.framebufferOnly = NO;
//            metalLayer.contentsScale = [UIScreen mainScreen].scale;
            metalLayer.drawableSize = CGSizeMake(desc.dims[0], desc.dims[1]);
            const u32 image_count = metalLayer.maximumDrawableCount;
            images.resize(image_count);
            for (auto i=0; i<1;i++) {
//                auto image = std::make_shared<GpuTextureMetal>();
                auto imagedesc = GpuTextureDesc::new_2d(PixelFormat::B8G8R8A8_UNorm, { desc.dims[0], desc.dims[1] })
                    .with_usage(TextureUsageFlags::STORAGE)
                    .with_mip_levels(1)
                    .with_array_elements(1);
//                image->desc = imagedesc;
                 auto image = device.create_texture(imagedesc,{},fmt::format("swapchian_img{}", i).c_str());
//                 device.set_name(image.get(), fmt::format("swapchian_img{}", images.size()).c_str());
                images[i] = image;
            }
        }
    
        auto SwapchainMetal::acquire_next_image()-> SwapchainImage
        {
            auto swapchain_img =  SwapchainImage{
                images[0],
                next_img
            };
            next_img = (next_img + 1) % images.size();
            return swapchain_img;
        }
    
        auto SwapchainMetal::present_image(const SwapchainImage& swap_chain,struct CommandBuffer* present_cb)->void
        {
            auto source_tex = dynamic_cast<rhi::GpuTextureMetal*>(swap_chain.image.get())->id_tex;
            @autoreleasepool
            {
               id<CAMetalDrawable> drawable = [metalLayer nextDrawable];
                if( drawable != nil)
                {
                    auto metal_cmd = dynamic_cast<GpuCommandBufferMetal*>(present_cb);
                    
                    auto blitEncoder =  [metal_cmd->cmd_buf blitCommandEncoder];
                    blitEncoder.label = @"blitTexture";
                    [blitEncoder copyFromTexture:source_tex toTexture:drawable.texture];
                    [blitEncoder endEncoding];
                    
                    [metal_cmd->cmd_buf presentDrawable:drawable];
                    [metal_cmd->cmd_buf commit];
                }
            }
        }
    
        void  SwapchainMetal::resize(u32 width,u32 height)
        {
            if( desc.dims[0] == width && desc.dims[1] == height) return;
            
            // device.waitIdle();
             desc.dims = {width,height};
             init(desc, nullptr);
        }
    }
}
#endif