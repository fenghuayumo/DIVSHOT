#pragma once
#include "core/base_type.h"
#include <memory>
#include <vector>
#include <unordered_map>
namespace diverse
{
    namespace rhi
    {
        struct GpuDevice;
        struct Swapchain;
        struct GpuTexture;
        struct CommandBuffer;
    }
    struct ImGuiTextureID
    {
        std::shared_ptr<rhi::GpuTexture> texture;
        u64*             sampler_handle = nullptr;
    };

    class IMGUIRenderer
    {
    public:
        static IMGUIRenderer* create(rhi::GpuDevice* dev, rhi::Swapchain* swapchain);
        IMGUIRenderer(rhi::GpuDevice* dev, rhi::Swapchain* swapchain);
        virtual ~IMGUIRenderer() = default;

        virtual void init() = 0;
        virtual void new_frame() = 0;
        virtual void render(rhi::CommandBuffer* cmd_buf) = 0;
        virtual void handle_resize(uint32_t width, uint32_t height) = 0;
        virtual void clear() { }
        virtual void rebuild_font_texture();
        virtual ImGuiTextureID* add_texture(const std::shared_ptr<rhi::GpuTexture>& texture);

        auto    create_texture(u32 width, u32 height,u8* imag_data) -> std::shared_ptr<rhi::GpuTexture>;
        auto    create_texture(u32 width,u32 height)-> std::shared_ptr<rhi::GpuTexture>;
        auto    get_target_tex()-> std::shared_ptr<rhi::GpuTexture>;
        auto    copy_texture(rhi::GpuTexture* src, rhi::GpuTexture* dst) -> void;
        auto    blit_texture(rhi::GpuTexture* src, rhi::GpuTexture* dst) -> void;
        auto    export_texture(rhi::GpuTexture* src) -> std::vector<u8>;
        auto    get_imgui_texID(const char* fpath)->ImGuiTextureID*;
#define MAX_IMGUI_TEXTURES 1024
        ImGuiTextureID m_TextureIDs[MAX_IMGUI_TEXTURES];
        uint32_t m_CurrentTextureIDIndex = 0;

        rhi::GpuDevice* device;
        rhi::Swapchain* swapchain;

        std::shared_ptr<rhi::GpuTexture>    target_tex[3];
        
        std::shared_ptr<rhi::GpuTexture> m_FontTexture;
        ImGuiTextureID m_FontTextureID;
        
        uint32_t m_Width;
        uint32_t m_Height;
        std::unordered_map<const char*, std::shared_ptr<rhi::GpuTexture>> imgui_tex_cache;
    };

    ImGuiTextureID* add_imgui_texture(const std::shared_ptr<rhi::GpuTexture>& tex);
} // namespace diverse
