#pragma once

#include "backend/drs_rhi//gpu_swapchain.h"

#include "vk_utils.h"
#include "vk_surface.h"

namespace diverse
{
    namespace rhi
    {
		struct SwapchainVulkan : public Swapchain
		{
			VkSwapchainKHR	swapchain = VK_NULL_HANDLE;
			VkSwapchainKHR	old_swapchain = VK_NULL_HANDLE;
			//SwapchainDesc	desc;
			std::vector<std::shared_ptr<GpuTexture>>	images;
			std::vector<VkSemaphore> acquire_semaphores;

			std::vector< VkSemaphore>	rendering_finished_semaphores;
			u64			next_semaphore = 0;
			u64			current_frame_id = 0;
		public:
			SwapchainVulkan(struct GpuDeviceVulkan* device,SwapchainDesc desc,void* window);
			~SwapchainVulkan();
			void	init(SwapchainDesc desc,
					void* window_handle);
			void	resize(u32 width,u32 height) override;
			auto	current_buffer_index() -> u64 {return next_semaphore;}
			auto	current_frame_index()-> u64 { return current_frame_id;}
			auto 	reset_frame_index() -> void {current_frame_id = 1;}
			auto  acquire_next_image() -> SwapchainImage;
			auto  present_image(const SwapchainImage& image,CommandBuffer* present_cb) -> void;
		protected:
			struct GpuDeviceVulkan* device;
			std::unique_ptr<SurfaceVulkan>	surface;
		public:
			static auto  enumerate_surface_formats(const GpuDeviceVulkan& device, const SurfaceVulkan& surface) -> std::vector<VkSurfaceFormatKHR>;
			std::array<uint32, 2>	extent();
			VkSurfaceKHR	get_surface() {return surface->surface;}
		};
    }
}