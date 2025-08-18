#pragma once

#include "editor_panel.h"
#include <imgui/imgui_ent_editor.h>

namespace diverse
{
    class InspectorPanel : public EditorPanel
    {
    public:
        InspectorPanel(bool active = true);
        ~InspectorPanel() = default;

        void on_new_scene(Scene* scene) override;
        void on_imgui_render() override;
        void set_debug_mode(bool mode);
        bool is_debug_mode() const { return debug_mode; };

    private:
        MM::ImGuiEntityEditor<entt::entity> entt_editor;
        bool debug_mode = false;
    };
}
