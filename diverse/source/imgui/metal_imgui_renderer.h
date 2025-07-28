#pragma once
#include "imgui_renderer.h"
#include "backend/dvs_metal/dvs_metal_utils.h"
#include "backend/drs_rhi/gpu_cmd_buffer.h"
#include <memory>
namespace diverse
{
    class MetalIMGUIRenderer : public IMGUIRenderer
    {
    public:
        MetalIMGUIRenderer(rhi::GpuDevice* dev, rhi::Swapchain* swapchain);
        ~MetalIMGUIRenderer();

        void init() override;
        void new_frame() override;
        void render(rhi::CommandBuffer* cmd_buf) override;
        void handle_resize(uint32_t width, uint32_t height) override;
        void clear() override;
        void rebuild_font_texture() override;
        ImGuiTextureID* add_texture(const std::shared_ptr<rhi::GpuTexture>& texture) override;
    protected:
     MTLRenderPassDescriptor *renderPassDescriptor;
    };
}
