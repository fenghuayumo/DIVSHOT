#include <core/string.h>
#include "hierarchy_panel.h"
#include "editor.h"
#include "inspector_panel.h"
#include "gaussian_edit.h"
#include <engine/input.h>
#include <engine/application.h>
#include <scene/scene_manager.h>
#include <imgui//imgui_helper.h>
#include <scene/scene_graph.h>
#include <maths/transform.h>
#include <scene/entity.h>
#include <scene/component/components.h>
#include <scene/component/environment.h>
#include <scene/component/time_line.h>
#include <scene/component/light/directional_light.h>
#include <scene/component/light/point_light.h>
#include <scene/component/light/rect_light.h>
#include <scene/component/light/spot_light.h>
#include <scene/component/light/disk_light.h>
#include <scene/component/light/cylinder_light.h>
#include <scene/component/mesh_model_component.h>

#include <imGui/IconsMaterialDesignIcons.h>
#include <utility/string_utils.h>

#include <scene/entity_factory.h>
#include <core/string.h>

#include <typeinfo>
#include <imgui/imgui_internal.h>
#include <sol/sol.hpp>

namespace diverse
{
    HierarchyPanel::HierarchyPanel(bool active)
        : EditorPanel(active)
        , had_recent_dropped_entity(entt::null)
        , double_clicked_entity(entt::null)
    {
        name = U8CStr2CStr(ICON_MDI_FILE_TREE " Hierarchy###hierarchy");
        simple_name = "Hierarchy";
        string_arena = ArenaAlloc(Kilobytes(32));
    }

    HierarchyPanel::~HierarchyPanel()
    {
        ArenaRelease(string_arena);
    }


    void HierarchyPanel::draw_node(entt::entity node, entt::registry& registry)
    {
        DS_PROFILE_FUNCTION_LOW();
        bool show = true;

        if (!registry.valid(node))
            return;

        Entity nodeEntity = { node, Application::get().get_scene_manager()->get_current_scene() };

        static const char* defaultName = "Entity";
        const NameComponent* nameComponent = registry.try_get<NameComponent>(node);
        String8 name = PushStr8Copy(string_arena, nameComponent ? nameComponent->name.c_str() : defaultName); // StringUtilities::ToString(entt::to_integral(node));

        if (hierarchy_filter.IsActive())
        {
            if (!hierarchy_filter.PassFilter((const char*)name.str))
            {
                show = false;
            }
        }

        if (show)
        {
            ImGuiHelper::PushID();
            auto hierarchyComponent = registry.try_get<Hierarchy>(node);
            bool noChildren = true;

            if (hierarchyComponent != nullptr && hierarchyComponent->first() != entt::null)
                noChildren = false;

            ImGuiTreeNodeFlags nodeFlags = ((editor->is_selected(node)) ? ImGuiTreeNodeFlags_Selected : 0);

            nodeFlags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth;

            if (noChildren)
            {
                nodeFlags |= ImGuiTreeNodeFlags_Leaf;
            }

            bool active = Entity(node, editor->get_current_scene()).active(); 

            if (!active)
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

            bool doubleClicked = false;
            if (node == double_clicked_entity)
            {
                doubleClicked = true;
            }

            if (doubleClicked)
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 1.0f, 2.0f });

            if (had_recent_dropped_entity == node)
            {
                ImGui::SetNextItemOpen(true);
                had_recent_dropped_entity = entt::null;
            }

            String8 icon = Str8C((char*)ICON_MDI_CUBE_OUTLINE);
            auto& iconMap = editor->get_component_iconmap();

            if (registry.all_of<Camera>(node))
            {
                if (iconMap.find(typeid(Camera).hash_code()) != iconMap.end())
                    icon = Str8C((char*)iconMap[typeid(Camera).hash_code()]);
            }
            else if(registry.all_of<Environment>(node))
            {
                if(iconMap.find(typeid(Environment).hash_code()) != iconMap.end())
                    icon = Str8C((char*)iconMap[typeid(Environment).hash_code()]);
            }
            ImGui::PushStyleColor(ImGuiCol_Text, ImGuiHelper::GetIconColour());

            bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)entt::to_integral(node), nodeFlags, (const char*)icon.str);
            {
                if (ImGui::BeginDragDropSource())
                {
                    if (!editor->is_selected(node))
                    {
      /*                  m_Editor->clear_selected();
                        m_Editor->set_selected(node);*/
                        auto selected = node;
                        ImGui::TextUnformatted(Entity(node, Application::get().get_current_scene()).get_name().c_str());
                        ImGui::SetDragDropPayload("Drag_Entity", &selected, sizeof(entt::entity));
                    }
                    else
                    { 
                        auto selected = editor->get_selected();
                        for (auto e : selected)
                        {
                            ImGui::TextUnformatted(Entity(e, Application::get().get_current_scene()).get_name().c_str());
                        }

                        ImGui::SetDragDropPayload("Drag_Entity", selected.data(), selected.size() * sizeof(entt::entity));
                    }
                    ImGui::EndDragDropSource();
                }

                // Allow clicking of icon and text. Need twice as they are separated
                if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsItemHovered() && !ImGui::IsItemToggledOpen())
                {
                    bool ctrlDown = Input::get().get_key_held(diverse::InputCode::Key::LeftControl) || Input::get().get_key_held(diverse::InputCode::Key::RightControl) || Input::get().get_key_held(diverse::InputCode::Key::LeftSuper);
                    if (!ctrlDown)
                        editor->clear_selected();

                    if (!editor->is_selected(node))
                        editor->set_selected(node);
                    else
                        editor->un_select(node);
                }
                else if (double_clicked_entity == node && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsItemHovered(ImGuiHoveredFlags_None))
                    double_clicked_entity = entt::null;
            }

            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered(ImGuiHoveredFlags_None))
            {
                double_clicked_entity = node;
                if (Application::get().get_editor_state() == EditorState::Preview)
                {
                    auto transform = registry.try_get<maths::Transform>(node);
                    if (transform)
                        editor->focus_camera(transform->get_world_position(), 2.0f, 2.0f);
                }
            }

            bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_None);

            ImGui::PopStyleColor();
            ImGui::SameLine();
            if (!doubleClicked)
            {
                bool isPrefab = false;
                if (registry.any_of<PrefabComponent>(node))
                    isPrefab = true;
                else
                {
                    auto Parent = nodeEntity.get_parent();
                    while (Parent && Parent.valid())
                    {
                        if (Parent.has_component<PrefabComponent>())
                        {
                            isPrefab = true;
                            Parent = {};
                        }
                        else
                        {
                            Parent = Parent.get_parent();
                        }
                    }
                }

                if (isPrefab)
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
                ImGui::TextUnformatted((const char*)name.str);
                if (isPrefab)
                    ImGui::PopStyleColor();
            }

            if (doubleClicked)
            {
                String8 nameBuffer = { 0 };
                nameBuffer.str = PushArray(string_arena, uint8_t, INPUT_BUF_SIZE);
                nameBuffer.size = INPUT_BUF_SIZE;

                MemoryCopy(nameBuffer.str, name.str, name.size);

                ImGui::PushItemWidth(-1);
                if (ImGui::InputText("##Name", (char*)nameBuffer.str, INPUT_BUF_SIZE, 0))
                    registry.get_or_emplace<NameComponent>(node).name = (const char*)nameBuffer.str;
                ImGui::PopStyleVar();
            }

            if (!active)
                ImGui::PopStyleColor();

            bool deleteEntity = false;

            if (ImGui::BeginPopupContextItem((const char*)name.str))
            {
                if (ImGui::Selectable("Copy"))
                {
                    if (!editor->is_selected(node))
                    {
                        editor->set_copied_entity(node);
                    }
                    for (auto entity : editor->get_selected())
                        editor->set_copied_entity(entity);
                }

                if (ImGui::Selectable("Cut"))
                {
                    for (auto entity : editor->get_selected())
                        editor->set_copied_entity(node, true);
                }

                if (editor->get_copied_entity().size() > 0 && registry.valid(editor->get_copied_entity().front()))
                {
                    if (ImGui::Selectable("Paste"))
                    {
                        for (auto entity : editor->get_copied_entity())
                        {
                            auto scene = Application::get().get_current_scene();
                            Entity copiedEntity = { entity, scene };
                            scene->duplicate_entity(copiedEntity, { node, scene });

                            if (editor->get_cut_copy_entity())
                                deleteEntity = true;
                        }
                    }
                }
                else
                {
                    ImGui::TextDisabled("Paste");
                }

                ImGui::Separator();

                if (ImGui::Selectable("Duplicate"))
                {
                    auto scene = Application::get().get_current_scene();
                    scene->duplicate_entity({ node, scene });
                }
                if (ImGui::Selectable("Delete"))
                    deleteEntity = true;
                // if(m_Editor->is_selected(node))
                //   m_Editor->UnSelect(node);
                ImGui::Separator();
                if (ImGui::Selectable("Rename"))
                    double_clicked_entity = node;
                ImGui::Separator();

                // if (ImGui::Selectable("Add Child"))
                // {
                //     auto scene = Application::get().get_scene_manager()->get_current_scene();
                //     auto child = scene->create_entity();
                //     child.set_parent({ node, scene });
                // }

                if (ImGui::Selectable("Zoom to"))
                {
                    auto transform = registry.try_get<maths::Transform>(node);
                    if (transform)
                        editor->focus_camera(transform->get_world_position(), 2.0f, 2.0f);
                }
                ImGui::EndPopup();
            }

            if (ImGui::BeginDragDropTarget())
            {
                const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Drag_Entity");
                if (payload)
                {
                    size_t count = payload->DataSize / sizeof(entt::entity);

                    auto currentScene = editor->get_current_scene();
                    for (size_t i = 0; i < count; i++)
                    {
                        entt::entity droppedEntityID = *(((entt::entity*)payload->Data) + i);
                        if (droppedEntityID != node)
                        {
                            auto hierarchyComponent = registry.try_get<Hierarchy>(droppedEntityID);
                            if (hierarchyComponent)
                                Hierarchy::reparent(droppedEntityID, node, registry, *hierarchyComponent);
                            else
                            {
                                registry.emplace<Hierarchy>(droppedEntityID, node);
                            }
                        }
                    }

                    had_recent_dropped_entity = node;
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::IsItemClicked() && !deleteEntity)
                editor->set_selected(node);
            else if (double_clicked_entity == node && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsItemHovered(ImGuiHoveredFlags_None))
                double_clicked_entity = entt::null;

            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered(ImGuiHoveredFlags_None))
            {
                double_clicked_entity = node;
                if (Application::get().get_editor_state() == EditorState::Preview)
                {
                    auto transform = registry.try_get<maths::Transform>(node);
                    if (transform)
                        editor->focus_camera(transform->get_world_position(), 2.0f, 2.0f);
                }
            }

            if (deleteEntity)
            {
                nodeEntity.get_scene()->destroy_entity(nodeEntity);
                if (nodeOpen)
                    ImGui::TreePop();

                ImGuiHelper::PopID();
                return;
            }

            if (select_up)
            {
                if (!editor->get_selected().empty() && editor->get_selected().front() == node && registry.valid(current_previous_entity))
                {
                    select_up = false;
                    editor->set_selected(current_previous_entity);
                }
            }

            if (select_down)
            {
                if (!editor->get_selected().empty() && registry.valid(current_previous_entity) && current_previous_entity == editor->get_selected().front())
                {
                    select_down = false;
                    editor->set_selected(node);
                }
            }

            current_previous_entity = node;

