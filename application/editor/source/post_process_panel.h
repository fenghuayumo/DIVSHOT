#pragma once

#include "editor_panel.h"
#include <memory>
#include <list>
#include <imgui/imgui_helper.h>
#include <future>
namespace diverse
{
    class PostProcessPanel : public EditorPanel
    {
    public:
        PostProcessPanel(bool active = false);
        void	on_imgui_render() override;
        void	on_update(float dt) override;
        void    on_new_scene(Scene* scene) override;

    protected:
        
    };
}