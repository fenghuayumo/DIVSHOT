#pragma once
#include "editor_panel.h"
#include "editor.h"
#include <maths/frustum.h>
#include <maths/transform.h>
#include <core/string.h>
#include <utility/string_utils.h>
#include <imgui/imgui_helper.h>
#include <maths/maths_utils.h>

#include <imgui/imgui.h>
DISABLE_WARNING_PUSH
DISABLE_WARNING_CONVERSION_TO_SMALLER_TYPE
#include <entt/entt.hpp>
DISABLE_WARNING_POP

namespace diverse
{
    class SceneViewPanel : public EditorPanel
    {
    public:
        SceneViewPanel(bool active = true);
        ~SceneViewPanel() = default;

        void on_imgui_render() override;
        void on_new_scene(Scene* scene) override;
        void toolbar();
        void draw_gizmos(float width, float height, float xpos, float ypos, Scene* scene);

        bool resize(uint32_t width, uint32_t height);
        void on_update(float dt) override;
        void draw_splat_edit_toolbox(const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize);
        void draw_progress(const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize);
        void draw_3d_paint_toolbar(const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize);
        void drwa_splat_mask_box(Camera* camera, maths::Transform* camera_transform,const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize);
        void handle_splat_edit(Camera* camera,maths::Transform* camera_transform,const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize);
        void handle_splat_crop(Camera* camera, maths::Transform* camera_transform,const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize);
        void handle_3d_paint(const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize);
        bool handle_mouse_move(MouseMovedEvent& e) override;
        bool handle_mouse_pressed(MouseButtonPressedEvent& e) override;
        bool handle_mouse_released(MouseButtonReleasedEvent& e) override;
 
        auto pick_splat(struct GaussianModel* splat,
                const glm::vec2& mouse_pos,
                const maths::Ray& ray,
                const glm::mat4& model_transform,
                const maths::Transform& camera_transform,
                const glm::mat4& project,
                u32 surface_width,
                u32 surface_height,
                glm::vec3& isect_points)->bool;
    protected:
        void take_photo_dialog();
        void render_frame_image();
        void reset_render_frame();
        bool is_valid_region(const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize) const;
    private:
        template <typename T>
        void ShowComponentGizmo(float width, float height, float xpos, float ypos, const glm::mat4& viewProj, const maths::Frustum& frustum, entt::registry& registry)
        {
            if (m_ShowComponentGizmoMap[typeid(T).hash_code()])
            {
                auto group = registry.group<T>(entt::get<maths::Transform>);

                for (auto entity : group)
                {
                    const auto& [component, trans] = group.template get<T, maths::Transform>(entity);

                    glm::vec3 pos = trans.get_world_position();

                    auto inside = frustum.is_inside(pos);

                    if (inside == Intersection::OUTSIDE)
                        continue;

                    glm::vec2 screenPos = maths::WorldToScreen(pos, viewProj, width, height, xpos, ypos);
                    ImGui::SetCursorPos({ screenPos.x - ImGui::GetFontSize() * 0.5f, screenPos.y - ImGui::GetFontSize() * 0.5f });
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.7f, 0.7f, 0.0f));

                    // if(ImGui::Button(m_Editor->GetComponentIconMap()[typeid(T).hash_code()]))
                    //{
                    //     m_Editor->set_selected(entity);
                    // }

                    ImGui::TextUnformatted(m_Editor->get_component_iconmap()[typeid(T).hash_code()]);
                    ImGui::PopStyleColor();

                    const char* demangledName = nullptr;
                    auto found = m_DemangledNames.find(typeid(T).hash_code());
                    if (found == m_DemangledNames.end())
                    {
                        m_DemangledNames[typeid(T).hash_code()] = stringutility::demangle(typeid(T).name());
                        demangledName = m_DemangledNames[typeid(T).hash_code()].c_str();
                    }
                    else
                        demangledName = found->second.c_str();

                    ImGuiHelper::Tooltip(demangledName);
                }
            }
        }

        std::unordered_map<size_t, bool> m_ShowComponentGizmoMap;
        std::unordered_map<size_t, std::string> m_DemangledNames;
        // std::shared_ptr<rhi::GpuTexture> m_GameViewTexture = nullptr;

        bool m_ShowStats = false;
        Scene* m_CurrentScene = nullptr;
        uint32_t m_Width, m_Height;
        glm::vec2 m_StartMousePos;
        glm::vec2 m_EndMousePos;
        bool      m_IsDoingSplatSelectOp = false;

        struct PhotonSetting
        {
            std::string file_path = "dvs.png";
            glm::ivec2  resolution = glm::ivec2(1920,1080);
            int         spp = 1024;
            i32         frame_counter = 0;
        }m_photon_setting;
    };
}
