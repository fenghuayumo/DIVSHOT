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

namespace diverse
{
    class HelperPanel : public EditorPanel
    {
    public:
        HelperPanel(bool active = false);
        ~HelperPanel() = default;

        void on_new_scene(Scene* scene) override;
        void on_imgui_render() override;

        void draw_about_dialog();
        void draw_update_dialog();
        void set_tab(int tab) { current_tab = tab; }
    private:
        int  current_tab = -1;
    };
}