#ifdef DS_RENDER_API_METAL
#include "metal_imgui_renderer.h"
#include "backend/dvs_metal/gpu_device_metal.h"

#include <imgui/imgui.h>
#define VK_NO_PROTOTYPES
#define __OBJC__
#include <imgui/backends/imgui_impl_metal.h>
#include "core/ds_log.h"
namespace diverse
{
    IMGUIRenderer* create_metal_imgui_renderer(rhi::GpuDevice* dev, rhi::Swapchain* swapchain)
    {
        return new MetalIMGUIRenderer(dev, swapchain);
    }

    MetalIMGUIRenderer::MetalIMGUIRenderer(rhi::GpuDevice* dev, rhi::Swapchain* swapchain)
        : IMGUIRenderer(dev, swapchain)
    {
    }
    MetalIMGUIRenderer::~MetalIMGUIRenderer()
    {
    }

    void MetalIMGUIRenderer::init()
    {
        auto metalswapchain = dynamic_cast<rhi::SwapchainMetal*>(swapchain);
        for (uint32_t i = 0; i < metalswapchain->back_buffer_count(); i++)
        {
            target_tex[i] = device->create_texture(rhi::GpuTextureDesc::new_2d(PixelFormat::R8G8B8A8_UNorm, {m_Width, m_Height}).with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::COLOR_ATTACHMENT), {}, "ui_tex");
        }
        auto metal_device = dynamic_cast<rhi::GpuDeviceMetal*>(device)->device;
        ImGui_ImplMetal_Init(metal_device);

        renderPassDescriptor = [MTLRenderPassDescriptor new];
        
        rebuild_font_texture();
    }

    void MetalIMGUIRenderer::rebuild_font_texture()
    {
        IMGUIRenderer::rebuild_font_texture();
        ImGuiIO& io = ImGui::GetIO();
        auto metal_font_tex = dynamic_cast<rhi::GpuTextureMetal*>(m_FontTexture.get());
        io.Fonts->TexID = metal_font_tex->id_tex;
    }
    
    ImGuiTextureID* MetalIMGUIRenderer::add_texture(const std::shared_ptr<rhi::GpuTexture>& texture)
    {
        if (m_CurrentTextureIDIndex > MAX_IMGUI_TEXTURES)
        {
            DS_LOG_ERROR("Exceeded max imgui textures");
            auto tex = dynamic_cast<rhi::GpuTextureMetal*>(texture.get());
            return reinterpret_cast<ImGuiTextureID*>(tex->id_tex);
        }
        m_TextureIDs[m_CurrentTextureIDIndex].texture = texture;
        m_TextureIDs[m_CurrentTextureIDIndex].sampler_handle = nullptr;
        auto tex = dynamic_cast<rhi::GpuTextureMetal*>(m_TextureIDs[m_CurrentTextureIDIndex++].texture.get());
        return reinterpret_cast<ImGuiTextureID*>(tex->id_tex);
    }

    void MetalIMGUIRenderer::new_frame()
    {
        m_CurrentTextureIDIndex = 0;
    }
    
    void MetalIMGUIRenderer::render(rhi::CommandBuffer* cmd_buf)
    {
        auto cmd_metal = dynamic_cast<rhi::GpuCommandBufferMetal*>(cmd_buf);
        auto metal_swapchain = dynamic_cast<rhi::SwapchainMetal*>(swapchain);
        auto commandBuffer = cmd_metal->cmd_buf;
        auto buffer_id = metal_swapchain->next_img;
        auto metal_tex = dynamic_cast<rhi::GpuTextureMetal*>(target_tex[buffer_id].get())->id_tex;
        renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0,0,0,1);
        renderPassDescriptor.colorAttachments[0].texture = metal_tex;
        renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
        renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
        id <MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        renderEncoder.label = @"ImGui Render";
        [renderEncoder pushDebugGroup:@"ImGui Render"];
        
        ImGui_ImplMetal_NewFrame(renderPassDescriptor);

        ImGui::Render();
        ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), commandBuffer, renderEncoder);

        [renderEncoder popDebugGroup];
        [renderEncoder endEncoding];

    }
    
    void MetalIMGUIRenderer::handle_resize(uint32_t width, uint32_t height)
    {
        if( width != m_Width || height != m_Height)
        {
            auto metaldevice = dynamic_cast<rhi::GpuDeviceMetal*>(device);
            auto metalswapchain = dynamic_cast<rhi::SwapchainMetal*>(swapchain);
            m_Width = width;
            m_Height = height;
            for (uint32_t i = 0; i <metalswapchain->back_buffer_count(); i++)
            {
                if( target_tex[i])
                {
//                    device->destroy_texture(target_tex[i]);
                    target_tex[i] = nullptr;
                }
                target_tex[i] = device->create_texture(rhi::GpuTextureDesc::new_2d(PixelFormat::R8G8B8A8_UNorm, {m_Width, m_Height}).with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::COLOR_ATTACHMENT), {}, std::format("ui_tex_{}",i).c_str());
            }
        }
    }
    
    void MetalIMGUIRenderer::clear()
    {
        
    }
}
#endif