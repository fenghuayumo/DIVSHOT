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
        ImGuiTextFilter m_HierarchyFilter;
        entt::entity m_DoubleClicked;
        entt::entity m_HadRecentDroppedEntity;
        entt::entity m_CurrentPrevious;
        bool m_SelectUp;
        bool m_SelectDown;

        Arena* m_StringArena;
    };
}
