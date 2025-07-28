#pragma once
#include "editor_panel.h"
#include "image_op.h"
#include <imgui/imgui_ent_editor.h>
#include "brush_tool.h"

namespace diverse
{
    namespace rhi
    {
        struct GpuTexture;
    }
    class TexturePaintPanel : public EditorPanel
    {
    public:
        TexturePaintPanel(bool active = true);
        ~TexturePaintPanel() = default;
    public:
        void on_imgui_render() override;
        void on_update(float dt) override ;
        void on_new_scene(Scene* scene) override;
    public:
        void draw_paint_tool_bar(const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize);
        void handle_texture_edit(const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize);
        void set_paint_texture(const std::shared_ptr<rhi::GpuTexture>& tex);
        bool is_valid_region(const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize) const;
    protected:
        float           texture_zoom_scale = 1.0f;
        glm::vec2       texture_offset = glm::vec2(10,10);
        BrushTool       brush_tool;
    };
}