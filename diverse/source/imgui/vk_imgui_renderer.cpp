#include <string>
#include "core/core.h"
#include "core/profiler.h"
#include "vk_imgui_renderer.h"
#include "backend/drs_vulkan_rhi/gpu_device_vulkan.h"

#include <imgui/imgui.h>

#define IMGUI_IMPL_VULKAN_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#include <imgui/backends/imgui_impl_vulkan.h>
#include "core/ds_log.h"
static ImGui_ImplVulkanH_Window g_WindowData;
static VkAllocationCallbacks* g_Allocator = nullptr;
static VkDescriptorPool g_DescriptorPool[3] = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE };

static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    printf("VkResult %d\n", err);
    if (err < 0)
        abort();
}

namespace diverse
{

    VKIMGUIRenderer::VKIMGUIRenderer(rhi::GpuDevice* dev, rhi::Swapchain* swapchain)
        : IMGUIRenderer(dev, swapchain)
        , m_Framebuffers{}
        , m_Renderpass(nullptr)
    {
        DS_PROFILE_FUNCTION();
    }
    VKIMGUIRenderer::~VKIMGUIRenderer()
    {
        DS_PROFILE_FUNCTION();
        //TODO:
        auto device = dynamic_cast<rhi::GpuDeviceVulkan*>(get_global_device())->device;
        g_device->defer_release([render_pass = m_Renderpass, 
            frame0=m_Framebuffers[0],
            frame1=m_Framebuffers[1], 
            frame2=m_Framebuffers[2], 
            device]() mutable{
            vkDestroyRenderPass(device, render_pass, nullptr);
            vkDestroyFramebuffer(device, frame0, nullptr);
            vkDestroyFramebuffer(device, frame1, nullptr);
            vkDestroyFramebuffer(device, frame2, nullptr);
        });
        ImGui_ImplVulkan_Shutdown();
    }
    void VKIMGUIRenderer::setup_vulkan_window_data(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, u32 width, u32 height)
    {
        DS_PROFILE_FUNCTION();
        auto vkdevice = dynamic_cast<rhi::GpuDeviceVulkan*>(device);
        auto vkswapchain = dynamic_cast<rhi::SwapchainVulkan*>(swapchain);
        // Create Descriptor Pool
        for (int i = 0; i < vkswapchain->images.size(); i++)
        {
            VkDescriptorPoolSize pool_sizes[] = {
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_IMGUI_TEXTURES }
            };
            VkDescriptorPoolCreateInfo pool_info = {};
            pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
            pool_info.maxSets = MAX_IMGUI_TEXTURES * IM_ARRAYSIZE(pool_sizes);
            pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
            pool_info.pPoolSizes = pool_sizes;
            VkResult err = vkCreateDescriptorPool(vkdevice->device, &pool_info, g_Allocator, &g_DescriptorPool[i]);
            check_vk_result(err);
        }

        wd->Surface = surface;
        wd->ClearEnable = m_ClearScreen;

        wd->Swapchain = vkswapchain->swapchain;
        wd->Width = width;
        wd->Height = height;
        wd->ImageCount = static_cast<uint32_t>(vkswapchain->images.size());
        m_Renderpass = create_render_pass();
        wd->RenderPass = m_Renderpass;

