#pragma once

#include "IconsMaterialDesignIcons.h"
#include <set>
#include <map>

#include <entt/entt.hpp>
#include <imgui.h>

namespace MM
{

    template <class Component, class EntityType>
    void ComponentEditorWidget(entt::basic_registry<EntityType>& registry, EntityType entity) { }

    template <class Component, class EntityType>
    void ComponentAddAction(entt::basic_registry<EntityType>& registry, EntityType entity)
    {
        registry.template emplace<Component>(entity);
    }

    template <class Component, class EntityType>
    void ComponentRemoveAction(entt::basic_registry<EntityType>& registry, EntityType entity)
    {
        registry.template remove<Component>(entity);
    }

    template <typename EntityType>
    class ImGuiEntityEditor
    {
    private:
        using Registry = entt::basic_registry<EntityType>;

        struct ComponentInfo
        {
            using Callback = std::function<void(Registry&, EntityType)>;
            std::string name;
            Callback widget, create, destroy;
        };

    private:
        using ComponentTypeID = entt::id_type;
        ImGuiTextFilter m_ComponentFilter;

        std::map<ComponentTypeID, ComponentInfo> component_infos;

        bool entityHasComponent(Registry& registry, EntityType& entity, ComponentTypeID type_id)
        {
            const auto* storage_ptr = registry.storage(type_id);
            return storage_ptr != nullptr && storage_ptr->contains(entity);
        }

    public:
        template <class Component>
        ComponentInfo& registerComponent(const ComponentInfo& component_info)
        {
            auto index = entt::type_hash<Component>::value();
            auto insert_info = component_infos.insert_or_assign(index, component_info);
            assert(insert_info.second);
            return std::get<ComponentInfo>(*insert_info.first);
        }

        template <class Component>
        ComponentInfo& registerComponent(const std::string& name, typename ComponentInfo::Callback widget)
        {
            return registerComponent<Component>(ComponentInfo{
                name,
                widget,
                ComponentAddAction<Component, EntityType>,
                ComponentRemoveAction<Component, EntityType>,
                });
        }

        template <class Component>
        ComponentInfo& registerComponent(const std::string& name)
        {
            return registerComponent<Component>(name, ComponentEditorWidget<Component, EntityType>);
        }

        // calls all the ImGui functions
        // call this every frame
        void RenderImGui(Registry& registry, typename Registry::entity_type& e)
        {
            if (e != entt::null)
            {
                static std::unordered_map<ComponentTypeID, ComponentInfo> has_not;
                static bool addComponentPopupOpen = false;
                static bool initialised = false;
                if (addComponentPopupOpen)
                {
                    if (!initialised)
                    {
                        has_not.reserve(64);
                        initialised = true;
                    }
                    else
                        has_not.clear();
                }

                for (auto& [component_type_id, ci] : component_infos)
                {
                    if (entityHasComponent(registry, e, component_type_id))
                    {
                        ImGui::PushID(component_type_id);

                        const std::string& label = ci.name;

                        bool open = ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_DefaultOpen);

                        bool removed = false;

                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::GetFontSize() - ImGui::GetStyle().ItemSpacing.x);

                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.7f, 0.7f, 0.0f));

                        static std::string removeName = "Remove Component";
                        if (ImGui::Button(U8CStr2CStr(ICON_MDI_TUNE "##RemoveButton")))
                            ImGui::OpenPopup(removeName.c_str());
                        ImGui::PopStyleColor();

                        if (ImGui::BeginPopup(removeName.c_str(), 3))
                        {
                            if (ImGui::Selectable("Remove"))
                            {
                                ci.destroy(registry, e);
                                removed = true;
                            }
                            ImGui::EndPopup();
                            if (removed)
                            {
                                ImGui::PopID();
                                continue;
                            }
                        }
                        if (open)
                        {
                            ImGui::PushID("Widget");
                            ci.widget(registry, e);
                            ImGui::PopID();
                        }
                        ImGui::PopID();
                    }
                    else
                    {
                        if (addComponentPopupOpen)
                            has_not[component_type_id] = ci;
                    }
                }

                // if(!has_not.empty())
                {
                    if (ImGui::Button(U8CStr2CStr(ICON_MDI_PLUS_BOX_OUTLINE " Add Component"), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                    {
                        ImGui::OpenPopup("addComponent");
                    }

                    if (ImGui::BeginPopup("addComponent"))
                    {
                        addComponentPopupOpen = true;
                        ImGui::Separator();

                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted(U8CStr2CStr(ICON_MDI_MAGNIFY));
                        ImGui::SameLine();

                        float filterSize = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().IndentSpacing;
                        filterSize = filterSize < 200 ? 200 : filterSize;
                        m_ComponentFilter.Draw("##ComponentFilter", filterSize);

                        for (auto& [component_type_id, ci] : has_not)
                        {
                            bool show = true;
                            if (m_ComponentFilter.IsActive())
                            {
                                if (!m_ComponentFilter.PassFilter(ci.name.c_str()))
                                {
                                    show = false;
                                }
                            }
                            if (show)
                            {
                                ImGui::PushID(component_type_id);
                                if (ImGui::Selectable(ci.name.c_str()))
                                {
                                    ci.create(registry, e);
                                }
                                ImGui::PopID();
                            }
                        }

                        ImGui::EndPopup();
                    }
                    else
                        addComponentPopupOpen = false;
                }
            }
        }
    };

} // MM

