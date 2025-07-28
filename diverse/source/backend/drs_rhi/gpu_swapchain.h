#pragma once
#include "gpu_texture.h"

namespace diverse
{
    namespace rhi
    {

        struct SwapchainDesc
        {
            std::array<u32,2> dims;
            bool vsync;
            PixelFormat format;
        };
        struct SwapchainImage
        {
            std::shared_ptr<GpuTexture>	image;
            u32 image_index;
        };
        struct Swapchain
        {
            SwapchainDesc   desc;
            Swapchain(){}
            virtual auto acquire_next_image()-> SwapchainImage = 0;
            virtual auto present_image(const SwapchainImage& swap_chain,struct CommandBuffer* present_cb)->void = 0;
            virtual auto resize(u32 width, u32 height)->void = 0;

            virtual auto current_buffer_index()-> u64 = 0;
            virtual auto current_frame_index()->u64 = 0;
            virtual auto reset_frame_index()->void = 0;
        };
    }
}
