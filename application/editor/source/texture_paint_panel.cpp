#include "texture_paint_panel.h"
#include "editor.h"
#include "image_paint.h"
#include <scene/scene.h>
#include <scene/entity_manager.h>
#include <utility/thread_pool.h>
#include <backend/drs_rhi/gpu_device.h>
#include <imgui.h>
#include <imgui/imgui_manager.h>
#include <imgui/imgui_renderer.h>
#include <imgui/imgui_helper.h>
#include <imgui/IconsMaterialDesignIcons.h>
#include <stb/image_utils.h>
#include <format>
#include <opencv2/opencv.hpp>
#include "redo_undo_system.h"
namespace diverse
{
    extern ImVec2 item_btn_size;
    TexturePaintPanel::TexturePaintPanel(bool active)
        : EditorPanel(active)
    {
        m_Name = U8CStr2CStr(ICON_MDI_GATE " Texture Paint###texture_paint_panel");
        m_SimpleName = "TexturePaintPanel";
    }
    
    void TexturePaintPanel::set_paint_texture(const std::shared_ptr<rhi::GpuTexture>& tex)
    {
        ImagePaint::get().set_paint_texture(tex);
        texture_zoom_scale = 1.0f;
        texture_offset = glm::vec2(0, 0);
    }
    bool TexturePaintPanel::is_valid_region(const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize) const
    {
        auto size = item_btn_size + ImVec2(10, 10);
        auto mouse_view_pos = (ImGui::GetMousePos() - sceneViewPosition);
        bool valid_region = mouse_view_pos.x >= size.x && mouse_view_pos.y > 10 && mouse_view_pos.x < (sceneViewSize.y - 10) && mouse_view_pos.x < (sceneViewSize.x - 10);
        return valid_region;
    }

    void TexturePaintPanel::on_imgui_render()
    {
        auto flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        if (!ImGui::Begin(m_Name.c_str(), &m_Active, flags))
        {
            ImGui::End();
            return;
        }
        ImVec2 offset = { 0.0f, 0.0f };
        auto sceneViewSize = ImGui::GetWindowContentRegionMax() - ImGui::GetWindowContentRegionMin() - offset * 0.5f;
        auto sceneViewPosition = ImGui::GetWindowPos() + offset;

        sceneViewSize.x -= static_cast<int>(sceneViewSize.x) % 2 != 0 ? 1.0f : 0.0f;
        sceneViewSize.y -= static_cast<int>(sceneViewSize.y) % 2 != 0 ? 1.0f : 0.0f;
        auto& paint_texture = ImagePaint::get().get_paint_texture();
        if(paint_texture )
        {
            ImGuiIO& io = ImGui::GetIO();

            ImVec2 mouse_pos = ImGui::GetMousePos();
            ImVec2 window_pos = ImGui::GetCursorScreenPos();
            ImVec2 rel_mouse_pos = ImVec2(mouse_pos.x - window_pos.x, mouse_pos.y - window_pos.y);
            if (ImGui::IsWindowHovered()) 
            {
                float wheel = io.MouseWheel;
                if (wheel != 0.0f) 
                {
                    ImVec2 old_mouse_rel = ImVec2(
                        (rel_mouse_pos.x - texture_offset.x) / texture_zoom_scale,
                        (rel_mouse_pos.y - texture_offset.y) / texture_zoom_scale
                    );
                    float new_zoom = texture_zoom_scale * (wheel > 0 ? 1.1f : 0.9f);
                    new_zoom = ImClamp(new_zoom, 0.02f, 8.0f); 

                    texture_offset.x = rel_mouse_pos.x - old_mouse_rel.x * new_zoom;
                    texture_offset.y = rel_mouse_pos.y - old_mouse_rel.y * new_zoom;

                    texture_zoom_scale = new_zoom;
                }
            }
            if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) && ImGui::IsWindowHovered())
            {
                texture_offset.x += io.MouseDelta.x;
                texture_offset.y += io.MouseDelta.y;
            }

