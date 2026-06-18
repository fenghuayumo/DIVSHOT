#pragma once
#include "imgui_renderer.h"
#include "backend/drs_vulkan_rhi/vk_utils.h"
#include "backend/drs_rhi/gpu_cmd_buffer.h"
#include <memory>
struct ImGui_ImplVulkanH_Window;
namespace diverse
{
    class VKIMGUIRenderer : public IMGUIRenderer
    {
    public:
        VKIMGUIRenderer(rhi::GpuDevice* dev, rhi::Swapchain* swapchain);
        ~VKIMGUIRenderer();


        void init() override;
        void new_frame() override;
        void render(rhi::CommandBuffer* cmd_buf) override;
        void render_draw_data(rhi::CommandBuffer* cmd_buf, ImDrawData* draw_data) override;
        void handle_resize(uint32_t width, uint32_t height) override;
        void clear() override;
        void rebuild_font_texture() override;
        ImTextureID snapshot_texture_id(ImTextureID texture_id, ImGuiDrawDataSnapshot& snapshot) override;
        void setup_vulkan_window_data(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, u32 width, u32 height);

    protected: 
        VkRenderPass     create_render_pass();
        VkFramebuffer    create_frame_buffer(VkRenderPass render_pass,const std::shared_ptr<rhi::GpuTexture>& tex);

        void             frame_render(ImGui_ImplVulkanH_Window* wd,rhi::CommandBuffer* cmd_buf, ImDrawData* draw_data);
    private:
        VkFramebuffer   m_Framebuffers[3];
        VkRenderPass    m_Renderpass;
        bool m_ClearScreen;
    };
}
