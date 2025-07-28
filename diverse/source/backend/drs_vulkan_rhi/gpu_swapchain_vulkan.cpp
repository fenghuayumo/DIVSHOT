
#include "gpu_swapchain_vulkan.h"
#include "gpu_device_vulkan.h"
#include "vk_surface.h"
#include "gpu_texture_vulkan.h"
#include "core/ds_log.h"

namespace diverse
{
    namespace rhi
    {
        auto select_surface_format(const std::vector<VkSurfaceFormatKHR>& formats) -> std::optional<VkSurfaceFormatKHR> 
        {
       
            auto preferred = VkSurfaceFormatKHR{
                VK_FORMAT_B8G8R8A8_UNORM,
               VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            };

            auto it = std::find_if(formats.begin(), formats.end(), [&](const VkSurfaceFormatKHR& fmts){return fmts.format == preferred.format; });
            if (it != formats.end()) {
                return preferred;
            }
            return {};
        }
        auto SwapchainVulkan::enumerate_surface_formats(const GpuDeviceVulkan& device,
            const SurfaceVulkan& surface)
            -> std::vector<VkSurfaceFormatKHR>
        {
            uint32_t formatCount;
            auto res = vkGetPhysicalDeviceSurfaceFormatsKHR(device.physcial_device.handle, surface.surface, &formatCount, nullptr);
            assert(res == VK_SUCCESS);

            std::vector<VkSurfaceFormatKHR> swapchain_formats(formatCount);
            res = vkGetPhysicalDeviceSurfaceFormatsKHR(device.physcial_device.handle, surface.surface, &formatCount, swapchain_formats.data());
            assert(res == VK_SUCCESS);
            return swapchain_formats;
        }

        auto SwapchainVulkan::acquire_next_image()->SwapchainImage
        {
            auto acquire_semaphore = acquire_semaphores[next_semaphore];
            auto rendering_finished_semaphore = rendering_finished_semaphores[next_semaphore];
            uint32 present_index;
            auto res = vkAcquireNextImageKHR(device->device, swapchain, UINT64_MAX, acquire_semaphore, nullptr, &present_index);

            if (res == VK_SUCCESS)
            {
                next_semaphore = (next_semaphore + 1) % images.size();
                current_frame_id = current_frame_id + 1;
                return SwapchainImage{
                    images[present_index],
                    present_index
                };
            }
            else
            {
                if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
                {
                    //DS_LOG_ERROR("Could not acquire swapchain image");

                    DS_LOG_ERROR("Acquire Image result : {0}", res == VK_ERROR_OUT_OF_DATE_KHR ? "Out of Date" : "SubOptimal");

                    if (res == VK_ERROR_OUT_OF_DATE_KHR)
                    {
                        resize(desc.dims[0],desc.dims[1]);
                    }
                }
            }
            return SwapchainImage();
        }

        auto SwapchainVulkan::present_image(const SwapchainImage& image, CommandBuffer* present_cb) -> void
        {
            auto presentation_cb = dynamic_cast<GpuCommandBufferVulkan*>(present_cb);

            auto acquire_semaphore = acquire_semaphores[image.image_index];
            auto rendering_finished_semaphore = rendering_finished_semaphores[image.image_index];

            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT /*, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR*/ };
            VkSubmitInfo	submit_info = {};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.pCommandBuffers = &presentation_cb->handle;
            submit_info.commandBufferCount = 1;
            submit_info.pWaitSemaphores = &acquire_semaphore;
            submit_info.waitSemaphoreCount = 1;
            submit_info.pSignalSemaphores = &rendering_finished_semaphore;
            submit_info.signalSemaphoreCount = 1;
            submit_info.pWaitDstStageMask = waitStages;
            vkResetFences(device->device, 1, &presentation_cb->submit_done_fence);
            VK_CHECK_RESULT(vkQueueSubmit(device->universe_queue.queue, 1, &submit_info, presentation_cb->submit_done_fence));

