#include "precompile.h"
#include "hierarchy_system.h"

#include "../components/parent_component.h"
#include "../components/children_component.h"
#include "core/ds_log.h"

DISABLE_WARNING_PUSH
DISABLE_WARNING_CONVERSION_TO_SMALLER_TYPE
#include <entt/entt.hpp>
DISABLE_WARNING_POP

namespace diverse
{

    void HierarchySystem::init(entt::registry& registry)
    {
        registry.on_construct<Parent>().connect<&HierarchySystem::on_parent_construct>();
        registry.on_destroy<Parent>().connect<&HierarchySystem::on_parent_destroy>();
        // Note: Generic entity destruction is handled through component callbacks
        // No need for separate on_destroy() callback in this design
    }

    void HierarchySystem::shutdown(entt::registry& registry)
    {
        registry.on_construct<Parent>().disconnect<&HierarchySystem::on_parent_construct>();
        registry.on_destroy<Parent>().disconnect<&HierarchySystem::on_parent_destroy>();
    }

    void HierarchySystem::reparent(entt::registry& registry, entt::entity entity, entt::entity new_parent)
    {
        if (entity == entt::null || !registry.valid(entity))
            return;

        // Prevent circular references
        if (new_parent != entt::null && registry.valid(new_parent))
        {
            entt::entity current = new_parent;
            while (current != entt::null)
            {
                if (current == entity)
                {
                    DS_LOG_WARN("Cannot reparent: would create circular reference");
                    return;
                }
                if (registry.all_of<Parent>(current))
                {
                    current = registry.get<Parent>(current).value;
                }
                else
                {
                    break;
                }
            }
        }

        // Remove from old parent
        if (registry.all_of<Parent>(entity))
        {
            auto& parent_component = registry.get<Parent>(entity);
            entt::entity old_parent = parent_component.value;

            if (old_parent != entt::null && registry.valid(old_parent))
            {
                remove_child_from_parent(registry, entity, old_parent);
            }

            registry.remove<Parent>(entity);
        }

        // Add to new parent
        if (new_parent != entt::null && registry.valid(new_parent))
        {
            registry.emplace<Parent>(entity, new_parent);
            add_child_to_parent(registry, entity, new_parent);
        }
    }

    void HierarchySystem::on_parent_construct(entt::registry& registry, entt::entity entity)
    {
        auto& parent_component = registry.get<Parent>(entity);
        entt::entity parent = parent_component.value;

        if (parent != entt::null && registry.valid(parent))
        {
            add_child_to_parent(registry, entity, parent);
        }
    }

    void HierarchySystem::on_parent_destroy(entt::registry& registry, entt::entity entity)
    {
        if (!registry.all_of<Parent>(entity))
            return;

        auto& parent_component = registry.get<Parent>(entity);
        entt::entity parent = parent_component.value;

        if (parent != entt::null && registry.valid(parent))
        {
            remove_child_from_parent(registry, entity, parent);
        }
    }

    void HierarchySystem::add_child_to_parent(entt::registry& registry, entt::entity child, entt::entity parent)
    {
        auto& children = registry.get_or_emplace<Children>(parent);
        children.push(child);
    }

    void HierarchySystem::remove_child_from_parent(entt::registry& registry, entt::entity child, entt::entity parent)
    {
        if (!registry.all_of<Children>(parent))
            return;

        auto& children = registry.get<Children>(parent);
        children.remove(child);

        // Remove Children component if empty
        if (children.is_empty())
        {
            registry.remove<Children>(parent);
        }
    }

}
