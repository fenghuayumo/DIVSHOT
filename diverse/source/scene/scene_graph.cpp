#include "precompile.h"
#include "scene_graph.h"
#include "maths/transform.h"

DISABLE_WARNING_PUSH
DISABLE_WARNING_CONVERSION_TO_SMALLER_TYPE
#include <entt/entt.hpp>
DISABLE_WARNING_POP

namespace diverse
{
    Hierarchy::Hierarchy(entt::entity p)
        : m_Parent(p)
    {
        m_First = entt::null;
        m_Next  = entt::null;
        m_Prev  = entt::null;
    }

    Hierarchy::Hierarchy()
    {
        m_Parent = entt::null;
        m_First  = entt::null;
        m_Next   = entt::null;
        m_Prev   = entt::null;
    }

    SceneGraph::SceneGraph()
    {
    }

    void SceneGraph::init(entt::registry& registry)
    {
        registry.on_construct<Hierarchy>().connect<&Hierarchy::on_construct>();
        registry.on_update<Hierarchy>().connect<&Hierarchy::on_update>();
        registry.on_destroy<Hierarchy>().connect<&Hierarchy::on_destroy>();
    }

    void SceneGraph::update(entt::registry& registry)
    {
        DS_PROFILE_FUNCTION();
        auto nonHierarchyView = registry.view<maths::Transform>(entt::exclude<Hierarchy>);

        for(auto entity : nonHierarchyView)
        {
            registry.get<maths::Transform>(entity).set_world_matrix(glm::mat4(1.0f));
        }

        auto view = registry.view<Hierarchy>();
        for(auto entity : view)
        {
            const auto hierarchy = registry.try_get<Hierarchy>(entity);
            if(hierarchy && hierarchy->parent() == entt::null)
            {
                // Recursively update children
                update_transform(entity, registry);
            }
        }
    }

    void SceneGraph::update_transform(entt::entity entity, entt::registry& registry)
    {
        DS_PROFILE_FUNCTION();
        auto hierarchyComponent = registry.try_get<Hierarchy>(entity);
        if(hierarchyComponent)
        {
            auto transform = registry.try_get<maths::Transform>(entity);
            if(transform)
            {
                if(hierarchyComponent->parent() != entt::null)
                {
                    auto parentTransform = registry.try_get<maths::Transform>(hierarchyComponent->parent());
                    if(parentTransform)
                    {
                        transform->set_world_matrix(parentTransform->get_world_matrix());
                    }
                    else
                    {
                        transform->set_world_matrix(glm::mat4(1.0f));
                    }
                }
                else
                {
                    transform->set_world_matrix(glm::mat4(1.0f));
                }
            }

            entt::entity child = hierarchyComponent->first();
            while(child != entt::null)
            {
                auto hierarchyComponent = registry.try_get<Hierarchy>(child);
                auto next               = hierarchyComponent ? hierarchyComponent->next() : entt::null;
                update_transform(child, registry);
                child = next;
            }
        }
    }

    void Hierarchy::reparent(entt::entity entity, entt::entity parent, entt::registry& registry, Hierarchy& hierarchy)
    {
        DS_PROFILE_FUNCTION();
        Hierarchy::on_destroy(registry, entity);

        hierarchy.m_Parent = entt::null;
        hierarchy.m_Next   = entt::null;
        hierarchy.m_Prev   = entt::null;

        if(parent != entt::null)
        {
            hierarchy.m_Parent = parent;
            Hierarchy::on_construct(registry, entity);
        }
    }

    bool Hierarchy::compare(const entt::registry& registry, const entt::entity rhs) const
    {
        DS_PROFILE_FUNCTION();
        if(rhs == entt::null || rhs == m_Parent || rhs == m_Prev)
        {
            return true;
        }
        else
        {
            if(m_Parent == entt::null)
            {
                return false;
            }
            else
            {
                auto& this_parent_h = registry.get<Hierarchy>(m_Parent);
                auto& rhs_h         = registry.get<Hierarchy>(rhs);
                if(this_parent_h.compare(registry, rhs_h.m_Parent))
                {
                    return true;
                }
            }
        }
        return false;
    }

    void Hierarchy::reset()
    {
        m_Parent = entt::null;
        m_First  = entt::null;
        m_Next   = entt::null;
        m_Prev   = entt::null;
    }

    void Hierarchy::on_construct(entt::registry& registry, entt::entity entity)
    {
        DS_PROFILE_FUNCTION();
        auto& hierarchy = registry.get<Hierarchy>(entity);
        if(hierarchy.m_Parent != entt::null)
        {
            auto& parent_hierarchy = registry.get_or_emplace<Hierarchy>(hierarchy.m_Parent);

            if(parent_hierarchy.m_First == entt::null)
            {
                parent_hierarchy.m_First = entity;
            }
            else
            {
                // get last children
                auto prev_ent          = parent_hierarchy.m_First;
                auto current_hierarchy = registry.try_get<Hierarchy>(prev_ent);
                while(current_hierarchy != nullptr && current_hierarchy->m_Next != entt::null)
                {
                    prev_ent          = current_hierarchy->m_Next;
                    current_hierarchy = registry.try_get<Hierarchy>(prev_ent);
                }
                // add new
                current_hierarchy->m_Next = entity;
                hierarchy.m_Prev          = prev_ent;
            }
            parent_hierarchy.m_ChildCount++;
        }
    }