            VkPresentInfoKHR    present_info = {};
            present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            present_info.pWaitSemaphores = &rendering_finished_semaphore;
            present_info.waitSemaphoreCount = 1;
            present_info.pSwapchains = &swapchain;
            present_info.pImageIndices = &image.image_index;
            present_info.swapchainCount = 1;
            auto res = vkQueuePresentKHR(device->universe_queue.queue, &present_info);
            if (res != VK_SUCCESS)
            {
                if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
                    DS_LOG_ERROR("Could not present image : ");
            }
        }

        SwapchainVulkan::SwapchainVulkan(GpuDeviceVulkan* dev,
                                    SwapchainDesc desc,
                                    void* window_handle)
            :device(dev)
        {
            this->desc = desc;
            init(desc, window_handle);
        }
        SwapchainVulkan::~SwapchainVulkan()
        {
            if(device)
            {
                vkDestroySwapchainKHR(device->device, swapchain, nullptr);
                swapchain = VK_NULL_HANDLE;
                
            }
        }

        void SwapchainVulkan::init(SwapchainDesc desc,
            void* window_handle)
        {
            if(!surface)
                surface.reset(new SurfaceVulkan(*device->instance, window_handle));
            
            VkBool32 bSupported = VK_FALSE;
            for (auto queue_fam_index = 0; queue_fam_index < device->physcial_device.queue_family.size(); queue_fam_index++) {
                auto queue_fam = device->physcial_device.queue_family[queue_fam_index];
                if (queue_fam.properties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    VkBool32 support_present = VK_FALSE;
                    vkGetPhysicalDeviceSurfaceSupportKHR(device->physcial_device.handle, queue_fam_index, surface->surface, &support_present);
                    if( support_present )
                    {
                        bSupported  = support_present;
                        break;;
                    }
                }
            }
            if (!bSupported) 
            {
                DS_LOG_ERROR("Present Queue not supported");
            }
            VkSurfaceCapabilitiesKHR surface_capabilities;
            auto res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physcial_device.handle, surface->surface, &surface_capabilities);
            assert(res == VK_SUCCESS);

            auto desired_image_count = std::max<uint32>(surface_capabilities.minImageCount, 3);
            if (surface_capabilities.maxImageCount != 0) {
                desired_image_count = std::min<uint32>(desired_image_count, surface_capabilities.maxImageCount);
            }
            DS_LOG_INFO("Swapchain image count: {}", desired_image_count);
            auto surface_resolution = desc.dims;
            if (0 == surface_resolution[0] || 0 == surface_resolution[1]) {
                assert(-1);
                return;
            }
            std::vector<VkPresentModeKHR> present_mode_preference;
            if (desc.vsync)
            {
                present_mode_preference.push_back(VkPresentModeKHR::VK_PRESENT_MODE_FIFO_RELAXED_KHR);
                present_mode_preference.push_back(VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR);
            }
            else
            {
                present_mode_preference.push_back(VkPresentModeKHR::VK_PRESENT_MODE_MAILBOX_KHR);
                present_mode_preference.push_back(VkPresentModeKHR::VK_PRESENT_MODE_IMMEDIATE_KHR);
            };


            uint32_t present_modecount;
            res = vkGetPhysicalDeviceSurfacePresentModesKHR(device->physcial_device.handle, surface->surface, &present_modecount, nullptr);
            assert(res == VK_SUCCESS);

            std::vector<VkPresentModeKHR> swapchain_presentModes(present_modecount);
            swapchain_presentModes.resize(present_modecount);
            res = vkGetPhysicalDeviceSurfacePresentModesKHR(device->physcial_device.handle, surface->surface, &present_modecount, swapchain_presentModes.data());
            assert(res == VK_SUCCESS);

            //std::find(present_mode_preference.begin(), present_mode_preference.end(), )
            auto present_mode = VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR;
            for (auto mode : present_mode_preference)
            {
                auto it = std::find(swapchain_presentModes.begin(), swapchain_presentModes.end(), mode);
                if (it != swapchain_presentModes.end())
                {
                    present_mode = mode;
                    break;
                }
            }
            DS_LOG_INFO("Presentation mode:  {}", present_mode);