#if 1
            bool showButton = true; // hovered || !active;

            if (showButton)
            {
                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(U8CStr2CStr(ICON_MDI_EYE)).x * 2.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.7f, 0.7f, 0.0f));
                if (ImGui::Button(active ? U8CStr2CStr(ICON_MDI_EYE) : U8CStr2CStr(ICON_MDI_EYE_OFF)))
                {
                    auto& activeComponent = registry.get_or_emplace<ActiveComponent>(node);

                    activeComponent.active = !active;
                }
                ImGui::PopStyleColor();
            }
#endif

            if (nodeOpen == false)
            {
                ImGuiHelper::PopID();
                return;
            }

            const ImColor TreeLineColor = ImColor(128, 128, 128, 128);
            const float SmallOffsetX = 6.0f * Application::get().get_window_dpi();
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            ImVec2 verticalLineStart = ImGui::GetCursorScreenPos();
            verticalLineStart.x += SmallOffsetX; // to nicely line up with the arrow symbol
            ImVec2 verticalLineEnd = verticalLineStart;

            if (!noChildren)
            {
                entt::entity child = hierarchyComponent->first();
                while (child != entt::null && registry.valid(child))
                {
                    float HorizontalTreeLineSize = 20.0f * Application::get().get_window_dpi(); // chosen arbitrarily
                    auto currentPos = ImGui::GetCursorScreenPos();
                    ImGui::Indent(10.0f);

                    auto childHerarchyComponent = registry.try_get<Hierarchy>(child);

                    if (childHerarchyComponent)
                    {
                        entt::entity firstChild = childHerarchyComponent->first();
                        if (firstChild != entt::null && registry.valid(firstChild))
                        {
                            HorizontalTreeLineSize *= 0.4f;
                        }
                    }
                    draw_node(child, registry);
                    ImGui::Unindent(10.0f);

                    const ImRect childRect = ImRect(currentPos, currentPos + ImVec2(0.0f, ImGui::GetFontSize() + (ImGui::GetStyle().FramePadding.y * 2)));

                    const float midpoint = (childRect.Min.y + childRect.Max.y) * 0.5f;
                    drawList->AddLine(ImVec2(verticalLineStart.x, midpoint), ImVec2(verticalLineStart.x + HorizontalTreeLineSize, midpoint), TreeLineColor);
                    verticalLineEnd.y = midpoint;

                    if (registry.valid(child))
                    {
                        auto hierarchyComponent = registry.try_get<Hierarchy>(child);
                        child = hierarchyComponent ? hierarchyComponent->next() : entt::null;
                    }
                }
            }

            drawList->AddLine(verticalLineStart, verticalLineEnd, TreeLineColor);

            ImGui::TreePop();
            ImGuiHelper::PopID();
        }
    }

    void HierarchyPanel::on_imgui_render()
    {
        DS_PROFILE_FUNCTION();
        auto flags = ImGuiWindowFlags_NoCollapse;
        current_previous_entity = entt::null;
        select_up = false;
        select_down = false;

        select_up = Input::get().get_key_pressed(diverse::InputCode::Key::Up);
        select_down = Input::get().get_key_pressed(diverse::InputCode::Key::Down);
        ArenaClear(string_arena);

        if (ImGui::Begin(name.c_str(), &is_active, flags))
        {
            ImRect windowRect = { ImGui::GetWindowContentRegionMin(), ImGui::GetWindowContentRegionMax() };

            auto scene = Application::get().get_current_scene();

            if (!scene)
            {
                ImGui::End();
                return;
            }
            auto& registry = scene->get_registry();
            auto AddEntity = [scene, this]()
            {
                if (ImGui::Selectable("Add Empty Entity"))
                {
                    scene->create_entity();
                }
                // if (ImGui::BeginMenu("Light"))
                // {

                //     if (ImGui::MenuItem("PointLight"))
                //     {
                //         auto entity = scene->create_entity("PointLight");
                //         entity.add_component<diverse::PointLightComponent>();
                //         editor->clear_selected();
                //         editor->set_selected(entity);
                //     }
                //     if (ImGui::MenuItem("SpotLight"))
                //     {
                //         auto entity = scene->create_entity("SpotLight");
                //         entity.add_component<diverse::SpotLightComponent>();
                //         editor->clear_selected();
                //         editor->set_selected(entity);
                //     }
                //     if (ImGui::MenuItem("RectLight"))
                //     {
                //         auto entity = scene->create_entity("RectLight");
                //         entity.add_component<diverse::RectLightComponent>();
                //         editor->clear_selected();
                //         editor->set_selected(entity);
                //     }
                //     if (ImGui::MenuItem("DiskLight"))
                //     {
                //         auto entity = scene->create_entity("DiskLight");
                //         entity.add_component<diverse::DiskLightComponent>();
                //         editor->clear_selected();
                //         editor->set_selected(entity);
                //     }
                //     if (ImGui::MenuItem("CylinderLight"))
                //     {
                //         auto entity = scene->create_entity("CylinderLight");
                //         entity.add_component<diverse::CylinderLightComponent>();
                //         editor->clear_selected();
                //         editor->set_selected(entity);
                //     }
                //     ImGui::EndMenu();
                // }

                if (ImGui::BeginMenu("Primitive"))
                {

                    if (ImGui::MenuItem("Cube"))
                    {
                        auto entity = scene->create_entity("Cube");
                        entity.add_component<diverse::MeshModelComponent>(diverse::PrimitiveType::Cube);
                        editor->clear_selected();
                        editor->set_selected(entity);
                    }
                    if (ImGui::MenuItem("Sphere"))
                    {
                        auto entity = scene->create_entity("Sphere");
                        entity.add_component<diverse::MeshModelComponent>(diverse::PrimitiveType::Sphere);
                        editor->clear_selected();
                        editor->set_selected(entity);
                    }
                    ImGui::EndMenu();
                }
                // if (ImGui::Selectable("Add Camera"))
                // {
                //     auto entity = scene->create_entity("Camera");
                //     entity.add_component<Camera>();
                //     entity.get_or_add_component<maths::Transform>();
                // }

            };

            ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImGui::GetStyleColorVec4(ImGuiCol_TabActive));

            if (ImGui::Button(U8CStr2CStr(ICON_MDI_PLUS)))
            {
                // Add Entity Menu
                ImGui::OpenPopup("AddEntity");
            }

            if (ImGui::BeginPopup("AddEntity"))
            {
                AddEntity();
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            ImGui::TextUnformatted(U8CStr2CStr(ICON_MDI_MAGNIFY));
            ImGui::SameLine();


            {
                ImGuiHelper::ScopedFont boldFont(ImGui::GetIO().Fonts->Fonts[1]);
                ImGuiHelper::ScopedStyle frameBorder(ImGuiStyleVar_FrameBorderSize, 0.0f);
                ImGuiHelper::ScopedColour frameColour(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
                hierarchy_filter.Draw("##HierarchyFilter", ImGui::GetContentRegionAvail().x - ImGui::GetStyle().IndentSpacing);
                ImGuiHelper::DrawItemActivityOutline(2.0f, false);
            }

            if (!hierarchy_filter.IsActive())
            {
                ImGui::SameLine();
                ImGuiHelper::ScopedFont boldFont(ImGui::GetIO().Fonts->Fonts[1]);
                ImGui::SetCursorPosX(ImGui::GetFontSize() * 4.0f);
                ImGuiHelper::ScopedStyle padding(ImGuiStyleVar_FramePadding, ImVec2(0.0f, ImGui::GetStyle().FramePadding.y));
                ImGui::TextUnformatted("Search...");
            }

            ImGui::PopStyleColor();
            ImGui::Unindent();

            // Right click popup
            if (ImGui::BeginPopupContextWindow())
            {
                if (!editor->get_copied_entity().empty() && registry.valid(editor->get_copied_entity().front()))
                {
                    if (ImGui::Selectable("Paste"))
                    {
                        for (auto entity : editor->get_copied_entity())
                        {
                            auto scene = Application::get().get_scene_manager()->get_current_scene();
                            Entity copiedEntity = { entity, scene };
                            scene->duplicate_entity(copiedEntity);

                            if (editor->get_cut_copy_entity())
                            {
                                copiedEntity.get_scene()->destroy_entity(copiedEntity);
                            }
                        }
                    }
                }
                else
                {
                    ImGui::TextDisabled("Paste");
                }

                ImGui::Separator();

                AddEntity();

                ImGui::EndPopup();
            }

            {
                ImGui::Indent();

                for (auto [entity] : registry.storage<entt::entity>().each())
                {
                    if (registry.valid(entity))
                    {
                        auto hierarchyComponent = registry.try_get<Hierarchy>(entity);
                        auto keyframetimeline = registry.try_get<KeyFrameTimeLine>(entity);
                        auto splatedit = registry.try_get<GaussianEdit>(entity);
                        if ((!splatedit) && (!keyframetimeline) && (!hierarchyComponent || hierarchyComponent->parent() == entt::null) )
                            draw_node(entity, registry);
                    }
                }

                // Only supports one scene
                ImVec2 min_space = ImGui::GetWindowContentRegionMin();
                ImVec2 max_space = ImGui::GetWindowContentRegionMax();

                float yOffset = std::max(45.0f, ImGui::GetScrollY()); // Dont include search bar
                min_space.x += ImGui::GetWindowPos().x + 1.0f;
                min_space.y += ImGui::GetWindowPos().y + 1.0f + yOffset;
                max_space.x += ImGui::GetWindowPos().x - 1.0f;
                max_space.y += ImGui::GetWindowPos().y - 1.0f + ImGui::GetScrollY();
                ImRect bb{ min_space, max_space };

                if (ImGui::BeginDragDropTargetCustom(windowRect, ImGui::GetCurrentWindow()->ID))
                {
                    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Drag_Entity", ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
                    if (payload)
                    {
                        size_t count = payload->DataSize / sizeof(entt::entity);

                        auto currentScene = editor->get_current_scene();
                        for (size_t i = 0; i < count; i++)
                        {
                            entt::entity droppedEntityID = *(((entt::entity*)payload->Data) + i);
                            auto hierarchyComponent = registry.try_get<Hierarchy>(droppedEntityID);
                            if (hierarchyComponent)
                            {
                                Hierarchy::reparent(droppedEntityID, entt::null, registry, *hierarchyComponent);
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
                
                if (ImGui::IsWindowFocused() && Input::get().get_key_pressed(diverse::InputCode::Key::Delete))
                {
                    auto* scene = Application::get().get_current_scene();
                    for (auto entity : editor->get_selected())
                        scene->destroy_entity(Entity(entity, scene));
                }
                if (ImGui::IsWindowFocused() && Input::get().get_key_held(diverse::InputCode::Key::LeftControl))
                {
                    auto* scene = Application::get().get_current_scene();
                    if(Input::get().get_key_pressed(diverse::InputCode::Key::C))
                    { 
                        for (auto entity : editor->get_selected())
                            editor->set_copied_entity(entity);
                    }
                    if (Input::get().get_key_pressed(diverse::InputCode::Key::X))
                    {
                        for (auto entity : editor->get_selected())
                            editor->set_copied_entity(entity,true);
                    }
                    if (Input::get().get_key_pressed(diverse::InputCode::Key::V))
                    {
                        for (auto entity : editor->get_copied_entity())
                        {
                            Entity copiedEntity = { entity, scene };
                            scene->duplicate_entity(copiedEntity);

                            if (editor->get_cut_copy_entity())
                                Entity(entity, scene).destroy();
                        }
                    }
                }
            }

            ImGui::End();
        }
    }


}