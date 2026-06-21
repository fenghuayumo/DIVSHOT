#pragma once

#include "backend/drs_rhi//gpu_swapchain.h"

#include "vk_utils.h"
#include "vk_surface.h"
#include <mutex>

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
			u64			next_semaphore = 0;  // Index for cycling through acquire semaphores
			u64			current_frame_id = 0;
			u32			current_image_index = 0;  // Index of the currently acquired swapchain image
			u64			current_acquire_sem_index = 0;  // Index of acquire semaphore used in current frame
			bool		needs_recreate_ = false;
			std::mutex	host_mutex;
		public:
			SwapchainVulkan(struct GpuDeviceVulkan* device,SwapchainDesc desc,void* window);
			~SwapchainVulkan();
			void	init(SwapchainDesc desc,
					void* window_handle);
			void	resize(u32 width,u32 height, bool force = false) override;
			void	recreate();
			auto	needs_recreate() const -> bool { return needs_recreate_; }
			auto	current_buffer_index() -> u64 {return next_semaphore;}
			auto	current_frame_index()-> u64 { return current_frame_id;}
			auto 	reset_frame_index() -> void {current_frame_id = 1;}
			auto  acquire_next_image() -> SwapchainImage;
			auto  present_image(const SwapchainImage& image,CommandBuffer* present_cb) -> void;
		protected:
			struct GpuDeviceVulkan* device;
			std::unique_ptr<SurfaceVulkan>	surface;
			auto query_surface_extent() -> std::array<u32, 2>;
		public:
			static auto  enumerate_surface_formats(const GpuDeviceVulkan& device, const SurfaceVulkan& surface) -> std::vector<VkSurfaceFormatKHR>;
			std::array<uint32, 2>	extent();
			VkSurfaceKHR	get_surface() {return surface->surface;}
		};
    }
}
