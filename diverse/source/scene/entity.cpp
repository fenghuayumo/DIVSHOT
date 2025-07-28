
#include "core/reference.h"
#include "core/ds_log.h"
#include "core/core.h"
#include "entity.h"
#include "maths/transform.h"
#include "scene/scene_graph.h"
#include "engine/application.h"

namespace diverse
{
    Entity* Entity::get_children_temp()
    {
        return get_children(Application::get().get_frame_arena());
    }

    u32 Entity::get_child_count()
    {
        auto hierarchyComponent = try_get_component<Hierarchy>();
        if(hierarchyComponent)
        {
            return hierarchyComponent->m_ChildCount;
        }

        return 0;
    }

    bool Entity::is_parent(Entity potentialParent)
    {
        DS_PROFILE_FUNCTION_LOW();
        auto nodeHierarchyComponent = m_Scene->get_registry().try_get<Hierarchy>(m_EntityHandle);
        if (nodeHierarchyComponent)
        {
            auto parent = nodeHierarchyComponent->parent();
            while (parent != entt::null)
            {
                if (parent == potentialParent.m_EntityHandle)
                {
                    return true;
                }
                else
                {
                    nodeHierarchyComponent = m_Scene->get_registry().try_get<Hierarchy>(parent);
                    parent = nodeHierarchyComponent ? nodeHierarchyComponent->parent() : entt::null;
                }
            }
        }

        return false;
    }

    std::vector<Entity> Entity::get_children()
    {
        DS_PROFILE_FUNCTION_LOW();
        std::vector<Entity> children;
        auto hierarchyComponent = try_get_component<Hierarchy>();
        if (hierarchyComponent)
        {
            entt::entity child = hierarchyComponent->first();
            while (child != entt::null && m_Scene->get_registry().valid(child))
            {
                children.emplace_back(child, m_Scene);
                hierarchyComponent = m_Scene->get_registry().try_get<Hierarchy>(child);
                if (hierarchyComponent)
                    child = hierarchyComponent->next();
            }
        }

        return children;
    }

    void Entity::clear_children()
    {
        DS_PROFILE_FUNCTION_LOW();
        auto hierarchyComponent = try_get_component<Hierarchy>();
        if (hierarchyComponent)
        {
            hierarchyComponent->m_First = entt::null;
        }
    }

    Entity* Entity::get_children(Arena* arena)
    {
        DS_PROFILE_FUNCTION_LOW();

        Entity* children = nullptr;
        u32 childIndex   = 0;

        auto hierarchyComponent = try_get_component<Hierarchy>();
        if(hierarchyComponent)
        {
            u32 childCount = 0;
            // TODO: remove
            {
                entt::entity child = hierarchyComponent->first();
                while(child != entt::null && m_Scene->get_registry().valid(child))
                {
                    childCount++;

                    hierarchyComponent = m_Scene->get_registry().try_get<Hierarchy>(child);
                    if(hierarchyComponent)
                        child = hierarchyComponent->next();
                }
            }

            hierarchyComponent               = try_get_component<Hierarchy>();
            hierarchyComponent->m_ChildCount = childCount;

            children           = PushArrayNoZero(arena, Entity, childCount);
            entt::entity child = hierarchyComponent->first();
            while(child != entt::null && m_Scene->get_registry().valid(child))
            {
                children[childIndex] = { child, m_Scene };
                childIndex++;

                hierarchyComponent = m_Scene->get_registry().try_get<Hierarchy>(child);
                if(hierarchyComponent)
                    child = hierarchyComponent->next();
            }
        }

        return children;
    }
}