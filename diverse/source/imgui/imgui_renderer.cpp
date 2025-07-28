#include <string>
#include "core/core.h"
#include "backend/drs_rhi/gpu_swapchain.h"
#include "backend/drs_rhi/gpu_device.h"
#include "imgui_renderer.h"
#include "backend/drs_rhi/drs_rhi.h"
#include <stb/image_utils.h>
#include <imgui/imgui.h>
#include "core/ds_log.h"

namespace diverse
{
    auto extern create_vk_imgui_renderer(rhi::GpuDevice* dev, rhi::Swapchain* swapchain)->IMGUIRenderer*;
    auto extern create_metal_imgui_renderer(rhi::GpuDevice* dev, rhi::Swapchain* swapchain)->IMGUIRenderer*;

    IMGUIRenderer* IMGUIRenderer::create(rhi::GpuDevice* dev, rhi::Swapchain* swapchain)
    {
        switch (get_render_api())
        {
        case RenderAPI::VULKAN:
        {
           return create_vk_imgui_renderer(dev, swapchain);
        };
#ifdef DS_RENDER_API_METAL
        case RenderAPI::METAL:
                return create_metal_imgui_renderer(dev, swapchain);
#endif
        default:
            break;
        }

        return create_vk_imgui_renderer(dev, swapchain);
    }

    IMGUIRenderer::IMGUIRenderer(rhi::GpuDevice* dev, rhi::Swapchain* swap_chain)
    		: device(dev)
		, swapchain(swap_chain),m_FontTexture(nullptr)
    {
        m_Width = swapchain->desc.dims[0];
        m_Height = swapchain->desc.dims[1];
    }

    ImGuiTextureID* IMGUIRenderer::add_texture(const std::shared_ptr<rhi::GpuTexture>& texture)
    {
        if (m_CurrentTextureIDIndex > MAX_IMGUI_TEXTURES)
        {
            DS_LOG_ERROR("Exceeded max imgui textures");
            return &m_TextureIDs[0];
        }
        //for(auto i=0;i<MAX_IMGUI_TEXTURES;i++){
        //    if(m_TextureIDs[i].texture == texture)
        //        return &m_TextureIDs[i];
        //}
        m_TextureIDs[m_CurrentTextureIDIndex].texture = texture;
        m_TextureIDs[m_CurrentTextureIDIndex].sampler_handle = nullptr;
        return &m_TextureIDs[m_CurrentTextureIDIndex++];
    }
    auto IMGUIRenderer::create_texture(u32 width, u32 height, u8* imag_data) -> std::shared_ptr<rhi::GpuTexture>
    {
        auto desc = rhi::GpuTextureDesc::new_2d(PixelFormat::R8G8B8A8_UNorm, { width, height })
            .with_usage(rhi::TextureUsageFlags::SAMPLED
                | rhi::TextureUsageFlags::TRANSFER_SRC
                | rhi::TextureUsageFlags::TRANSFER_DST);
        auto tex = device->create_texture(desc,{ rhi::ImageSubData{imag_data, (width * height * 4), width * 4}});
        return tex;
    }
    auto IMGUIRenderer::create_texture(u32 width, u32 height) ->std::shared_ptr<rhi::GpuTexture>
    {
        auto desc = rhi::GpuTextureDesc::new_2d(PixelFormat::R8G8B8A8_UNorm, {width, height})
                        .with_usage(rhi::TextureUsageFlags::SAMPLED 
                        | rhi::TextureUsageFlags::TRANSFER_SRC
                        | rhi::TextureUsageFlags::TRANSFER_DST);
        auto tex = device->create_texture(desc, {});
        return tex;
    }

    auto    IMGUIRenderer::copy_texture(rhi::GpuTexture* src, rhi::GpuTexture* dst) -> void
    {
        return device->copy_image(src, dst, nullptr);
    }

    auto    IMGUIRenderer::blit_texture(rhi::GpuTexture* src, rhi::GpuTexture* dst)->void
    {
        return device->blit_image(src,dst, nullptr);
    }
    auto IMGUIRenderer::export_texture(rhi::GpuTexture* src) -> std::vector<u8>
    {
        return device->export_image(src);
    }
    auto IMGUIRenderer::get_target_tex() -> std::shared_ptr<rhi::GpuTexture>
    {
        return target_tex[swapchain->current_buffer_index()];
    }

    void IMGUIRenderer::rebuild_font_texture()
    {        
        // Upload Fonts
        {
            ImGuiIO& io = ImGui::GetIO();
            
            unsigned char* pixels;
            int width, height;
            io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
            
            std::vector<rhi::ImageSubData>  initial_data;
            u32 row_pitch = width * 4;
            
            initial_data.emplace_back(rhi::ImageSubData{ pixels, (u32)(height * row_pitch), row_pitch, 0});
            m_FontTexture = device->create_texture(rhi::GpuTextureDesc::new_2d(
                                                                               PixelFormat::R8G8B8A8_UNorm,
                                                                               std::array<u32,2>{(u32)width, (u32)height})
                                                   .with_usage(rhi::TextureUsageFlags::SAMPLED),
                                                   initial_data,
                                                   "font_image");
            m_FontTextureID.texture = m_FontTexture;
        }
    }

    auto IMGUIRenderer::get_imgui_texID(const char* fpath)->ImGuiTextureID*
    {
        std::shared_ptr<rhi::GpuTexture> tex;
        if( imgui_tex_cache.find(fpath) != imgui_tex_cache.end())
        {
            tex = imgui_tex_cache[fpath];
        }
        else {
            int width, height, comp;
            u8* img_data;
            try{
                img_data = load_stbi(fpath, &width, &height, &comp, 4);
            }
            catch(std::exception& e){
				DS_LOG_ERROR("Failed to load image: {}", e.what());
				return nullptr;
			}
            if(!img_data){
				DS_LOG_ERROR("Failed to load image: {}", fpath);
				return nullptr;
			}
            tex = create_texture(width, height, img_data);
            free(img_data);
            imgui_tex_cache[fpath] = tex;
        }
        return add_texture(tex);
    }
}
