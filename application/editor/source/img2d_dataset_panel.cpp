#include "img2d_dataset_panel.h"
#include "editor.h"
#include <scene/scene.h>
#include <scene/entity_manager.h>
#include <engine/input.h>
#include <imgui.h>
#include <imgui/imgui_manager.h>
#include <imgui/imgui_renderer.h>
#include <imgui/imgui_helper.h>
#include <imgui/IconsMaterialDesignIcons.h>
#include <stb/image_utils.h>
#include <format>
#ifdef DS_SPLAT_TRAIN
#include <gaussian_trainer_scene.hpp>
#endif
namespace diverse
{
    Img2DDataSetPanel::Img2DDataSetPanel(bool active)
        : EditorPanel(active)
    {
        m_Name = U8CStr2CStr(ICON_MDI_IMAGE " Img2D###Img2D");
        m_SimpleName = "Img2D";
    }

    void Img2DDataSetPanel::on_imgui_render()
    {
        auto flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        if (!ImGui::Begin(m_Name.c_str(), &m_Active, flags))
        {
            ImGui::End();
            return;
        }
        auto sceneViewSize = ImGui::GetWindowContentRegionMax() - ImGui::GetWindowContentRegionMin();
        auto sceneViewPosition = ImGui::GetWindowPos();

        sceneViewSize.x -= static_cast<int>(sceneViewSize.x) % 2 != 0 ? 1.0f : 0.0f;
        sceneViewSize.y -= static_cast<int>(sceneViewSize.y) % 2 != 0 ? 1.0f : 0.0f;

#ifdef DS_SPLAT_TRAIN
        auto& reg = m_Editor->get_current_scene()->get_registry();
        auto gsTrain = reg.try_get<GaussianTrainerScene>(m_Editor->get_current_splat_entt());
        if(gsTrain)
        { 
            auto& current_train_view_id = m_Editor->get_current_train_view_id();
            if(current_train_view_id >= 0 )
            {
                auto game_view_tex = get_current_train_view_texture()->gpu_texture;
                ImGuiHelper::Image(game_view_tex, glm::vec2(sceneViewSize.x, sceneViewSize.y), false);
                ImGui::Text("Camera ID: %s", train_view_texture[current_train_view_id]->file_path.c_str());
            }
            if(ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) )
            {
                if (Input::get().get_key_pressed(InputCode::Key::Left))
                {
                    if(current_train_view_id == -1) current_train_view_id = 0;
                    current_train_view_id = (current_train_view_id - 1 + gsTrain->getNumCameras()) % gsTrain->getNumCameras();
                    //only one image persists in the map, so we need to update it
                    train_view_texture.clear();
                }
                if (Input::get().get_key_pressed(InputCode::Key::Right))
                {
                    current_train_view_id = (current_train_view_id + 1) % gsTrain->getNumCameras();
                    train_view_texture.clear();
                }
            }
            auto ButtonPos = ImGui::GetCursorPos();
            ButtonPos.x = sceneViewSize.x * 0.35;
            ButtonPos.y = sceneViewSize.y * 0.9;
            auto ButtonSize = ImVec2(sceneViewSize.x * 0.1,24);
            ImGui::SetCursorPos(ButtonPos);
            if (ImGui::Button(U8CStr2CStr(ICON_MDI_ARROW_LEFT), ButtonSize))
            {
                if (current_train_view_id == -1) current_train_view_id = 0;
                current_train_view_id = (current_train_view_id - 1 + gsTrain->getNumCameras()) % gsTrain->getNumCameras();
                //only one image persists in the map, so we need to update it
                train_view_texture.clear();
            }
            ButtonPos.x = sceneViewSize.x * 0.65;
            ImGui::SetCursorPos(ButtonPos);
            if (ImGui::Button(U8CStr2CStr(ICON_MDI_ARROW_RIGHT), ButtonSize))
            {
                current_train_view_id = (current_train_view_id + 1) % gsTrain->getNumCameras();
                train_view_texture.clear();
            }
        }
#endif
        ImGui::End();
    }
    void Img2DDataSetPanel::on_update(float dt)
    {
        
    }
    void Img2DDataSetPanel::on_new_scene(Scene* scene)
    {
    }
    std::shared_ptr<asset::Texture> Img2DDataSetPanel::get_current_train_view_texture()
    {
        auto& current_train_view_id = m_Editor->get_current_train_view_id();
		return get_train_view_texture(current_train_view_id);
    }
    std::shared_ptr<asset::Texture> Img2DDataSetPanel::get_train_view_texture(int id)
    {
        if (id == -1) return nullptr;
        if (train_view_texture.find(id) != train_view_texture.end())
        {
            return train_view_texture[id];
        }
        auto& reg = m_Editor->get_current_scene()->get_registry();
        auto gsTrain = reg.try_get<GaussianTrainerScene>(m_Editor->get_current_splat_entt());
        if(!gsTrain) return nullptr;
        auto splat_imge = gsTrain->getSplatImageView(id);
        auto tex = std::make_shared<asset::Texture>(splat_imge.width, splat_imge.height, splat_imge.data, PixelFormat::R8G8B8A8_UNorm);
        tex->file_path = splat_imge.name;
        train_view_texture[id] = tex;
        return tex;
    }
}