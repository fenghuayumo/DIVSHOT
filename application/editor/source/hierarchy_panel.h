#pragma once

#include "editor_panel.h"
#include "core/memory.h"
#include <entt/entity/fwd.hpp>
#include <imgui/imgui.h>

namespace diverse
{
    class HierarchyPanel : public EditorPanel
    {
    public:
        HierarchyPanel(bool active = true);
        ~HierarchyPanel();

        void draw_node(entt::entity node, entt::registry& registry);
        void on_imgui_render() override;
    private:
        ImGuiTextFilter hierarchy_filter;
        entt::entity double_clicked_entity;
        entt::entity had_recent_dropped_entity;
        entt::entity current_previous_entity;
        bool select_up;
        bool select_down;

        Arena* string_arena;
    };
}
