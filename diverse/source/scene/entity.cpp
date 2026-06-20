#include "core/reference.h"
#include "core/ds_log.h"
#include "core/core.h"
#include "entity.h"
#include "scene/components/parent_component.h"
#include "scene/components/children_component.h"
#include "scene/scene_graph.h"
#include "systems/hierarchy_system.h"
#include "engine/application.h"

namespace diverse
{

    Entity* Entity::get_children_temp()
    {
        return get_children(Application::get().get_frame_arena());
    }

    u32 Entity::get_child_count()
    {
        auto& registry = scene->get_registry();
        if (registry.all_of<Children>(entity_handle))
        {
            auto& children = registry.get<Children>(entity_handle);
            return static_cast<u32>(children.size());
        }
        return 0;
    }

    bool Entity::is_parent(Entity potentialParent)
    {
        DS_PROFILE_FUNCTION_LOW();
        auto& registry = scene->get_registry();
        entt::entity current = entity_handle;

        while (current != entt::null && registry.valid(current))
        {
            if (registry.all_of<Parent>(current))
            {
                auto& parent_comp = registry.get<Parent>(current);
                if (parent_comp.value == potentialParent.entity_handle)
                {
                    return true;
                }
                current = parent_comp.value;
            }
            else
            {
                break;
            }
        }
        return false;
    }

    std::vector<Entity> Entity::get_children()
    {
        DS_PROFILE_FUNCTION_LOW();
        std::vector<Entity> children;
        auto& registry = scene->get_registry();
        if (registry.all_of<Children>(entity_handle))
        {
            auto& children_comp = registry.get<Children>(entity_handle);
            for (auto child : children_comp)
            {
                if (registry.valid(child))
                {
                    children.emplace_back(child, scene);
                }
            }
        }
        return children;
    }

    void Entity::clear_children()
    {
        DS_PROFILE_FUNCTION_LOW();
        auto& registry = scene->get_registry();
        if (registry.all_of<Children>(entity_handle))
        {
            // Destroy all children
            auto& children_comp = registry.get<Children>(entity_handle);
            std::vector<entt::entity> to_destroy;
            for (auto child : children_comp)
            {
                if (registry.valid(child))
                {
                    to_destroy.push_back(child);
                }
            }
            for (auto child : to_destroy)
            {
                registry.destroy(child);
            }
        }
    }

    Entity* Entity::get_children(Arena* arena)
    {
        DS_PROFILE_FUNCTION_LOW();

        Entity* children = nullptr;
        u32 childIndex = 0;

        auto& registry = scene->get_registry();
        if (registry.all_of<Children>(entity_handle))
        {
            auto& children_comp = registry.get<Children>(entity_handle);
            u32 childCount = static_cast<u32>(children_comp.size());

            children = PushArrayNoZero(arena, Entity, childCount);
            for (auto child : children_comp)
            {
                if (registry.valid(child))
                {
                    children[childIndex] = { child, scene };
                    childIndex++;
                }
            }
        }

        return children;
    }

}
