#pragma once
#include "backend/drs_rhi/gpu_swapchain.h"
#include "dvs_metal_utils.h"
//#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
namespace diverse
{
    namespace rhi
    {
        struct SwapchainMetal : public Swapchain
        {
           	SwapchainMetal(struct GpuDeviceMetal& device,SwapchainDesc desc,void* window);
			~SwapchainMetal();
			void	init(SwapchainDesc desc,void* window_handle);
			void	resize(u32 width,u32 height) override;
            auto acquire_next_image()-> SwapchainImage override;
            auto present_image(const SwapchainImage& swap_chain,struct CommandBuffer* present_cb)->void override;
            auto current_buffer_index()->u64 override { return (u64)next_img;}
            auto current_frame_index()->u64 override {return current_frame_id;}
            auto reset_frame_index() -> void override {current_frame_id = 1;}
            auto back_buffer_count()->u32 {return images.size();}
        public:
            CAMetalLayer *metalLayer;
            struct GpuDeviceMetal& device;
            u32 next_img = 0;
            std::vector<std::shared_ptr<GpuTexture>>	images;
            u64                current_frame_id = 0;
        };
    }
}
