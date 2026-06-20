#pragma once
#include "editor_panel.h"
#include <imgui/imgui_ent_editor.h>
#include <assets/cpu_assets.h>
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
        std::shared_ptr<TextureAsset> get_current_train_view_texture();
        std::shared_ptr<TextureAsset> get_train_view_texture(int id);
    protected:
        std::unordered_map<int, std::shared_ptr<TextureAsset>>   train_view_texture;
    };
}