            auto pre_transform = surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : surface_capabilities.currentTransform;
            auto surface_formats = enumerate_surface_formats(*device, *surface);

            VkSwapchainCreateInfoKHR createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            createInfo.surface = surface->surface;
            createInfo.minImageCount = desired_image_count;
            createInfo.imageFormat = desc.format == PixelFormat::Unknown ? select_surface_format(surface_formats)->format : pixel_format_2_vk(desc.format);
            createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            createInfo.imageExtent = {surface_resolution[0], surface_resolution [1]};
            createInfo.imageArrayLayers = 1;
            //createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            createInfo.imageUsage = VK_IMAGE_USAGE_STORAGE_BIT;
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            createInfo.preTransform = pre_transform;
            createInfo.presentMode = present_mode; // The only one that is always supported
            createInfo.pQueueFamilyIndices = VK_NULL_HANDLE;
            createInfo.clipped = VK_TRUE;
            createInfo.oldSwapchain = old_swapchain;
            VK_CHECK_RESULT(vkCreateSwapchainKHR(device->device, &createInfo, nullptr, &swapchain));

            if (old_swapchain != VK_NULL_HANDLE)
            {
                vkDestroySwapchainKHR(device->device, old_swapchain, nullptr);
            }
            old_swapchain = swapchain;
            uint32 image_count;
            res = vkGetSwapchainImagesKHR(device->device, swapchain, &image_count, nullptr);
            assert(res == VK_SUCCESS);
            std::vector<VkImage> vk_images;
            vk_images.resize(image_count);
            res = vkGetSwapchainImagesKHR(device->device, swapchain, &image_count, vk_images.data());
            assert(res == VK_SUCCESS);
            
            images.clear();

            for (auto vk_image : vk_images) {
                auto image = std::make_shared<GpuTextureVulkan>();
                image->image = vk_image;
                image->desc = GpuTextureDesc::new_2d(PixelFormat::B8G8R8A8_UNorm, { desc.dims[0], desc.dims[1] })
                    .with_usage(TextureUsageFlags::STORAGE)
                    .with_mip_levels(1)
                    .with_array_elements(1);
                image->b_swapchin = true;
                device->set_name(image.get(), fmt::format("swapchian_img{}", images.size()).c_str());
                images.push_back(image);
            }

            assert(desired_image_count == images.size());

            for (int i = 0; i < acquire_semaphores.size(); i++) {
                auto acquire_semaphore = acquire_semaphores[i];
                auto rendering_finished_semaphore = rendering_finished_semaphores[i];
                device->defer_release([=]()->void{
                    if (acquire_semaphore != nullptr)
                    {
                        vkDestroySemaphore(device->device, acquire_semaphore, nullptr);
                    }
                    if (rendering_finished_semaphore != nullptr)
                    {
                        vkDestroySemaphore(device->device, rendering_finished_semaphore, nullptr);
                    }
                });
            }

            acquire_semaphores.resize(images.size());
            rendering_finished_semaphores.resize(images.size());
            for (int i = 0; i < images.size(); i++) {
                VkSemaphoreCreateInfo   semaphore_create_info = {};
                semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
                vkCreateSemaphore(device->device, &semaphore_create_info, nullptr, &acquire_semaphores[i]);
                vkCreateSemaphore(device->device, &semaphore_create_info, nullptr, &rendering_finished_semaphores[i]);
            }

            next_semaphore = 0;
            current_frame_id = 0;
        }

        void SwapchainVulkan::resize(u32 width, u32 height)
        {
            if( desc.dims[0] == width && desc.dims[1] == height) return;

            vkDeviceWaitIdle(device->device);
            desc.dims = {width,height};
            init(desc, nullptr);
            vkDeviceWaitIdle(device->device);
        }

        std::array<uint32, 2> SwapchainVulkan::extent()
        {
            return { desc.dims[0],desc.dims[1]};
        }
    }
}
