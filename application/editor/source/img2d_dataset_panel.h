#pragma once
#include "editor_panel.h"
#include <imgui/imgui_ent_editor.h>
#include <assets/texture.h>
namespace diverse
{

    class Img2DDataSetPanel : public EditorPanel
    {
    public:
        Img2DDataSetPanel(bool active = true);
        ~Img2DDataSetPanel() = default;

    public:
        void on_imgui_render() override;
        void on_update(float dt) override ;
        void on_new_scene(Scene* scene) override;
        std::shared_ptr<asset::Texture> get_current_train_view_texture();
        std::shared_ptr<asset::Texture> get_train_view_texture(int id);
    protected:
        std::unordered_map<int, std::shared_ptr<asset::Texture>>   train_view_texture;
    };
}