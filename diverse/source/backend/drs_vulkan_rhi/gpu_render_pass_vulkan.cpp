#include <unordered_map>
#include "gpu_render_pass_vulkan.h"
#include "gpu_device_vulkan.h"
#include "core/ds_log.h"
namespace diverse
{
    namespace rhi
    {
        FrameBufferCache::FrameBufferCache(VkRenderPass r_pass, const std::vector<RenderPassAttachmentDesc>& color_attachments, const std::optional<RenderPassAttachmentDesc>& depth_attachment)
            :render_pass(r_pass)
        {
            attachment_desc = color_attachments;
            if(depth_attachment.has_value())
                attachment_desc.push_back(depth_attachment.value());

            color_attachment_count = color_attachments.size();
        }
        FrameBufferCache::~FrameBufferCache()
        {
        }
        auto FrameBufferCache::get_or_create(const GpuDeviceVulkan& device, const FramebufferCacheKey& key) -> VkFramebuffer
        {
            auto it = entries.find(key);
            if (it != entries.end())
                return it->second;

            std::vector<VkFormat>	color_formats(attachment_desc.size());
            std::vector< VkFramebufferAttachmentImageInfoKHR> attachments;
            for (auto i = 0; i< attachment_desc.size(); i++)
            {
                color_formats[i] = (pixel_format_2_vk(attachment_desc[i].format));
                VkFramebufferAttachmentImageInfoKHR	frame_attachment_image_info = {};
                frame_attachment_image_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO_KHR;
                frame_attachment_image_info.width = key.dims[0];
                frame_attachment_image_info.height = key.dims[1];
                frame_attachment_image_info.flags = texture_create_flag_2_vk(key.attachments[i].second);
                frame_attachment_image_info.layerCount = 1;
                frame_attachment_image_info.pViewFormats = &color_formats[i];
                frame_attachment_image_info.viewFormatCount = 1;
                frame_attachment_image_info.usage = texture_usage_2_vk(key.attachments[i].first);
                attachments.push_back(frame_attachment_image_info);
            }

            VkFramebufferAttachmentsCreateInfoKHR	imageless_desc = {};
            imageless_desc.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO_KHR;
            imageless_desc.pAttachmentImageInfos = attachments.data();
            imageless_desc.attachmentImageInfoCount = attachments.size();
            
            VkFramebufferCreateInfo	fbo_des = {};
            fbo_des.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbo_des.flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT_KHR;
            fbo_des.width = key.dims[0];
            fbo_des.height = key.dims[1];
            fbo_des.layers = 1;
            fbo_des.pNext = &imageless_desc;
            fbo_des.attachmentCount = attachments.size();
            fbo_des.renderPass = render_pass;

            VkFramebuffer	frame_buffer;
            VK_CHECK_RESULT(vkCreateFramebuffer(device.device, &fbo_des, VK_NULL_HANDLE, &frame_buffer));

            entries[key] = frame_buffer;

            return frame_buffer;
        }
        RenderPassVulkan::~RenderPassVulkan()
        {
            auto device = dynamic_cast<GpuDeviceVulkan*>(get_global_device())->device;
            g_device->defer_release([render_pass = this->render_pass, framebuffer_cache = this->framebuffer_cache, device]() mutable{
                if (render_pass != VK_NULL_HANDLE)
                {  
                    vkDestroyRenderPass(device, render_pass,nullptr);
                    render_pass = VK_NULL_HANDLE;
                    for (auto& entry : framebuffer_cache.entries)
                    {
                        if (entry.second != VK_NULL_HANDLE)
                        {
                            vkDestroyFramebuffer(device, entry.second, nullptr);
                            entry.second = VK_NULL_HANDLE;
                        }
                    }
                }
            });
        }
        void RenderPassVulkan::begin_render_pass()
        {
        }
        void RenderPassVulkan::end_render_pass()
        {
        }
    }
}