    void delete_children(entt::entity parent, entt::registry& registry)
    {
        DS_PROFILE_FUNCTION();
        auto hierarchy = registry.try_get<Hierarchy>(parent);

        if(hierarchy)
        {
            entt::entity child = hierarchy->first();
            while(child != entt::null)
            {
                delete_children(child, registry);
                hierarchy = registry.try_get<Hierarchy>(child);
                registry.destroy(child);

                if(hierarchy)
                {
                    child = hierarchy->next();
                }
            }
            hierarchy->m_ChildCount = 0;
        }
    }

    void Hierarchy::on_update(entt::registry& registry, entt::entity entity)
    {
        DS_PROFILE_FUNCTION();
        auto& hierarchy = registry.get<Hierarchy>(entity);
        // if is the first child
        if(hierarchy.m_Prev == entt::null)
        {
            if(hierarchy.m_Parent != entt::null)
            {
                auto parent_hierarchy = registry.try_get<Hierarchy>(hierarchy.m_Parent);
                if(parent_hierarchy != nullptr)
                {
                    parent_hierarchy->m_First = hierarchy.m_Next;
                    if(hierarchy.m_Next != entt::null)
                    {
                        auto next_hierarchy = registry.try_get<Hierarchy>(hierarchy.m_Next);
                        if(next_hierarchy != nullptr)
                        {
                            next_hierarchy->m_Prev = entt::null;
                        }
                    }
                }
            }
        }
        else
        {
            auto prev_hierarchy = registry.try_get<Hierarchy>(hierarchy.m_Prev);
            if(prev_hierarchy != nullptr)
            {
                prev_hierarchy->m_Next = hierarchy.m_Next;
            }
            if(hierarchy.m_Next != entt::null)
            {
                auto next_hierarchy = registry.try_get<Hierarchy>(hierarchy.m_Next);
                if(next_hierarchy != nullptr)
                {
                    next_hierarchy->m_Prev = hierarchy.m_Prev;
                }
            }
        }

        // sort
        //		registry.sort<Hierarchy>([&registry](const entt::entity lhs, const entt::entity rhs)
        //		{
        //			auto& right_h = registry.get<Hierarchy>(rhs);
        //			return right_h.Compare(registry, lhs);
        //		});
    }

    void Hierarchy::on_destroy(entt::registry& registry, entt::entity entity)
    {
        DS_PROFILE_FUNCTION();
        auto& hierarchy = registry.get<Hierarchy>(entity);
        // if is the first child
        if(hierarchy.m_Prev == entt::null || !registry.valid(hierarchy.m_Prev))
        {
            if(hierarchy.m_Parent != entt::null && registry.valid(hierarchy.m_Parent))
            {
                auto parent_hierarchy = registry.try_get<Hierarchy>(hierarchy.m_Parent);
                if(parent_hierarchy != nullptr)
                {
                    parent_hierarchy->m_First = hierarchy.m_Next;
                    if(hierarchy.m_Next != entt::null)
                    {
                        auto next_hierarchy = registry.try_get<Hierarchy>(hierarchy.m_Next);
                        if(next_hierarchy != nullptr)
                        {
                            next_hierarchy->m_Prev = entt::null;
                        }
                    }
                    parent_hierarchy->m_ChildCount--;
                }
            }
        }
        else
        {
            auto prev_hierarchy = registry.try_get<Hierarchy>(hierarchy.m_Prev);
            if(prev_hierarchy != nullptr)
            {
                prev_hierarchy->m_Next = hierarchy.m_Next;
                prev_hierarchy->m_ChildCount--;
            }
            if(hierarchy.m_Next != entt::null)
            {
                auto next_hierarchy = registry.try_get<Hierarchy>(hierarchy.m_Next);
                if(next_hierarchy != nullptr)
                {
                    next_hierarchy->m_Prev = hierarchy.m_Prev;
                }
            }
        }

        // sort
        //		registry.sort<Hierarchy>([&registry](const entt::entity lhs, const entt::entity rhs)
        //		{
        //			auto& right_h = registry.get<Hierarchy>(rhs);
        //			return right_h.Compare(registry, lhs);
        //		});
    }

    void SceneGraph::disable_on_construct(bool disable, entt::registry& registry)
    {
        DS_PROFILE_FUNCTION();
        if(disable)
            registry.on_construct<Hierarchy>().disconnect<&Hierarchy::on_construct>();
        else
            registry.on_construct<Hierarchy>().connect<&Hierarchy::on_construct>();
    }

}