            glm::vec2 image_size = glm::vec2{paint_texture->desc.extent[0],paint_texture->desc.extent[1]} * texture_zoom_scale;
            ImGui::SetCursorPos(ImVec2(texture_offset.x, texture_offset.y));
            ImGuiHelper::Image(paint_texture, image_size, false);
        }

        ImGui::GetWindowDrawList()->PushClipRect(sceneViewPosition, { sceneViewSize.x + sceneViewPosition.x, sceneViewSize.y + sceneViewPosition.y - 2.0f });
        
        draw_paint_tool_bar(sceneViewPosition,sceneViewSize);
        handle_texture_edit(sceneViewPosition, sceneViewSize);
        ImGui::GetWindowDrawList()->PopClipRect();
        ImGui::End();
    }
    
    void TexturePaintPanel::on_update(float dt)
    {
    }

    void TexturePaintPanel::on_new_scene(Scene* scene)
    {
    }

    void TexturePaintPanel::draw_paint_tool_bar(const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize)
    {
        auto size = item_btn_size + ImVec2(10, 10);
        auto offsetY = sceneViewPosition.y + (sceneViewSize.y - 12 * size.y) / 2.0f;
        ImGui::SetCursorPosY(offsetY);
        auto& edit_type = ImagePaint::get().image_edit_type();
        bool selected = edit_type == ImageEditOpType::Paint;
        if (selected)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGuiHelper::GetSelectedColour());
        if (ImGui::Button(U8CStr2CStr(ICON_MDI_BRUSH), item_btn_size))
        {
            edit_type = selected ? ImageEditOpType::None : ImageEditOpType::Paint;
        }
        if (selected)
            ImGui::PopStyleColor();
        ImGuiHelper::Tooltip("Paint");
    }
    void TexturePaintPanel::handle_texture_edit(const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize)
    {
        auto& edit_type = ImagePaint::get().image_edit_type();
        //auto& brush_tool = ImagePaint::get().brush();
        auto& paint_texture = ImagePaint::get().get_paint_texture();
        auto mouseX = ImGui::GetMousePos().x - sceneViewPosition.x;
        if (edit_type != ImageEditOpType::None && ImGui::IsWindowFocused() && is_valid_region(sceneViewPosition,sceneViewSize))
        {
            brush_tool.create_brush_buffer(sceneViewSize.x, sceneViewSize.y);
            auto drawList = ImGui::GetWindowDrawList();
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                if (edit_type == ImageEditOpType::Paint)
                {
                    brush_tool.reset_brush();
                    auto p0 = ImGui::GetMousePos() - sceneViewPosition;
                    brush_tool.brush_points().push_back({ p0.x,p0.y });
                }
            }
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                auto p1 = ImGui::GetMousePos() - sceneViewPosition;
                auto p1_v = glm::ivec2(p1.x, p1.y);
                if (brush_tool.brush_points().size() > 0 && brush_tool.brush_points().back() != p1_v)
                    brush_tool.brush_points().push_back(p1_v);
                const int num_points = brush_tool.brush_points().size();
                auto ptr = brush_tool.brush_points().data();
                auto paint_color = glm::vec3(1);
                auto line_color =  cv::Scalar(255 * paint_color.x, 255 * paint_color.y, 255 * paint_color.z, 102);
                cv::polylines(*brush_tool.brush_mask(), (const cv::Point* const*)(&ptr), &num_points, 1, false, line_color, brush_tool.brush_thick_ness(), cv::LINE_AA);
                brush_tool.update_brush_buffer();

                auto texID = Application::get().get_imgui_manager()->get_imgui_renderer()->add_texture(brush_tool.brush_texture());
                drawList->AddImage(texID, sceneViewPosition, sceneViewPosition + sceneViewSize, ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));
            }
            else
            {
                if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                { 
                    std::vector<int> paint_points(paint_texture->desc.extent[0] * paint_texture->desc.extent[1],-1);
                    parallel_for<size_t>(0, paint_points.size(), [&](size_t i) {
                        auto mask_data = reinterpret_cast<uint32*>(brush_tool.brush_mask()->data);
                        auto row = i / paint_texture->desc.extent[0];
                        auto col = i % paint_texture->desc.extent[0];
                        int x = (col ) * texture_zoom_scale + texture_offset.x;
                        int y = (row ) * texture_zoom_scale + texture_offset.y;
                        if (x >= 0 && y >= 0 && x < sceneViewSize.x && y < sceneViewSize.y)
                        {
                            int idx = y * sceneViewSize.x + x;
                            if (mask_data[idx] > 0)
                            {
                                paint_points[i] = 1;
                            }
                        }
                    });
                    ImagePaintColorAdjustment paint_state = {glm::vec3(1,0,0),1.0f};
                    UndoRedoSystem::get().add(std::make_shared<ImagePaintOperation>(paint_texture, paint_state,paint_points));
                }
            }

            glm::vec4 paint_color = glm::vec4(1);
            auto cirle_color =  IM_COL32(255 * paint_color.x, 255 * paint_color.y, 255 * paint_color.z, 102);
            drawList->AddCircleFilled(ImGui::GetMousePos(), brush_tool.brush_thick_ness() / 2.0f, cirle_color);
        }
    }
}