        wd->Frames = (ImGui_ImplVulkanH_Frame*)IM_ALLOC(sizeof(ImGui_ImplVulkanH_Frame) * wd->ImageCount);
        wd->FrameSemaphores = (ImGui_ImplVulkanH_FrameSemaphores*)IM_ALLOC(sizeof(ImGui_ImplVulkanH_FrameSemaphores) * wd->ImageCount);
        memset(wd->Frames, 0, sizeof(wd->Frames[0]) * wd->ImageCount);
        // Create The Image Views
        {
            for (uint32_t i = 0; i < wd->ImageCount; i++)
            {
                auto scBuffer = dynamic_cast<rhi::GpuTextureVulkan*>(vkswapchain->images[i].get());
                wd->Frames[i].Backbuffer = scBuffer->image;
                wd->Frames[i].BackbufferView = static_cast<rhi::GpuTextureViewVulkan*>(scBuffer->view(vkdevice, rhi::GpuTextureViewDesc{}).get())->img_view;
            }
        }
        for (uint32_t i = 0; i < wd->ImageCount; i++)
        {
            auto tex = device->create_texture(rhi::GpuTextureDesc::new_2d(PixelFormat::R8G8B8A8_UNorm,
                            std::array<u32, 2>{width, height})
                            .with_usage(rhi::TextureUsageFlags::SAMPLED |
                            rhi::TextureUsageFlags::COLOR_ATTACHMENT), {},
                            std::format("ui_tex_{}", i).c_str());
            auto frame_buf = create_frame_buffer(m_Renderpass, tex);
            m_Framebuffers[i] = frame_buf;
            target_tex[i] = tex;
            wd->Frames[i].Framebuffer = frame_buf;
        }
    }


    void VKIMGUIRenderer::init()
    {
        DS_PROFILE_FUNCTION();
        m_CurrentTextureIDIndex = 0;

        auto vkdevice = dynamic_cast<rhi::GpuDeviceVulkan*>(device);
        auto vkswapchain = dynamic_cast<rhi::SwapchainVulkan*>(swapchain);
        int w, h;
        w = (int)m_Width;
        h = (int)m_Height;
        ImGui_ImplVulkanH_Window* wd = &g_WindowData;
        VkSurfaceKHR surface = vkswapchain->get_surface();
        setup_vulkan_window_data(wd, surface, w, h);

        // Setup Vulkan binding
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = vkdevice->instance->instance;
        init_info.PhysicalDevice = vkdevice->physcial_device.handle;
        init_info.Device = vkdevice->device;
        init_info.QueueFamily = vkdevice->universe_queue.family.index;
        init_info.Queue = vkdevice->universe_queue.queue;
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPools = g_DescriptorPool;
        init_info.Allocator = g_Allocator;
        init_info.CheckVkResultFn = NULL;
        init_info.MinImageCount = 2;
        init_info.ImageCount = (uint32_t)vkswapchain->images.size();
        ImGui_ImplVulkan_Init(&init_info, wd->RenderPass);

        rebuild_font_texture();
    }


    void VKIMGUIRenderer::new_frame()
    {
        auto vkdevice = dynamic_cast<rhi::GpuDeviceVulkan*>(device);
        auto vkswapchain = dynamic_cast<rhi::SwapchainVulkan*>(swapchain);
        //vkResetDescriptorPool(vkdevice->device, g_DescriptorPool[0], 0);
        //ImGui_ImplVulkan_ClearDescriptors(0);

        m_CurrentTextureIDIndex = 0;
    }

    void VKIMGUIRenderer::render(rhi::CommandBuffer* cmd_buf)
    {
        DS_PROFILE_FUNCTION();

        ImGui::Render();
        frame_render(&g_WindowData, cmd_buf);
    }

    VkRenderPass VKIMGUIRenderer::create_render_pass()
    {
        VkAttachmentDescription attachments = {};
        attachments.format = VK_FORMAT_R8G8B8A8_UNORM;
        attachments.samples = VK_SAMPLE_COUNT_1_BIT;
        attachments.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colourAttachmentReferences = {};
        colourAttachmentReferences.attachment = 0;
        colourAttachmentReferences.layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

        VkSubpassDependency dependencies = {};
        dependencies.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colourAttachmentReferences;

        VkRenderPassCreateInfo  renderpass_create_info = {};
        renderpass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderpass_create_info.attachmentCount = 1;
        renderpass_create_info.pAttachments = &attachments;
        renderpass_create_info.subpassCount = 1;
        renderpass_create_info.pSubpasses = &subpass;

        auto vkdevice = dynamic_cast<rhi::GpuDeviceVulkan*>(device);
        VkRenderPass    render_pass;
        vkCreateRenderPass(vkdevice->device,&renderpass_create_info, nullptr, &render_pass);

        return render_pass;
    }

    VkFramebuffer VKIMGUIRenderer::create_frame_buffer(VkRenderPass render_pass, const std::shared_ptr<rhi::GpuTexture>& tex)
    {
        auto framebuffer_attachments = static_cast<rhi::GpuTextureViewVulkan*>(tex->view(device, rhi::GpuTextureViewDesc()).get())->img_view;
        VkFramebufferCreateInfo fbo_des = {};
        fbo_des.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbo_des.width = tex->desc.extent[0];
        fbo_des.height =  tex->desc.extent[1];
        fbo_des.layers = 1;
        fbo_des.attachmentCount = 1;
        fbo_des.pAttachments = &framebuffer_attachments;
        fbo_des.renderPass = render_pass;
        auto vkdevice = dynamic_cast<rhi::GpuDeviceVulkan*>(device);
        VkFramebuffer	frame_buffer;
        vkCreateFramebuffer(vkdevice->device, &fbo_des, VK_NULL_HANDLE, &frame_buffer);

        return frame_buffer;
    }

    void VKIMGUIRenderer::frame_render(ImGui_ImplVulkanH_Window* wd, rhi::CommandBuffer* cmd_buf)
    {
        DS_PROFILE_FUNCTION();

        //DS_PROFILE_GPU("ImGui Pass");
        auto vkdevice = dynamic_cast<rhi::GpuDeviceVulkan*>(device);
        auto vkswapchain = dynamic_cast<rhi::SwapchainVulkan*>(swapchain);
        auto vkcmd = dynamic_cast<rhi::GpuCommandBufferVulkan*>(cmd_buf)->handle;

        wd->FrameIndex = vkswapchain->next_semaphore;
        auto& descriptorImageMap = ImGui_ImplVulkan_GetDescriptorImageMap();
        {
            auto draw_data = ImGui::GetDrawData();
            for (int n = 0; n < draw_data->CmdListsCount; n++)
            {
                const ImDrawList* cmd_list = draw_data->CmdLists[n];
                for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
                {
                    const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
                    ImGuiTextureID* texID = (ImGuiTextureID*)pcmd->TextureId;

                    if (texID && texID->texture)
                    {
                        VkDescriptorImageInfo   image_info = {};
                        image_info.sampler = texID->sampler_handle ? reinterpret_cast<VkSampler>(texID->sampler_handle) : vkdevice->get_sampler(rhi::GpuSamplerDesc{ rhi::TexelFilter::LINEAR,
                                     rhi::SamplerMipmapMode::LINEAR, rhi::SamplerAddressMode ::REPEAT});
                        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        image_info.imageView = static_cast<rhi::GpuTextureViewVulkan*>(texID->texture->view(device, rhi::GpuTextureViewDesc()).get())->img_view;
                        descriptorImageMap[pcmd->TextureId] = image_info;
         /*               if( !texID->sampler_handle )
                        { 
                            device->record_image_barrier(cmd_buf, rhi::ImageBarrier{
                              texID->texture,
                              rhi::AccessType::TransferWrite,
                              rhi::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer,
                              rhi::ImageAspectFlags::COLOR }
                             );
                         }*/
                    }
                }
            }
        }

        ImGui_ImplVulkan_CreateDescriptorSets(ImGui::GetDrawData(), wd->FrameIndex);

        auto clear_values = VkClearValue{ VkClearColorValue {{0.0f,0.0f,0.0f,0.0f}}};
  
        VkRenderPassBeginInfo rpBegin = {};
        rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBegin.pNext = NULL;
        rpBegin.renderPass = m_Renderpass;
        rpBegin.framebuffer = m_Framebuffers[wd->FrameIndex];
        rpBegin.renderArea.offset.x = 0;
        rpBegin.renderArea.offset.y = 0;
        rpBegin.renderArea.extent.width = wd->Width;
        rpBegin.renderArea.extent.height = wd->Height;
        rpBegin.clearValueCount = 4;
        rpBegin.pClearValues = &clear_values;
        vkCmdBeginRenderPass(vkcmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
        {
            //Record Imgui Draw Data and draw funcs into command buffer
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vkcmd, VK_NULL_HANDLE, wd->FrameIndex);
        }
        vkCmdEndRenderPass(vkcmd);
    }

    void VKIMGUIRenderer::handle_resize(uint32_t width, uint32_t height)
    {
        DS_PROFILE_FUNCTION();
        auto* wd = &g_WindowData;
        auto vkdevice = dynamic_cast<rhi::GpuDeviceVulkan*>(device);
        auto vkswapchain = dynamic_cast<rhi::SwapchainVulkan*>(swapchain);
        wd->Swapchain = vkswapchain->swapchain;
        for (uint32_t i = 0; i < wd->ImageCount; i++)
        {
            auto scBuffer = dynamic_cast<rhi::GpuTextureVulkan*>(vkswapchain->images[i].get());
            wd->Frames[i].Backbuffer = scBuffer->image;
            wd->Frames[i].BackbufferView = static_cast<rhi::GpuTextureViewVulkan*>(scBuffer->view(vkdevice, rhi::GpuTextureViewDesc{}).get())->img_view;
        }

        wd->Width = width;
        wd->Height = height;
        
        for (uint32_t i = 0; i < wd->ImageCount; i++)
        {
                if (m_Framebuffers[i])
                {
                    device->defer_release([frame_buffer = m_Framebuffers[i], vkdevice]() mutable{
                        if( frame_buffer )
                        {
                            vkDestroyFramebuffer(vkdevice->device, frame_buffer, nullptr);
                            frame_buffer = VK_NULL_HANDLE;
                        }
                    });
                    m_Framebuffers[i] = nullptr;
                }
        }
        //
        for (uint32_t i = 0; i < wd->ImageCount; i++)
        {
            auto tex = device->create_texture(rhi::GpuTextureDesc::new_2d(PixelFormat::R8G8B8A8_UNorm, 
                            std::array<u32, 2>{width, height})
                            .with_usage(rhi::TextureUsageFlags::SAMPLED |
                            rhi::TextureUsageFlags::COLOR_ATTACHMENT), {},
                            std::format("ui_tex_{}", i).c_str());
            
            auto frame_buf = create_frame_buffer(m_Renderpass, tex);
            m_Framebuffers[i] = frame_buf;
            target_tex[i] = tex;
            wd->Frames[i].Framebuffer = frame_buf;
        }
        m_CurrentTextureIDIndex = 0;
    }

    void VKIMGUIRenderer::clear()
    {
    }

    void VKIMGUIRenderer::rebuild_font_texture()
    {
        IMGUIRenderer::rebuild_font_texture();
        m_FontTextureID.sampler_handle = (u64*)dynamic_cast<rhi::GpuDeviceVulkan*>(device)->get_sampler(rhi::GpuSamplerDesc{ rhi::TexelFilter::NEAREST,
                                rhi::SamplerMipmapMode::NEAREST, rhi::SamplerAddressMode ::REPEAT});
        auto& io = ImGui::GetIO();
        io.Fonts->TexID = (ImTextureID)&m_FontTextureID;
    }

    IMGUIRenderer* create_vk_imgui_renderer(rhi::GpuDevice* dev, rhi::Swapchain* swapchain)
    {
        return new VKIMGUIRenderer(dev, swapchain);
    }

}
